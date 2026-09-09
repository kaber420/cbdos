
<script lang="ts">
  import { onMount } from 'svelte';
  import { BOARDS, findBoard } from './boards';
  import { fetchReleasesRaw, buildFirmwareList, formatSize, type FirmwareFile, type GhRelease } from './firmware';

  // --- Estado ---
  let boardId = $state('jc4880p443');
  const board = $derived(findBoard(boardId));

  // Releases crudos (1 sola lectura a GitHub, cacheada) + filtro local por placa.
  let allReleases = $state<GhRelease[]>([]);
  let cacheInfo = $state<string>('');
  const CACHE_KEY = 'cbdos-releases-v1';
  const CACHE_TTL_MS = 30 * 60 * 1000; // 30 min: cambiar de placa NO vuelve a pegar a GitHub

  let files = $state<FirmwareFile[]>([]);
  let loading = $state(false);
  let listError = $state<string | null>(null);
  let selectedIdx = $state(0);
  const selected = $derived(files[selectedIdx] ?? null);

  // Firma instalada (banner CBDos) + chip detectado (ROM bootloader, aun sin CBDos).
  interface Installed { board: string; soc: string; ver: string; raw: string }
  let installed = $state<Installed | null>(null);
  let chipFound = $state<string | null>(null);
  let detecting = $state(false);
  let detectError = $state<string | null>(null);

  // Flasheo
  let flashing = $state(false);
  let flashProgress = $state(0);
  let logLines = $state<string[]>([]);
  let done = $state<string | null>(null);
  let flashError = $state<string | null>(null);

  const serialOK = typeof navigator !== 'undefined' && 'serial' in navigator;
  const BANNER_RE = /CBDOS:BOARD=(\S+)\s+SOC=(\S+)\s+VER=(\S+)/;

  function log(msg: string) {
    const t = new Date().toLocaleTimeString('es-MX', { hour12: false });
    logLines = [...logLines.slice(-200), `[${t}] ${msg}`];
  }

  // Solo el botón "Recargar" pega a GitHub. Cambiar de placa filtra en local.
  async function applyFilter() {
    files = await buildFirmwareList(allReleases, board);
    selectedIdx = 0;
    if (files.length === 0 && !listError) {
      listError = `Aún no hay firmwares publicados para ${board.boardId} (${board.soc}). Revisa que el Release contenga assets cbdos-${board.boardId}-${board.soc}-*-merged.bin.`;
    }
  }

  async function refreshReleases(force = false) {
    if (loading) return;
    loading = true;
    listError = null;
    try {
      if (!force) {
        try {
          const cached = JSON.parse(localStorage.getItem(CACHE_KEY) ?? 'null');
          if (cached && Date.now() - cached.ts < CACHE_TTL_MS) {
            allReleases = cached.data;
            const min = Math.max(1, Math.round((Date.now() - cached.ts) / 60000));
            cacheInfo = `Cache de hace ${min} min — el botón Recargar fuerza lectura nueva.`;
            log(`Releases desde cache (${allReleases.length}), cero peticiones a GitHub.`);
            await applyFilter();
            return;
          }
        } catch { /* cache inválida: sigue a red */ }
      }
      log('Leyendo Releases de GitHub (1 sola petición)…');
      const res = await fetchReleasesRaw();
      if (res.error) {
        listError = res.error;
        files = [];
        log(res.error);
        return;
      }
      allReleases = res.releases;
      try {
        localStorage.setItem(CACHE_KEY, JSON.stringify({ ts: Date.now(), data: res.releases }));
      } catch { /* almacenamiento lleno/bloqueado: no pasa nada */ }
      cacheInfo = 'Actualizado ahora mismo desde GitHub.';
      log(`OK: ${allReleases.length} release(s). Filtrando en local para ${board.short}…`);
      await applyFilter();
      if (!listError) log(`${files.length} firmware(s) para ${board.short}.`);
    } finally {
      loading = false;
    }
  }

  onMount(() => { refreshReleases(false); });

  function selectBoard(id: string) {
    if (id === boardId) return;
    boardId = id;
    selectedIdx = 0;
    installed = null;
    done = null;
    applyFilter(); // local, cero red
  }

  function pick(i: number) {
    selectedIdx = i;
    done = null;
    flashError = null;
  }

  // --- Detección: banner CBDos (con auto-reset) + chip vía ROM (placa nueva) ---
  const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));

  /** Pulso de reset por DTR/RTS: reinicia la placa a la app para que imprima el banner. */
  async function pulseReset(port: any) {
    try {
      await port.setSignals({ dataTerminalReady: false, requestToSend: true });
      await sleep(150);
      await port.setSignals({ dataTerminalReady: false, requestToSend: false });
      await sleep(400);
      log('Reset por serial enviado.');
    } catch {
      log('Reset por serial no soportado; pulsa el botón RST de la placa.');
    }
  }

  /** Pregunta al ROM bootloader qué chip es (funciona con flash vacía/sin CBDos). */
  async function detectChip(port: any): Promise<string | null> {
    let transport: any = null;
    try {
      const { Transport, ESPLoader } = await import('esptool-js');
      transport = new Transport(port, true);
      const quiet = { clean: () => {}, writeLine: () => {}, write: () => {} };
      const loader = new ESPLoader({ transport, baudrate: 115200, terminal: quiet, enableTracing: false });
      const chip = await loader.main();
      await loader.after('hard_reset'); // devuelve la placa a modo normal
      return chip;
    } catch (e) {
      log(`Sin respuesta del bootloader: ${e}`);
      return null;
    } finally {
      try { await transport?.disconnect(); } catch { /* noop */ }
    }
  }

  async function detectInstalled() {
    if (!serialOK) return;
    detecting = true;
    detectError = null;
    installed = null;
    chipFound = null;
    let port: any = null;
    try {
      port = await (navigator as any).serial.requestPort();
      await port.open({ baudRate: 115200 });
      await pulseReset(port); // la placa rebooteará e imprimirá el banner si trae CBDos
      log('Puerto abierto @115200, escuchando banner…');
      const decoder = new TextDecoder();
      let buf = '';
      let queried = false;
      const start = Date.now();

      const writer = port.writable?.getWriter();
      const reader = port.readable.getReader();

      const timeout = setTimeout(async () => {
        if (!queried && writer) {
          queried = true;
          try {
            await writer.write(new TextEncoder().encode('CBDOS:VERSION?\n'));
            log('→ CBDOS:VERSION? enviado, esperando respuesta…');
          } catch { /* noop */ }
        }
      }, 2500);

      try {
        while (Date.now() - start < 9000) {
          const { value, done: rdDone } = await Promise.race([
            reader.read(),
            new Promise<{ value: undefined; done: boolean }>((res) =>
              setTimeout(() => res({ value: undefined, done: false }), 500),
            ),
          ]);
          if (value) {
            buf += decoder.decode(value, { stream: true });
            const m = buf.match(BANNER_RE);
            if (m) {
              installed = { board: m[1], soc: m[2], ver: m[3], raw: m[0] };
              log(`Instalado detectado: ${m[0]}`);
              break;
            }
          }
          if (rdDone) break;
        }
      } finally {
        clearTimeout(timeout);
        try { reader.releaseLock(); } catch { /* noop */ }
        try { writer?.releaseLock(); } catch { /* noop */ }
      }
      try { await port.close(); } catch { /* noop */ }

      if (!installed) {
        // Plan B: ¿al menos qué chip hay? (placa nueva o sin CBDos)
        log('Sin banner CBDos. Preguntando al bootloader qué chip es…');
        const chip = await detectChip(port);
        try { await port.close(); } catch { /* noop */ }
        if (chip) {
          chipFound = chip;
          log(`Chip detectado: ${chip} (sin CBDos instalado o versión sin banner).`);
          // Preselección de placa según chip
          const up = chip.toUpperCase();
          if (up.includes('P4')) selectBoard('jc4880p443');
          else if (up.includes('S3')) selectBoard('jc3248w535');
        } else {
          detectError = 'No responde ni CBDos ni el bootloader. Revisa cable (debe ser de datos), puerto USB-Serial/JTAG y alimentación.';
        }
      }
    } catch (e: any) {
      if (e?.name !== 'NotFoundError') {
        detectError = `No se pudo abrir el puerto: ${e?.message ?? e}`;
      }
    } finally {
      try { await port?.close(); } catch { /* noop */ }
      detecting = false;
    }
  }

  function versionHint(): string | null {
    if (!installed || !selected) return null;
    if (installed.ver === selected.version) return 'Misma versión que la instalada.';
    return installed.ver < selected.version
      ? `Actualización: ${installed.ver} → ${selected.version}.`
      : `Downgrade: ${installed.ver} → ${selected.version}.`;
  }

  // --- Flasheo real con esptool-js (merged-bin @0x0) ---
  async function flashSelected() {
    if (!selected || flashing) return;
    if (!serialOK) {
      flashError = 'Tu navegador no soporta WebSerial. Usa Chrome/Edge en HTTPS o localhost.';
      return;
    }
    flashing = true;
    flashProgress = 0;
    flashError = null;
    done = null;
    let transport: any = null;
    let port: any = null;
    try {
      log(`Descargando ${selected.fileName}…`);
      const resp = await fetch(selected.downloadUrl);
      if (!resp.ok) throw new Error(`Descarga HTTP ${resp.status}`);
      const bin = new Uint8Array(await resp.arrayBuffer());
      log(`Binario: ${(bin.length / 1024 / 1024).toFixed(2)} MB. Conecta la placa en modo download si el auto-reset falla.`);

      const { Transport, ESPLoader } = await import('esptool-js');
      port = await (navigator as any).serial.requestPort();
      transport = new Transport(port, true);
      const terminal = {
        clean: () => {},
        writeLine: (d: string) => log(d.replace(/\x1b\[[0-9;]*m/g, '')),
        write: (d: string) => log(d.replace(/\x1b\[[0-9;]*m/g, '')),
      };
      const loader = new ESPLoader({
        transport,
        baudrate: board.baud,
        terminal,
        enableTracing: false,
      });
      log(`Conectando (${board.soc} @${board.baud})…`);
      const chip = await loader.main();
      log(`Chip: ${chip}. Escribiendo merged @0x0…`);
      await loader.writeFlash({
        fileArray: [{ data: bin, address: 0 }],
        flashSize: 'keep',
        flashMode: 'keep',
        flashFreq: 'keep',
        eraseAll: false,
        compress: true,
        reportProgress: (_fi: number, written: number, total: number) => {
          flashProgress = Math.round((written / total) * 100);
        },
      });
      log('Verificado. Reiniciando…');
      await loader.after('hard_reset');
      done = `${selected.fileName} flasheado al 100 %. La placa reinicia con CBDos ${selected.version}.`;
      log(done);
    } catch (e: any) {
      if (e?.name === 'NotFoundError') {
        log('Selección de puerto cancelada.');
      } else {
        flashError = `Falló el flasheo: ${e?.message ?? e}`;
        log(flashError);
      }
    } finally {
      try { await transport?.disconnect(); } catch { /* noop */ }
      try { await port?.close(); } catch { /* noop */ }
      flashing = false;
    }
  }
</script>

<div class="webflasher cyber-card">
  <div class="wf-head">
    <div>
      <span class="badge badge-green">● Instalador Web v1</span>
      <h3>Flashea CBDos desde el navegador</h3>
      <p>
        Lee los <code>Releases</code> de GitHub, filtra por
        <code>soc + board_id</code> y escribe el <code>merged-bin @0x0</code>.
        Requiere Chrome/Edge (WebSerial) en HTTPS o localhost.
      </p>
    </div>
    <button class="btn btn-secondary" onclick={() => refreshReleases(true)} disabled={loading}>
      {loading ? 'Leyendo…' : '↻ Recargar Releases'}
    </button>
  </div>

  <!-- Selector de placa -->
  <div class="board-tabs">
    {#each BOARDS as b}
      <button
        class="board-tab"
        class:active={boardId === b.boardId}
        onclick={() => selectBoard(b.boardId)}
      >
        <strong>{b.name}</strong>
        <span>{b.boardId} · {b.soc}</span>
      </button>
    {/each}
  </div>

  {#if !serialOK}
    <div class="warn">
      ⚠ Tu navegador no expone WebSerial: podrás descargar el <code>.bin</code> pero no flashear
      ni detectar versión. Usa Chrome o Edge.
    </div>
  {/if}

  <!-- Firmware instalado -->
  <div class="row">
    <div class="installed">
      <span class="lbl">// FIRMWARE INSTALADO EN EL TARGET</span>
      {#if installed}
        <code class="banner">{installed.raw}</code>
        {#if versionHint()}
          <span class="hint">{versionHint()}</span>
        {/if}
      {:else if chipFound}
        <code class="banner">CHIP={chipFound} (sin CBDos — placa nueva o firmware sin banner)</code>
        <span class="hint">Placa preseleccionada por chip. Elige el firmware y flashea.</span>
      {:else if detectError}
        <span class="err">{detectError}</span>
      {:else}
        <span class="muted">Desconocido — conecta la placa y detecta.</span>
      {/if}
    </div>
    <button class="btn btn-secondary" onclick={detectInstalled} disabled={!serialOK || detecting}>
      {detecting ? 'Detectando…' : '🔌 Detectar placa'}
    </button>
  </div>

  <!-- Lista de firmwares -->
  <div class="fw-list">
    <span class="lbl">// FIRMWARES EN RELEASES PARA {board.boardId.toUpperCase()} ({board.soc.toUpperCase()})</span>
    {#if cacheInfo}<span class="cache-note">{cacheInfo}</span>{/if}
    {#if loading}
      <p class="muted">Consultando api.github.com/repos/kaber420/CBD-os/releases…</p>
    {:else if listError}
      <p class="err">{listError}</p>
    {:else}
      {#each files as f, i}
        <button class="fw-item" class:active={i === selectedIdx} onclick={() => pick(i)}>
          <div class="fw-main">
            <strong>{f.version}</strong>
            <span class="tag">{f.releaseTag}</span>
            {#if !f.boardId}
              <span class="tag generic">genérico {f.soc}</span>
            {/if}
          </div>
          <div class="fw-sub">
            <span>{f.fileName}</span>
            <span>· {formatSize(f.size)}</span>
            <span>· {new Date(f.publishedAt).toLocaleDateString('es-MX')}</span>
          </div>
        </button>
      {/each}
    {/if}
  </div>

  <!-- Acciones -->
  {#if selected}
    <div class="actions">
      <a class="btn btn-secondary" href={selected.downloadUrl} download={selected.fileName}>
        ⬇ Descargar .bin
      </a>
      {#if selected.iniUrl}
        <a class="btn btn-secondary" href={selected.iniUrl} download>
          .ini
        </a>
      {/if}
      <button class="btn btn-primary" onclick={flashSelected} disabled={!serialOK || flashing}>
        {flashing ? `Flasheando ${flashProgress}%…` : `⚡ Flashear ${selected.version} @0x0`}
      </button>
    </div>
    {#if flashing}
      <div class="progress"><div class="bar" style="width: {flashProgress}%"></div></div>
    {/if}
    {#if done}<p class="ok">✓ {done}</p>{/if}
    {#if flashError}<p class="err">{flashError}</p>{/if}
    {#if selected.notes}
      <details class="notes">
        <summary>Notas del release {selected.releaseTag}</summary>
        <pre>{selected.notes.slice(0, 2000)}</pre>
      </details>
    {/if}
  {/if}

  <!-- Consola -->
  <div class="console">
    <span class="lbl">// CONSOLA</span>
    <div class="console-body">
      {#each logLines as l}<div>{l}</div>{/each}
      {#if logLines.length === 0}<div class="muted">Listo para operar.</div>{/if}
    </div>
  </div>

  <p class="foot">
    BSP: <code>{board.bspPath}</code> · Convención:
    <code>cbdos-{board.boardId}-{board.soc}-&lt;ver&gt;-merged.bin</code>
    + <code>.ini</code> hermano. DTB completo (pines/drivers) reservado a futuro.
  </p>
</div>

<style>
  .webflasher { padding: 2rem; max-width: 900px; margin: 0 auto 2.5rem; }
  .wf-head { display: flex; justify-content: space-between; gap: 1rem; align-items: flex-start; margin-bottom: 1.5rem; flex-wrap: wrap; }
  .wf-head h3 { font-size: 1.4rem; margin: 0.5rem 0 0.3rem; }
  .wf-head p { color: var(--text-muted); font-size: 0.9rem; max-width: 560px; }
  .wf-head code, .foot code, .installed code { font-family: var(--font-mono); font-size: 0.8rem; color: var(--cyan-core); }
  .board-tabs { display: flex; gap: 0.8rem; margin-bottom: 1.5rem; flex-wrap: wrap; }
  .board-tab { flex: 1; min-width: 220px; display: flex; flex-direction: column; gap: 0.15rem; padding: 0.9rem 1.1rem; text-align: left;
    background: rgba(14,20,32,.6); border: 1px solid var(--border-subtle); border-radius: var(--radius-md);
    color: var(--text-main); cursor: pointer; }
  .board-tab span { font-family: var(--font-mono); font-size: 0.75rem; color: var(--text-muted); }
  .board-tab.active { border-color: var(--cyan-core); background: var(--cyan-dim); box-shadow: 0 0 20px var(--cyan-glow); }
  .warn { background: rgba(255,176,32,.08); border: 1px solid rgba(255,176,32,.3); color: var(--amber-core);
    padding: 0.8rem 1rem; border-radius: var(--radius-sm); font-size: 0.85rem; margin-bottom: 1.2rem; }
  .row { display: flex; justify-content: space-between; align-items: center; gap: 1rem; margin-bottom: 1.2rem; flex-wrap: wrap;
    background: rgba(0,0,0,.35); border: 1px solid var(--border-subtle); border-radius: var(--radius-sm); padding: 1rem; }
  .lbl { display: block; font-family: var(--font-mono); font-size: 0.72rem; color: var(--green-matrix); margin-bottom: 0.4rem; }
  .muted { color: var(--text-muted); font-size: 0.85rem; }
  .err { color: #ff6b6b; font-size: 0.85rem; }
  .ok { color: var(--green-matrix); font-size: 0.9rem; }
  .hint { display: block; font-size: 0.8rem; color: var(--amber-core); margin-top: 0.3rem; }
  .banner { display: block; margin: 0.3rem 0; word-break: break-all; }
  .fw-list { display: flex; flex-direction: column; gap: 0.6rem; margin-bottom: 1.2rem; }
  .cache-note { font-family: var(--font-mono); font-size: 0.72rem; color: var(--text-dim); }
  .fw-item { text-align: left; background: rgba(14,20,32,.6); border: 1px solid var(--border-subtle);
    border-radius: var(--radius-sm); padding: 0.8rem 1rem; cursor: pointer; color: var(--text-main); }
  .fw-item:hover { border-color: var(--border-cyan); }
  .fw-item.active { border-color: var(--cyan-core); background: var(--cyan-dim); }
  .fw-main { display: flex; gap: 0.6rem; align-items: center; }
  .tag { font-family: var(--font-mono); font-size: 0.72rem; color: var(--text-muted); border: 1px solid var(--border-subtle);
    border-radius: 4px; padding: 0.1rem 0.45rem; }
  .tag.generic { color: var(--amber-core); border-color: rgba(255,176,32,.35); }
  .fw-sub { font-family: var(--font-mono); font-size: 0.75rem; color: var(--text-muted); margin-top: 0.25rem; }
  .actions { display: flex; gap: 0.8rem; flex-wrap: wrap; margin-bottom: 1rem; }
  .progress { height: 8px; background: rgba(255,255,255,.06); border-radius: 4px; overflow: hidden; margin-bottom: 0.8rem; }
  .bar { height: 100%; background: var(--cyan-core); box-shadow: 0 0 12px var(--cyan-glow); transition: width .2s; }
  .notes { font-size: 0.8rem; color: var(--text-muted); margin-bottom: 1rem; }
  .notes pre { white-space: pre-wrap; font-size: 0.75rem; max-height: 200px; overflow: auto; }
  .console-body { background: rgba(0,0,0,.7); border: 1px solid var(--border-subtle); border-radius: var(--radius-sm);
    padding: 0.8rem 1rem; font-family: var(--font-mono); font-size: 0.75rem; max-height: 220px; overflow-y: auto; }
  .foot { font-size: 0.75rem; color: var(--text-dim); margin-top: 1rem; }
</style>
