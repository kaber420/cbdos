<script lang="ts">
  let selectedBoard = $state<'p4' | 's3'>('p4');

  const boards = {
    p4: {
      name: 'Guition JC4880P443C',
      tag: 'Target Principal // Flagship',
      soc: 'ESP32-P4 RISC-V Dual-Core @ 400 MHz (Chip Rev 1.3)',
      ram: '32 MB Hexal-PSRAM (Alta Velocidad)',
      flash: '16 MB NOR Flash',
      display: '4.3" IPS 480x800 MIPI-DPI (ST7701S) @ 60 FPS',
      touch: 'Capacitivo Goodix GT911 (I2C SDA=7, SCL=8, RST=3, INT=4)',
      audio: 'Everest ES8311 I2S + Amplificador PA Pin 11 (MCLK=13, BCLK=12, WS=10, DOUT=9)',
      storage: 'MicroSD Slot 0 SDMMC 4-bit (GPIO 39-44) + Regulador LDO VO4 (3.3V)',
      coproc: 'ESP32-C6-MINI (Wi-Fi 6 / Bluetooth 5 / Thread vía SDIO Slot 1)',
      framework: 'ESP-IDF 5.5 Nativo (CMake / Ninja)',
      bspPath: 'bsp/esp32_p4_jc4880',
      buildCmd: '. $HOME/esp/esp-idf/export.sh && cd bsp/esp32_p4_jc4880 && idf.py build',
      flashCmd: 'idf.py -p /dev/ttyACM0 flash monitor',
      image: 'images/jc4880p443c_pinout_header.png'
    },
    s3: {
      name: 'Guition JC3248W535',
      tag: 'Target Portátil // Ultra-Compact',
      soc: 'ESP32-S3 Dual-Core Xtensa LX7 @ 240 MHz',
      ram: '8 MB Octal-PSRAM',
      flash: '16 MB Quad-SPI Flash',
      display: '3.5" IPS 320x480 QSPI (AXS15231B / ST7796) @ 30 FPS',
      touch: 'Capacitivo FocalTech FT6336 (I2C)',
      audio: 'DAC I2S Integrado + Altavoz mono con control de mute',
      storage: 'MicroSD Slot SPI estándar',
      coproc: 'Wi-Fi 4 + BLE 5 nativo en el propio SoC ESP32-S3',
      framework: 'PlatformIO + Arduino Core (pioarduino)',
      bspPath: 'bsp/esp32_s3_jc3248',
      buildCmd: 'pio run -d bsp/esp32_s3_jc3248',
      flashCmd: 'pio run -d bsp/esp32_s3_jc3248 -t upload --upload-port /dev/ttyACM0',
      image: null
    }
  };
</script>

<section id="hardware" class="hardware-section">
  <div class="container">
    <div class="section-header">
      <span class="section-tag">// SOPORTE MULTI-TARGET</span>
      <h2 class="section-title">Hardware Compatible</h2>
      <p class="section-desc">
        CBDos desacopla el núcleo agnóstico del soporte físico. Selecciona la placa para consultar especificaciones de pinout, buses y comandos.
      </p>
    </div>

    <!-- Board Selector Tabs -->
    <div class="board-selector">
      <button 
        class="board-tab-btn" 
        class:active={selectedBoard === 'p4'} 
        onclick={() => selectedBoard = 'p4'}
      >
        <div class="board-btn-info">
          <span class="board-title">JC4880P443C (ESP32-P4)</span>
          <span class="board-badge">400MHz • 4.3" MIPI-DPI • 60 FPS</span>
        </div>
      </button>

      <button 
        class="board-tab-btn" 
        class:active={selectedBoard === 's3'} 
        onclick={() => selectedBoard = 's3'}
      >
        <div class="board-btn-info">
          <span class="board-title">JC3248W535 (ESP32-S3)</span>
          <span class="board-badge">240MHz • 3.5" QSPI • 30 FPS</span>
        </div>
      </button>
    </div>

    <!-- Active Board Card -->
    <div class="board-details cyber-card">
      <div class="board-header">
        <div>
          <span class="badge badge-cyan">{boards[selectedBoard].tag}</span>
          <h3 class="board-model">{boards[selectedBoard].name}</h3>
        </div>
        <div class="bsp-path-tag">
          <code>{boards[selectedBoard].bspPath}</code>
        </div>
      </div>

      <div class="board-grid">
        <!-- Specs Column -->
        <div class="specs-col">
          <div class="spec-row">
            <span class="spec-label">Microcontrolador:</span>
            <span class="spec-val highlight">{boards[selectedBoard].soc}</span>
          </div>
          <div class="spec-row">
            <span class="spec-label">Memoria PSRAM:</span>
            <span class="spec-val">{boards[selectedBoard].ram}</span>
          </div>
          <div class="spec-row">
            <span class="spec-label">Almacenamiento Flash:</span>
            <span class="spec-val">{boards[selectedBoard].flash}</span>
          </div>
          <div class="spec-row">
            <span class="spec-label">Pantalla:</span>
            <span class="spec-val">{boards[selectedBoard].display}</span>
          </div>
          <div class="spec-row">
            <span class="spec-label">Panel Táctil:</span>
            <span class="spec-val">{boards[selectedBoard].touch}</span>
          </div>
          <div class="spec-row">
            <span class="spec-label">Subsistema Audio:</span>
            <span class="spec-val">{boards[selectedBoard].audio}</span>
          </div>
          <div class="spec-row">
            <span class="spec-label">Slot MicroSD:</span>
            <span class="spec-val">{boards[selectedBoard].storage}</span>
          </div>
          <div class="spec-row">
            <span class="spec-label">Coprocesador / Radio:</span>
            <span class="spec-val highlight-purple">{boards[selectedBoard].coproc}</span>
          </div>
          <div class="spec-row">
            <span class="spec-label">Framework Build:</span>
            <span class="spec-val">{boards[selectedBoard].framework}</span>
          </div>
        </div>

        <!-- Media / Diagram Column -->
        <div class="diagram-col">
          {#if boards[selectedBoard].image}
            <div class="diagram-wrapper">
              <div class="diagram-label">// DIAGRAMA DE PINES DEL HEADER FÍSICO</div>
              <img src={boards[selectedBoard].image} alt="Pinout Header JC4880P443C" class="pinout-img" />
            </div>
          {:else}
            <div class="placeholder-diagram">
              <span class="diagram-icon">📟</span>
              <h4>Placa JC3248W535 (ESP32-S3)</h4>
              <p>Formato compacto con conector QSPI integrado, USB-C dual y slot MicroSD SPI.</p>
              <div class="badge badge-purple">Soporte Completo en bsp/esp32_s3_jc3248</div>
            </div>
          {/if}

          <!-- Quick Terminal Build Command Box -->
          <div class="quick-cmd-box">
            <div class="cmd-label">// COMPILACIÓN RÁPIDA ({selectedBoard.toUpperCase()})</div>
            <code>{boards[selectedBoard].buildCmd}</code>
            <div class="cmd-label" style="margin-top: 0.6rem;">// FLASHEAR Y MONITOREAR</div>
            <code>{boards[selectedBoard].flashCmd}</code>
          </div>
        </div>
      </div>
    </div>
  </div>
</section>

<style>
  .hardware-section {
    padding: 5rem 0;
    position: relative;
  }

  .board-selector {
    display: flex;
    gap: 1.2rem;
    margin-bottom: 2rem;
    flex-wrap: wrap;
  }

  .board-tab-btn {
    flex: 1;
    min-width: 280px;
    display: flex;
    align-items: center;
    gap: 1rem;
    padding: 1.2rem 1.6rem;
    background: rgba(14, 20, 32, 0.6);
    border: 1px solid var(--border-subtle);
    border-radius: var(--radius-lg);
    cursor: pointer;
    transition: all 0.25s cubic-bezier(0.16, 1, 0.3, 1);
    text-align: left;
  }

  .board-tab-btn:hover {
    border-color: var(--border-cyan);
    background: rgba(0, 240, 255, 0.05);
  }

  .board-tab-btn.active {
    border-color: var(--cyan-core);
    background: rgba(0, 240, 255, 0.1);
    box-shadow: 0 0 25px rgba(0, 240, 255, 0.15);
  }

  .board-btn-info {
    display: flex;
    flex-direction: column;
    gap: 0.25rem;
  }

  .board-title {
    font-size: 1.15rem;
    font-weight: 700;
    color: var(--text-main);
  }

  .board-badge {
    font-family: var(--font-mono);
    font-size: 0.75rem;
    color: var(--cyan-core);
  }

  .board-details {
    padding: 2.5rem;
  }

  .board-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    margin-bottom: 2rem;
    padding-bottom: 1.5rem;
    border-bottom: 1px solid var(--border-subtle);
    flex-wrap: wrap;
    gap: 1rem;
  }

  .board-model {
    font-size: 2rem;
    font-weight: 800;
    margin-top: 0.4rem;
    color: var(--text-main);
  }

  .bsp-path-tag code {
    font-family: var(--font-mono);
    font-size: 0.85rem;
    padding: 0.4rem 0.8rem;
    background: rgba(0, 0, 0, 0.6);
    border: 1px solid var(--border-subtle);
    border-radius: var(--radius-sm);
    color: var(--text-muted);
  }

  .board-grid {
    display: grid;
    grid-template-columns: 1.15fr 1fr;
    gap: 2.5rem;
    align-items: start;
  }

  .specs-col {
    display: flex;
    flex-direction: column;
    gap: 1rem;
  }

  .spec-row {
    display: flex;
    flex-direction: column;
    gap: 0.2rem;
    padding-bottom: 0.8rem;
    border-bottom: 1px dashed rgba(255, 255, 255, 0.06);
  }

  .spec-label {
    font-size: 0.8rem;
    font-family: var(--font-mono);
    text-transform: uppercase;
    letter-spacing: 0.05em;
    color: var(--text-dim);
  }

  .spec-val {
    font-size: 0.95rem;
    font-weight: 500;
    color: var(--text-main);
  }

  .spec-val.highlight {
    color: var(--cyan-core);
    font-weight: 600;
  }

  .spec-val.highlight-purple {
    color: #c084fc;
    font-weight: 600;
  }

  .diagram-col {
    display: flex;
    flex-direction: column;
    gap: 1.5rem;
  }

  .diagram-wrapper {
    background: rgba(0, 0, 0, 0.4);
    border: 1px solid var(--border-subtle);
    border-radius: var(--radius-md);
    padding: 1.2rem;
  }

  .diagram-label {
    font-family: var(--font-mono);
    font-size: 0.75rem;
    color: var(--cyan-core);
    margin-bottom: 0.8rem;
  }

  .pinout-img {
    width: 100%;
    height: auto;
    border-radius: var(--radius-sm);
    border: 1px solid rgba(255, 255, 255, 0.08);
  }

  .placeholder-diagram {
    padding: 3rem 2rem;
    text-align: center;
    background: rgba(14, 20, 32, 0.4);
    border: 1px dashed var(--border-subtle);
    border-radius: var(--radius-md);
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 0.8rem;
  }

  .placeholder-diagram h4 {
    font-size: 1.2rem;
  }

  .placeholder-diagram p {
    font-size: 0.9rem;
    color: var(--text-muted);
    max-width: 320px;
  }

  .quick-cmd-box {
    background: rgba(6, 8, 12, 0.9);
    border: 1px solid var(--border-cyan);
    border-radius: var(--radius-md);
    padding: 1.2rem;
    font-family: var(--font-mono);
    font-size: 0.8rem;
  }

  .cmd-label {
    color: var(--green-matrix);
    font-size: 0.72rem;
    margin-bottom: 0.3rem;
  }

  .quick-cmd-box code {
    display: block;
    color: var(--text-main);
    word-break: break-all;
  }

  @media (max-width: 900px) {
    .board-grid {
      grid-template-columns: 1fr;
    }
  }
</style>
