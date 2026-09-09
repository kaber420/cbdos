<script lang="ts">
  import WebFlasher from './WebFlasher.svelte';
  let activeGuide = $state<'p4' | 's3' | 'coproc'>('p4');
  let copiedStep = $state<string | null>(null);

  function copyText(key: string, text: string) {
    navigator.clipboard.writeText(text);
    copiedStep = key;
    setTimeout(() => copiedStep = null, 2000);
  }
</script>

<section id="flashing" class="flashing-section">
  <div class="container">
    <div class="section-header">
      <span class="section-tag">// DESPLIEGUE & INSTALACIÓN</span>
      <h2 class="section-title">Guía Rápida de Flasheo</h2>
      <p class="section-desc">
        Instrucciones paso a paso para compilar y cargar CBDos en tu dispositivo mediante USB-C.
      </p>
    </div>

    <WebFlasher />

    <div class="section-header" style="margin-top: 1rem;">
      <span class="section-tag">// COMPILACIÓN MANUAL (AVANZADO)</span>
    </div>

    <div class="guide-toggle">
      <button 
        class="toggle-btn" 
        class:active={activeGuide === 'p4'} 
        onclick={() => activeGuide = 'p4'}
      >
        ESP32-P4 (JC4880 - ESP-IDF)
      </button>
      <button 
        class="toggle-btn" 
        class:active={activeGuide === 's3'} 
        onclick={() => activeGuide = 's3'}
      >
        ESP32-S3 (JC3248 - PlatformIO)
      </button>
      <button 
        class="toggle-btn" 
        class:active={activeGuide === 'coproc'} 
        onclick={() => activeGuide = 'coproc'}
      >
        Coprocesador C6 (Flasher Autónomo)
      </button>
    </div>

    <!-- P4 Guide -->
    {#if activeGuide === 'p4'}
      <div class="guide-content cyber-card">
        <div class="guide-intro">
          <div class="intro-badges">
            <span class="badge badge-cyan">ESP-IDF v5.5 Nativo</span>
            <span class="badge badge-purple">Puerto: /dev/ttyACM0</span>
          </div>
          <h3>Instalación en Guition JC4880P443C (ESP32-P4 Rev 1.3)</h3>
          <p>Asegúrate de conectar el cable USB-C al puerto rotulado <strong>USB-Serial / JTAG</strong> de la placa.</p>
        </div>

        <div class="steps-list">
          <div class="step-item">
            <div class="step-num">1</div>
            <div class="step-body">
              <h4>Exportar las variables de entorno de ESP-IDF</h4>
              <div class="step-code">
                <code>. $HOME/esp/esp-idf/export.sh</code>
                <button class="step-copy" onclick={() => copyText('p4-1', '. $HOME/esp/esp-idf/export.sh')}>
                  {copiedStep === 'p4-1' ? '✓' : 'Copiar'}
                </button>
              </div>
            </div>
          </div>

          <div class="step-item">
            <div class="step-num">2</div>
            <div class="step-body">
              <h4>Navegar al BSP de la placa y compilar</h4>
              <div class="step-code">
                <code>cd bsp/esp32_p4_jc4880 && idf.py build</code>
                <button class="step-copy" onclick={() => copyText('p4-2', 'cd bsp/esp32_p4_jc4880 && idf.py build')}>
                  {copiedStep === 'p4-2' ? '✓' : 'Copiar'}
                </button>
              </div>
            </div>
          </div>

          <div class="step-item">
            <div class="step-num">3</div>
            <div class="step-body">
              <h4>Flashear el firmware y abrir el monitor serie</h4>
              <div class="step-code">
                <code>idf.py -p /dev/ttyACM0 flash monitor</code>
                <button class="step-copy" onclick={() => copyText('p4-3', 'idf.py -p /dev/ttyACM0 flash monitor')}>
                  {copiedStep === 'p4-3' ? '✓' : 'Copiar'}
                </button>
              </div>
            </div>
          </div>
        </div>
      </div>
    {/if}

    <!-- S3 Guide -->
    {#if activeGuide === 's3'}
      <div class="guide-content cyber-card">
        <div class="guide-intro">
          <div class="intro-badges">
            <span class="badge badge-purple">PlatformIO / pioarduino</span>
            <span class="badge badge-cyan">Puerto: /dev/ttyACM0</span>
          </div>
          <h3>Instalación en Guition JC3248W535 (ESP32-S3)</h3>
          <p>Compilación mediante PlatformIO usando el núcleo moderno pioarduino para soporte LVGL 9.5.</p>
        </div>

        <div class="steps-list">
          <div class="step-item">
            <div class="step-num">1</div>
            <div class="step-body">
              <h4>Compilar el proyecto de PlatformIO</h4>
              <div class="step-code">
                <code>pio run -d bsp/esp32_s3_jc3248</code>
                <button class="step-copy" onclick={() => copyText('s3-1', 'pio run -d bsp/esp32_s3_jc3248')}>
                  {copiedStep === 's3-1' ? '✓' : 'Copiar'}
                </button>
              </div>
            </div>
          </div>

          <div class="step-item">
            <div class="step-num">2</div>
            <div class="step-body">
              <h4>Cargar el binario a través del puerto serie</h4>
              <div class="step-code">
                <code>pio run -d bsp/esp32_s3_jc3248 -t upload --upload-port /dev/ttyACM0</code>
                <button class="step-copy" onclick={() => copyText('s3-2', 'pio run -d bsp/esp32_s3_jc3248 -t upload --upload-port /dev/ttyACM0')}>
                  {copiedStep === 's3-2' ? '✓' : 'Copiar'}
                </button>
              </div>
            </div>
          </div>

          <div class="step-item">
            <div class="step-num">3</div>
            <div class="step-body">
              <h4>Monitorear la salida de depuración</h4>
              <div class="step-code">
                <code>pio device monitor -d bsp/esp32_s3_jc3248 -b 115200</code>
                <button class="step-copy" onclick={() => copyText('s3-3', 'pio device monitor -d bsp/esp32_s3_jc3248 -b 115200')}>
                  {copiedStep === 's3-3' ? '✓' : 'Copiar'}
                </button>
              </div>
            </div>
          </div>
        </div>
      </div>
    {/if}

    <!-- Coproc Guide -->
    {#if activeGuide === 'coproc'}
      <div class="guide-content cyber-card">
        <div class="guide-intro">
          <div class="intro-badges">
            <span class="badge badge-green">Flasher de Campo</span>
            <span class="badge badge-amber">MicroSD Direct</span>
          </div>
          <h3>Flasheo Autónomo del Coprocesador ESP32-C6</h3>
          <p>CBDos puede programar el módulo de radio C6 conectado internamente sin necesidad de desconectarlo ni usar herramientas externas.</p>
        </div>

        <div class="diagram-center">
          <img src="images/esp32_c6_flasher_diagram.png" alt="Diagrama Flasher Coprocesador C6" class="guide-img" />
        </div>

        <div class="step-item" style="margin-top: 1.5rem;">
          <div class="step-num">✓</div>
          <div class="step-body">
            <h4>Procedimiento desde CBDos</h4>
            <p style="font-size: 0.9rem; color: var(--text-muted); line-height: 1.6;">
              1. Copia el binario <code>esp_hosted_c6.bin</code> a <code>/sdcard/firmwares/</code>.<br/>
              2. Abre la aplicación <strong>Flasher</strong> desde el menú de CBDos.<br/>
              3. Selecciona <em>"Target: ESP32-C6 (Internal SDIO)"</em> y presiona <em>"Flashear Coprocesador"</em>.<br/>
              4. CBDos conmuta las líneas de BOOT/RST y escribe el binario en menos de 15 segundos.
            </p>
          </div>
        </div>
      </div>
    {/if}
  </div>
</section>

<style>
  .flashing-section {
    padding: 5rem 0;
    position: relative;
  }

  .guide-toggle {
    display: flex;
    justify-content: center;
    gap: 0.8rem;
    margin-bottom: 2.5rem;
    flex-wrap: wrap;
  }

  .toggle-btn {
    padding: 0.7rem 1.4rem;
    background: rgba(14, 20, 32, 0.6);
    border: 1px solid var(--border-subtle);
    border-radius: var(--radius-md);
    color: var(--text-muted);
    font-size: 0.9rem;
    font-weight: 600;
    cursor: pointer;
    transition: all 0.2s;
  }

  .toggle-btn:hover {
    color: var(--text-main);
    border-color: var(--border-cyan);
  }

  .toggle-btn.active {
    background: var(--cyan-dim);
    border-color: var(--cyan-core);
    color: var(--cyan-core);
    box-shadow: 0 0 20px var(--cyan-glow);
  }

  .guide-content {
    padding: 2.5rem;
    max-width: 900px;
    margin: 0 auto;
  }

  .guide-intro {
    margin-bottom: 2rem;
    padding-bottom: 1.5rem;
    border-bottom: 1px solid var(--border-subtle);
  }

  .intro-badges {
    display: flex;
    gap: 0.5rem;
    margin-bottom: 0.8rem;
  }

  .guide-intro h3 {
    font-size: 1.5rem;
    font-weight: 700;
    margin-bottom: 0.5rem;
  }

  .guide-intro p {
    color: var(--text-muted);
    font-size: 0.95rem;
  }

  .steps-list {
    display: flex;
    flex-direction: column;
    gap: 1.5rem;
  }

  .step-item {
    display: flex;
    gap: 1.2rem;
    align-items: flex-start;
  }

  .step-num {
    width: 32px;
    height: 32px;
    border-radius: 50%;
    background: var(--cyan-dim);
    border: 1px solid var(--cyan-core);
    color: var(--cyan-core);
    display: flex;
    align-items: center;
    justify-content: center;
    font-family: var(--font-mono);
    font-weight: 700;
    font-size: 0.9rem;
    flex-shrink: 0;
  }

  .step-body {
    flex: 1;
  }

  .step-body h4 {
    font-size: 1rem;
    font-weight: 600;
    margin-bottom: 0.6rem;
    color: var(--text-main);
  }

  .step-code {
    display: flex;
    align-items: center;
    justify-content: space-between;
    background: rgba(6, 8, 12, 0.9);
    border: 1px solid var(--border-subtle);
    border-radius: var(--radius-sm);
    padding: 0.6rem 1rem;
    font-family: var(--font-mono);
    font-size: 0.85rem;
  }

  .step-code code {
    color: var(--cyan-core);
    word-break: break-all;
  }

  .step-copy {
    background: rgba(255, 255, 255, 0.05);
    border: 1px solid var(--border-subtle);
    border-radius: 4px;
    color: var(--text-muted);
    font-size: 0.75rem;
    padding: 0.25rem 0.6rem;
    cursor: pointer;
    transition: all 0.2s;
    margin-left: 0.8rem;
    white-space: nowrap;
  }

  .step-copy:hover {
    border-color: var(--cyan-core);
    color: var(--cyan-core);
  }

  .diagram-center {
    text-align: center;
    margin: 1.5rem 0;
  }

  .guide-img {
    max-width: 100%;
    height: auto;
    border-radius: var(--radius-md);
    border: 1px solid var(--border-subtle);
  }
</style>
