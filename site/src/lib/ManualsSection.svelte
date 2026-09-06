<script lang="ts">
  const manuals = [
    {
      id: 'index',
      title: 'Visión del Sistema & Arquitectura',
      file: 'manual/index.md',
      desc: 'Introducción a CBDos, arquitectura y capacidades del sistema (Vectores Lottie, TTS, Cartuchos OTA).',
      icon: '📖',
      tag: 'General'
    },
    {
      id: 'flashing',
      title: 'Guía de Flasheo e Instalación',
      file: 'manual/getting-started/flashing.md',
      desc: 'Instrucciones paso a paso para compilar y flashear en ESP32-P4 (ESP-IDF) y ESP32-S3 (PlatformIO).',
      icon: '⚡',
      tag: 'Instalación'
    },
    {
      id: 'hardware',
      title: 'Placas y Periféricos Soportados',
      file: 'manual/hardware/supported-boards.md',
      desc: 'Pinouts exactos, configuración de buses I2C/I2S, códec ES8311, táctil GT911 y pantalla ST7701S.',
      icon: '🔌',
      tag: 'Hardware'
    },
    {
      id: 'apps',
      title: 'Catálogo y Manual de Apps',
      file: 'manual/apps/overview.md',
      desc: 'Detalles operativos de Flasher, FileManager, AudioPlayer, BadUSB, TextEditor y Cartridge Manager.',
      icon: '🕹️',
      tag: 'Software'
    },
    {
      id: 'luapp',
      title: 'Guía de Desarrollo en Lua++',
      file: 'manual/developers/luapp.md',
      desc: 'Manual completo de APIs, sandbox en PSRAM y ejemplos para crear tus propias micro-apps .luapp.',
      icon: '💻',
      tag: 'Desarrollo'
    }
  ];
</script>

<section id="manuales" class="manuals-section">
  <div class="container">
    <div class="section-header">
      <span class="section-tag">// DOCUMENTACIÓN LIMPIA & PORTABLE</span>
      <h2 class="section-title">Manuales del Sistema</h2>
      <p class="section-desc">
        Documentación redactada en Markdown estándar y limpio dentro de la carpeta <code>manual/</code>, diseñada para leerse con <code>cat</code> o <code>glow</code> en la terminal, o directamente en CBDos.
      </p>
    </div>

    <div class="manuals-grid">
      {#each manuals as m}
        <div class="manual-card cyber-card">
          <div class="manual-top">
            <span class="manual-icon">{m.icon}</span>
            <span class="badge badge-purple">{m.tag}</span>
          </div>

          <h3 class="manual-title">{m.title}</h3>
          <p class="manual-desc">{m.desc}</p>

          <div class="manual-footer">
            <div class="manual-filepath">
              <code>{m.file}</code>
            </div>
            <a 
              href={`https://github.com/kaber420/CBD-os/blob/main/${m.file}`} 
              target="_blank" 
              rel="noopener noreferrer" 
              class="manual-action"
            >
              <span>Abrir en GitHub</span>
              <svg viewBox="0 0 24 24" width="14" height="14" fill="none" stroke="currentColor" stroke-width="2">
                <path d="M18 13v6a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V8a2 2 0 0 1 2-2h6"></path>
                <polyline points="15 3 21 3 21 9"></polyline>
                <line x1="10" y1="14" x2="21" y2="3"></line>
              </svg>
            </a>
          </div>
        </div>
      {/each}
    </div>
  </div>
</section>

<style>
  .manuals-section {
    padding: 5rem 0;
    position: relative;
  }

  .manuals-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(320px, 1fr));
    gap: 1.8rem;
  }

  .manual-card {
    padding: 1.8rem;
    display: flex;
    flex-direction: column;
    height: 100%;
  }

  .manual-top {
    display: flex;
    align-items: center;
    justify-content: space-between;
    margin-bottom: 1rem;
  }

  .manual-icon {
    font-size: 2rem;
  }

  .manual-title {
    font-size: 1.25rem;
    font-weight: 700;
    color: var(--text-main);
    margin-bottom: 0.6rem;
  }

  .manual-desc {
    font-size: 0.9rem;
    color: var(--text-muted);
    line-height: 1.6;
    flex: 1;
    margin-bottom: 1.5rem;
  }

  .manual-footer {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding-top: 1rem;
    border-top: 1px solid var(--border-subtle);
    flex-wrap: wrap;
    gap: 0.6rem;
  }

  .manual-filepath code {
    font-family: var(--font-mono);
    font-size: 0.75rem;
    color: var(--cyan-core);
  }

  .manual-action {
    display: inline-flex;
    align-items: center;
    gap: 0.35rem;
    font-size: 0.8rem;
    font-weight: 600;
    color: var(--text-muted);
    transition: color 0.2s;
  }

  .manual-action:hover {
    color: var(--cyan-core);
  }
</style>
