<script lang="ts">
  type ExampleKey = 'ui' | 'badusb' | 'mesh' | 'audio';

  let activeExample = $state<ExampleKey>('ui');
  let copyStatus = $state(false);

  const examples: Record<ExampleKey, { title: string; filename: string; desc: string; code: string }> = {
    ui: {
      title: 'Ventana & Botón Táctil',
      filename: 'counter.luapp',
      desc: 'Crea ventanas y controles interactivos en LVGL v9.5 sin recompilar el firmware.',
      code: `-- counter.luapp - Aplicación Lua++ para CBDos
local app = cbdos.app.create("Contador Táctil", "v1.0")
local count = 0

-- Contenedor y etiqueta de texto
local label = cbdos.ui.label(app.view, "Pulsaciones: 0")
label:set_style("font-size: 24px; color: #00f0ff;")

-- Botón reactivo con evento de pulsación
local btn = cbdos.ui.button(app.view, "INCREMENTAR")
btn:on_click(function()
    count = count + 1
    label:set_text("Pulsaciones: " .. tostring(count))
    cbdos.audio.beep(880, 50) -- Tono de confirmación
end)

app:run()`
    },
    badusb: {
      title: 'Inyector HID BadUSB',
      filename: 'stealth_recon.luapp',
      desc: 'Control del periférico USB-OTG en modo teclado/ratón para auditorías de seguridad.',
      code: `-- stealth_recon.luapp - Ataque DuckyScript mediante API HID
local hid = cbdos.hid

if not hid.is_connected() then
    print("[ERROR] Conecta el puerto USB-OTG a un host.")
    return
end

print("[HID] Dispositivo USB inicializado. Inyectando payload...")
cbdos.sys.delay(1000)

-- Abrir diálogo Ejecutar en Windows
hid.send_combo({"GUI", "r"})
cbdos.sys.delay(300)

-- Escribir comando powershell y ejecutar
hid.type_string("powershell -NoP -NonI -W Hidden -Exec Bypass")
hid.press_key("ENTER")
cbdos.sys.delay(500)

print("[HID] Inyección completada exitosamente.")`
    },
    mesh: {
      title: 'Transmisor Mesh ESP-NOW',
      filename: 'mesh_beacon.luapp',
      desc: 'Envío de paquetes TLV cifrados fuera de internet a nodos cercanos.',
      code: `-- mesh_beacon.luapp - Baliza de telemetría P2P
local mesh = cbdos.mesh
local channel = 6

mesh.set_channel(channel)
mesh.set_encryption_key("CBDOS-SECRET-KEY-128")

-- Callback reactivo al recibir paquetes de otros Cyberdecks
mesh.on_receive(function(sender_short_id, rssi, data)
    print(string.format("[MESH] De: 0x%04X | RSSI: %d dBm | Msg: %s", 
        sender_short_id, rssi, data))
end)

-- Enviar paquete TLV de estado cada 5 segundos
while true do
    local battery = cbdos.sys.get_battery_voltage()
    local payload = string.format("NODE_ALIVE:batt=%.2fV", battery)
    mesh.broadcast(payload)
    cbdos.sys.delay(5000)
end`
    },
    audio: {
      title: 'Control de Audio ES8311',
      filename: 'player_control.luapp',
      desc: 'Manejo del decodificador MP3 Helix y amplificador I2S Everest ES8311.',
      code: `-- player_control.luapp - Reproductor de música en segundo plano
local audio = cbdos.audio

-- Inicializar códec Everest ES8311 con ganancia segura
audio.set_volume(75) -- 0 a 100%
audio.set_eq_preset("BASS_BOOST")

-- Cargar lista de reproducción de la MicroSD
local playlist = cbdos.storage.list_files("/sdcard/music", "*.mp3")
print(string.format("[AUDIO] %d canciones encontradas.", #playlist))

if #playlist > 0 then
    print("[AUDIO] Reproduciendo: " .. playlist[1])
    audio.play_file("/sdcard/music/" .. playlist[1])
    
    -- El decodificador Helix corre en Core 1 sin congelar la UI
end`
    }
  };

  function copyCode() {
    navigator.clipboard.writeText(examples[activeExample].code);
    copyStatus = true;
    setTimeout(() => copyStatus = false, 2000);
  }
</script>

<section id="luapp" class="luapp-section">
  <div class="container">
    <div class="section-header">
      <span class="section-tag">// ECOSISTEMA EXTENSIBLE</span>
      <h2 class="section-title">Micro-Apps en Lua++ (.luapp)</h2>
      <p class="section-desc">
        Desarrolla herramientas y juegos en scripts Lua++ ligeros. Se ejecutan en una máquina virtual segura en PSRAM con acceso directo a pantalla, audio, USB y radio.
      </p>
    </div>

    <div class="playground-grid">
      <!-- Left: Code Editor Window -->
      <div class="code-window cyber-card">
        <div class="editor-header">
          <div class="editor-tabs">
            <button 
              class="tab-file" 
              class:active={activeExample === 'ui'} 
              onclick={() => activeExample = 'ui'}
            >
              counter.luapp
            </button>
            <button 
              class="tab-file" 
              class:active={activeExample === 'badusb'} 
              onclick={() => activeExample = 'badusb'}
            >
              badusb.luapp
            </button>
            <button 
              class="tab-file" 
              class:active={activeExample === 'mesh'} 
              onclick={() => activeExample = 'mesh'}
            >
              mesh.luapp
            </button>
            <button 
              class="tab-file" 
              class:active={activeExample === 'audio'} 
              onclick={() => activeExample = 'audio'}
            >
              audio.luapp
            </button>
          </div>

          <button class="btn-copy-code" onclick={copyCode}>
            {#if copyStatus}
              <span style="color: var(--green-matrix);">✓ Copiado</span>
            {:else}
              <svg viewBox="0 0 24 24" width="16" height="16" fill="none" stroke="currentColor" stroke-width="2">
                <rect x="9" y="9" width="13" height="13" rx="2" ry="2"></rect>
                <path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1"></path>
              </svg>
              <span>Copiar</span>
            {/if}
          </button>
        </div>

        <div class="editor-info-banner">
          <span class="badge badge-purple">{examples[activeExample].title}</span>
          <span class="banner-desc">{examples[activeExample].desc}</span>
        </div>

        <pre class="code-body"><code>{examples[activeExample].code}</code></pre>
      </div>

      <!-- Right: Lua++ API Reference Card -->
      <div class="api-card cyber-card">
        <div class="api-header">
          <span class="badge badge-cyan">CBDOS LUA++ SDK</span>
          <h3>Módulos del Sistema</h3>
        </div>

        <div class="api-list">
          <div class="api-item">
            <code>cbdos.ui.*</code>
            <p>Creación de widgets LVGL 9.5 (botones, switches, labels, arc visualizers, canvas).</p>
          </div>
          <div class="api-item">
            <code>cbdos.hid.*</code>
            <p>Emulación de teclado/ratón USB, inyección de macros DuckyScript y combinaciones de teclas.</p>
          </div>
          <div class="api-item">
            <code>cbdos.mesh.*</code>
            <p>Comunicaciones P2P con ESP-NOW, descubrimiento de nodos y paquetes con cifrado simétrico.</p>
          </div>
          <div class="api-item">
            <code>cbdos.audio.*</code>
            <p>Reproducción de archivos MP3/WAV mediante Helix en Core 1, control de volumen y presets de EQ.</p>
          </div>
          <div class="api-item">
            <code>cbdos.storage.*</code>
            <p>Lectura/escritura en MicroSD Slot 0 (/sdcard/), listado de carpetas y carga de configs.</p>
          </div>
          <div class="api-item">
            <code>cbdos.sys.*</code>
            <p>Frecuencia de CPU (400MHz), estado de memoria PSRAM libre, batería y delays reactivos.</p>
          </div>
        </div>

        <div class="luapp-deploy-note">
          <span class="note-icon">💡</span>
          <p>Para ejecutar una micro-app, simplemente guarda el archivo <code>mi_app.luapp</code> en la MicroSD dentro de <code>/sdcard/apps/</code> y ábrela desde el lanzador de CBDos.</p>
        </div>
      </div>
    </div>
  </div>
</section>

<style>
  .luapp-section {
    padding: 5rem 0;
    position: relative;
  }

  .playground-grid {
    display: grid;
    grid-template-columns: 1.3fr 1fr;
    gap: 2rem;
  }

  .code-window {
    display: flex;
    flex-direction: column;
    background: rgba(10, 14, 22, 0.9);
    border: 1px solid var(--border-subtle);
  }

  .editor-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    background: rgba(6, 8, 14, 0.85);
    border-bottom: 1px solid var(--border-subtle);
    padding: 0 0.8rem;
    overflow-x: auto;
  }

  .editor-tabs {
    display: flex;
  }

  .tab-file {
    padding: 0.8rem 1.1rem;
    background: transparent;
    border: none;
    border-bottom: 2px solid transparent;
    color: var(--text-muted);
    font-family: var(--font-mono);
    font-size: 0.82rem;
    cursor: pointer;
    transition: all 0.2s;
  }

  .tab-file:hover {
    color: var(--text-main);
  }

  .tab-file.active {
    color: var(--cyan-core);
    border-bottom-color: var(--cyan-core);
    background: rgba(0, 240, 255, 0.04);
  }

  .btn-copy-code {
    display: flex;
    align-items: center;
    gap: 0.4rem;
    background: rgba(255, 255, 255, 0.05);
    border: 1px solid var(--border-subtle);
    border-radius: var(--radius-sm);
    color: var(--text-muted);
    font-family: var(--font-mono);
    font-size: 0.75rem;
    padding: 0.35rem 0.7rem;
    cursor: pointer;
    transition: all 0.2s;
  }

  .btn-copy-code:hover {
    border-color: var(--border-cyan);
    color: var(--cyan-core);
  }

  .editor-info-banner {
    padding: 0.75rem 1.2rem;
    background: rgba(139, 92, 246, 0.08);
    border-bottom: 1px solid rgba(139, 92, 246, 0.15);
    display: flex;
    align-items: center;
    gap: 0.8rem;
  }

  .banner-desc {
    font-size: 0.85rem;
    color: var(--text-muted);
  }

  .code-body {
    padding: 1.5rem;
    font-family: var(--font-mono);
    font-size: 0.85rem;
    line-height: 1.65;
    color: #e2e8f0;
    overflow-x: auto;
    flex: 1;
  }

  .api-card {
    padding: 2rem;
    display: flex;
    flex-direction: column;
  }

  .api-header {
    margin-bottom: 1.5rem;
    display: flex;
    flex-direction: column;
    gap: 0.4rem;
  }

  .api-header h3 {
    font-size: 1.5rem;
    font-weight: 700;
  }

  .api-list {
    display: flex;
    flex-direction: column;
    gap: 1.1rem;
    flex: 1;
  }

  .api-item code {
    font-family: var(--font-mono);
    font-size: 0.85rem;
    color: var(--cyan-core);
    display: block;
    margin-bottom: 0.2rem;
  }

  .api-item p {
    font-size: 0.85rem;
    color: var(--text-muted);
    line-height: 1.4;
  }

  .luapp-deploy-note {
    margin-top: 1.5rem;
    padding: 1rem;
    background: rgba(0, 240, 255, 0.06);
    border: 1px dashed var(--border-cyan);
    border-radius: var(--radius-md);
    display: flex;
    align-items: flex-start;
    gap: 0.75rem;
  }

  .note-icon {
    font-size: 1.2rem;
  }

  .luapp-deploy-note p {
    font-size: 0.82rem;
    color: var(--text-muted);
    line-height: 1.5;
  }

  .luapp-deploy-note code {
    font-family: var(--font-mono);
    color: var(--cyan-core);
  }

  @media (max-width: 990px) {
    .playground-grid {
      grid-template-columns: 1fr;
    }
  }
</style>
