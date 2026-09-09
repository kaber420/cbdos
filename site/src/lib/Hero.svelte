<script lang="ts">
  let activeCliTab = $state('status');
  let copiedCommand = $state(false);

  const cliOutputs: Record<string, { cmd: string; output: string[] }> = {
    status: {
      cmd: 'sys.status()',
      output: [
        '[CBDOS] Target: Guition JC4880P443C (ESP32-P4 RISC-V @ 400MHz)',
        '[CBDOS] Cores: 2 (Core 0: OS/LVGL, Core 1: Media/RF Engine)',
        '[CBDOS] Memory: 32MB Hexal-PSRAM | Free: 29.4MB | Heap: OK',
        '[CBDOS] Display: 4.3" MIPI-DPI 480x800 ST7701S @ 60 FPS (DMA2D)',
        '[CBDOS] Audio Codec: Everest ES8311 (I2S DMA) PA Pin: 11',
        '[CBDOS] Storage: MicroSD Slot 0 SDMMC 4-bit (3.3V VO4) -> /sdcard',
        '[CBDOS] Coprocessor: ESP32-C6-MINI via SDIO Slot 1 (Ready)',
        '[STATUS] System Ready. Zero-polling event loops active.'
      ]
    },
    badusb: {
      cmd: 'hid.run_payload("/sdcard/payloads/recon.ducky")',
      output: [
        '[HID] USB-OTG Device enumerated as USB Composite (Keyboard + Mouse)',
        '[HID] Parsing DuckyScript v2 payload: recon.ducky...',
        '[HID] SENDING: GUI r',
        '[HID] DELAY: 250ms',
        '[HID] SENDING: powershell -w hidden -c "Write-Host CBDos Pwned!"',
        '[HID] SENDING: ENTER',
        '[HID] Injected 48 keystrokes in 320ms. Device detached cleanly.'
      ]
    },
    audio: {
      cmd: 'audio.play_stream("/sdcard/music/synthwave.mp3")',
      output: [
        '[AUDIO] Codec ES8311 powered on via I2C (0x18), LDO VO4 stable',
        '[AUDIO] Helix MP3 decoder spawned on Core 1 (Task: audio_decode)',
        '[AUDIO] Bitrate: 320 kbps | Samplerate: 44.1 kHz | Stereo',
        '[AUDIO] RingBuffer: 64 KB DMA Stream OK | FFT visualizer reactive'
      ]
    }
  };

  function copyInstallCmd() {
    navigator.clipboard.writeText('git clone https://github.com/kaber420/cbdos.git && cd cbdos');
    copiedCommand = true;
    setTimeout(() => copiedCommand = false, 2500);
  }
</script>

<section id="hero" class="hero-section">
  <div class="container hero-grid">
    <div class="hero-content">
      <div class="hero-badges">
        <span class="badge badge-cyan">ESP32-P4 RISC-V 400MHz</span>
        <span class="badge badge-purple">LVGL v9.5 Strict</span>
        <span class="badge badge-green">Zero-Poll Reactive</span>
      </div>

      <h1 class="hero-title">
        CyBerDeck <span class="gradient-text">OS</span>
      </h1>

      <p class="hero-description">
        Sistema operativo gráfico para cyberdecks y dispositivos portátiles basados en ESP32 (P4 y S3). Interfaz fluida con LVGL v9.5 acelerada por hardware, ecosistema de scripts Lua++ y herramientas integradas.
      </p>

      <div class="hero-cta">
        <a href="#/flasheo" class="btn btn-primary">
          <svg viewBox="0 0 24 24" width="20" height="20" fill="none" stroke="currentColor" stroke-width="2.5">
            <polygon points="13 2 3 14 12 14 11 22 21 10 12 10 13 2"/>
          </svg>
          Guía de Flasheo
        </a>
        <a href="#/hardware" class="btn btn-secondary">
          <svg viewBox="0 0 24 24" width="20" height="20" fill="none" stroke="currentColor" stroke-width="2">
            <rect x="4" y="4" width="16" height="16" rx="2"/>
            <rect x="9" y="9" width="6" height="6"/>
            <line x1="9" y1="1" x2="9" y2="4"/>
            <line x1="15" y1="1" x2="15" y2="4"/>
            <line x1="9" y1="20" x2="9" y2="23"/>
            <line x1="15" y1="20" x2="15" y2="23"/>
          </svg>
          Explorar Hardware
        </a>
        <a href="#/manuales" class="btn btn-secondary">
          <svg viewBox="0 0 24 24" width="20" height="20" fill="none" stroke="currentColor" stroke-width="2">
            <path d="M4 19.5A2.5 2.5 0 0 1 6.5 17H20"/>
            <path d="M6.5 2H20v20H6.5A2.5 2.5 0 0 1 4 19.5v-15A2.5 2.5 0 0 1 6.5 2z"/>
          </svg>
          Manuales .md
        </a>
      </div>

      <div class="clone-box">
        <span class="clone-prompt">$</span>
        <code class="clone-code">git clone https://github.com/kaber420/cbdos.git</code>
        <button class="btn-copy" onclick={copyInstallCmd} aria-label="Copiar comando">
          {#if copiedCommand}
            <span class="copied-feedback">✓ Copiado</span>
          {:else}
            <svg viewBox="0 0 24 24" width="16" height="16" fill="none" stroke="currentColor" stroke-width="2">
              <rect x="9" y="9" width="13" height="13" rx="2" ry="2"></rect>
              <path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1"></path>
            </svg>
          {/if}
        </button>
      </div>
    </div>

    <!-- Live Interactive Console Widget -->
    <div class="hero-terminal cyber-card">
      <div class="terminal-header">
        <div class="terminal-dots">
          <span class="dot dot-red"></span>
          <span class="dot dot-yellow"></span>
          <span class="dot dot-green"></span>
        </div>
        <div class="terminal-title">cbdos-serial-cli // /dev/ttyACM0</div>
        <div class="terminal-badge">LIVE REPL</div>
      </div>

      <div class="terminal-tabs">
        <button class="tab-btn" class:active={activeCliTab === 'status'} onclick={() => activeCliTab = 'status'}>
          sys.status()
        </button>
        <button class="tab-btn" class:active={activeCliTab === 'badusb'} onclick={() => activeCliTab = 'badusb'}>
          hid.badusb
        </button>
        <button class="tab-btn" class:active={activeCliTab === 'audio'} onclick={() => activeCliTab = 'audio'}>
          audio.stream
        </button>
      </div>

      <div class="terminal-body">
        <div class="terminal-input-line">
          <span class="terminal-prompt">&gt;</span>
          <span class="terminal-cmd">{cliOutputs[activeCliTab].cmd}</span>
        </div>
        <div class="terminal-stream">
          {#each cliOutputs[activeCliTab].output as line}
            <div class="terminal-line" class:highlight={line.startsWith('[STATUS]') || line.startsWith('[CBDOS]')}>
              {line}
            </div>
          {/each}
        </div>
      </div>
    </div>
  </div>
</section>

<style>
  .hero-section {
    padding: 5.5rem 0 4rem;
    position: relative;
  }

  .hero-grid {
    display: grid;
    grid-template-columns: 1fr 1.05fr;
    gap: 3.5rem;
    align-items: center;
  }

  .hero-badges {
    display: flex;
    flex-wrap: wrap;
    gap: 0.6rem;
    margin-bottom: 1.5rem;
  }

  .hero-title {
    font-size: 3.8rem;
    font-weight: 900;
    line-height: 1.05;
    letter-spacing: -0.03em;
    margin-bottom: 1.4rem;
    color: var(--text-main);
  }

  .gradient-text {
    background: linear-gradient(135deg, var(--cyan-core) 0%, #8b5cf6 50%, var(--pink-neon) 100%);
    -webkit-background-clip: text;
    -webkit-text-fill-color: transparent;
  }

  .hero-description {
    font-size: 1.15rem;
    line-height: 1.65;
    color: var(--text-muted);
    margin-bottom: 2.2rem;
  }

  .hero-cta {
    display: flex;
    flex-wrap: wrap;
    gap: 1rem;
    margin-bottom: 2rem;
  }

  .clone-box {
    display: flex;
    align-items: center;
    background: rgba(9, 11, 16, 0.8);
    border: 1px solid var(--border-subtle);
    border-radius: var(--radius-md);
    padding: 0.6rem 1rem;
    font-family: var(--font-mono);
    font-size: 0.85rem;
    max-width: 480px;
  }

  .clone-prompt {
    color: var(--cyan-core);
    margin-right: 0.75rem;
    font-weight: 700;
  }

  .clone-code {
    flex: 1;
    color: var(--text-main);
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }

  .btn-copy {
    background: transparent;
    border: none;
    color: var(--text-muted);
    cursor: pointer;
    display: flex;
    align-items: center;
    padding: 0.2rem;
    transition: color 0.2s;
  }

  .btn-copy:hover {
    color: var(--cyan-core);
  }

  .copied-feedback {
    font-size: 0.75rem;
    color: var(--green-matrix);
    font-weight: 700;
  }

  /* Terminal Styling */
  .hero-terminal {
    background: rgba(10, 14, 23, 0.88);
    border: 1px solid var(--border-cyan);
    box-shadow: 0 12px 40px rgba(0, 0, 0, 0.6), 0 0 30px rgba(0, 240, 255, 0.12);
  }

  .terminal-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 0.75rem 1.2rem;
    background: rgba(5, 8, 14, 0.9);
    border-bottom: 1px solid var(--border-subtle);
  }

  .terminal-dots {
    display: flex;
    gap: 0.45rem;
  }

  .dot {
    width: 10px;
    height: 10px;
    border-radius: 50%;
  }

  .dot-red { background: #ef4444; }
  .dot-yellow { background: #eab308; }
  .dot-green { background: #22c55e; }

  .terminal-title {
    font-family: var(--font-mono);
    font-size: 0.75rem;
    color: var(--text-muted);
  }

  .terminal-badge {
    font-family: var(--font-mono);
    font-size: 0.65rem;
    padding: 0.15rem 0.4rem;
    background: rgba(0, 255, 102, 0.15);
    color: var(--green-matrix);
    border: 1px solid rgba(0, 255, 102, 0.3);
    border-radius: 3px;
  }

  .terminal-tabs {
    display: flex;
    background: rgba(14, 20, 32, 0.6);
    border-bottom: 1px solid var(--border-subtle);
    overflow-x: auto;
  }

  .tab-btn {
    padding: 0.6rem 1rem;
    background: transparent;
    border: none;
    border-bottom: 2px solid transparent;
    color: var(--text-muted);
    font-family: var(--font-mono);
    font-size: 0.78rem;
    cursor: pointer;
    transition: all 0.2s;
    white-space: nowrap;
  }

  .tab-btn:hover {
    color: var(--text-main);
  }

  .tab-btn.active {
    color: var(--cyan-core);
    border-bottom-color: var(--cyan-core);
    background: rgba(0, 240, 255, 0.05);
  }

  .terminal-body {
    padding: 1.2rem;
    font-family: var(--font-mono);
    font-size: 0.82rem;
    min-height: 280px;
    display: flex;
    flex-direction: column;
    gap: 0.6rem;
  }

  .terminal-input-line {
    display: flex;
    align-items: center;
    gap: 0.6rem;
    padding-bottom: 0.4rem;
    border-bottom: 1px dashed rgba(255, 255, 255, 0.08);
  }

  .terminal-prompt {
    color: var(--pink-neon);
    font-weight: 800;
  }

  .terminal-cmd {
    color: var(--cyan-core);
    font-weight: 600;
  }

  .terminal-stream {
    display: flex;
    flex-direction: column;
    gap: 0.35rem;
    line-height: 1.5;
  }

  .terminal-line {
    color: var(--text-muted);
  }

  .terminal-line.highlight {
    color: #cbd5e1;
  }

  @media (max-width: 990px) {
    .hero-grid {
      grid-template-columns: 1fr;
      gap: 2.5rem;
    }

    .hero-title {
      font-size: 3rem;
    }
  }
</style>
