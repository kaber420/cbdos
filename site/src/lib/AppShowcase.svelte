<script lang="ts">
  type Category = 'all' | 'system' | 'usb' | 'media' | 'security';

  let selectedCategory = $state<Category>('all');
  let searchQuery = $state('');

  interface AppItem {
    id: string;
    title: string;
    category: Category;
    catLabel: string;
    icon: string;
    desc: string;
    tech: string[];
    isNative: boolean;
    hasLuaApi: boolean;
  }

  const apps: AppItem[] = [
    {
      id: 'flasher',
      title: 'Flasheador Autónomo',
      category: 'system',
      catLabel: 'Sistema',
      icon: '⚡',
      desc: 'Convierte el dispositivo en un programador autónomo. Flashea microcontroladores externos (ESP32-C3, ESP32-C6, RP2040) por USB-OTG o UART directamente desde la MicroSD.',
      tech: ['USB-OTG Host', 'UART Driver', 'SDMMC FATFS'],
      isNative: true,
      hasLuaApi: true
    },
    {
      id: 'filemanager',
      title: 'Explorador de Archivos',
      category: 'system',
      catLabel: 'Sistema',
      icon: '📁',
      desc: 'Navegación veloz por el sistema de ficheros FAT32/exFAT. Permite inspeccionar carpetas, lanzar archivos de configuración y ejecutar micro-apps .luapp.',
      tech: ['SDMMC 4-bit', 'FATFS Virtual File System', 'LDO VO4'],
      isNative: true,
      hasLuaApi: true
    },
    {
      id: 'texteditor',
      title: 'Editor de Texto & Código',
      category: 'system',
      catLabel: 'Sistema',
      icon: '📝',
      desc: 'Edición directa de ficheros de configuración, logs y scripts en la tarjeta MicroSD con teclado táctil en pantalla o teclado físico USB.',
      tech: ['LVGL Textarea', 'MicroSD VFS', 'Teclado Virtual'],
      isNative: true,
      hasLuaApi: true
    },
    {
      id: 'terminal',
      title: 'Terminal & Consola Interactiva',
      category: 'system',
      catLabel: 'Sistema',
      icon: '💻',
      desc: 'Shell interactivo local para inspección del sistema de archivos, ejecución de comandos del sistema y pruebas en tiempo real.',
      tech: ['ANSI Stream', 'CLI Parser', 'Zero-Poll'],
      isNative: true,
      hasLuaApi: true
    },
    {
      id: 'badusb',
      title: 'BadUSB & DuckyScript Runner',
      category: 'usb',
      catLabel: 'USB & HID',
      icon: '⌨️',
      desc: 'Emulador HID USB de alta velocidad. Inyección de pulsaciones de teclado a partir de scripts DuckyScript v2 con consola serie reactiva.',
      tech: ['USB Composite HID', 'DuckyScript v2', 'Zero-Poll'],
      isNative: true,
      hasLuaApi: true
    },
    {
      id: 'audioplayer',
      title: 'Reproductor Helix MP3',
      category: 'media',
      catLabel: 'Multimedia & Juegos',
      icon: '🎵',
      desc: 'Reproductor de música con decodificador Helix MP3 de punto fijo corriendo en Core 1, códec Everest ES8311 por I2S y barra de espectro FFT reactiva.',
      tech: ['Helix Fixed-Point', 'Everest ES8311', 'I2S DMA'],
      isNative: true,
      hasLuaApi: true
    },
    {
      id: 'audiorecorder',
      title: 'Grabadora de Audio I2S',
      category: 'media',
      catLabel: 'Multimedia & Juegos',
      icon: '🎙️',
      desc: 'Captura y grabación de audio en tiempo real directamente a la MicroSD utilizando el códec Everest ES8311 a través de canales DMA.',
      tech: ['Everest ES8311', 'I2S DMA ADC', 'WAV/PCM'],
      isNative: true,
      hasLuaApi: true
    },
    {
      id: 'gallery',
      title: 'Galería & Visor de Imágenes',
      category: 'media',
      catLabel: 'Multimedia & Juegos',
      icon: '🖼️',
      desc: 'Visor de imágenes con vista de miniaturas y navegación a pantalla completa desde la tarjeta MicroSD con aceleración DMA2D.',
      tech: ['DMA2D Blit', 'Decodificador PNG/BMP', 'Touch Gestures'],
      isNative: true,
      hasLuaApi: false
    },
    {
      id: 'doom',
      title: 'DOOM WAD & Retro Gaming',
      category: 'media',
      catLabel: 'Multimedia & Juegos',
      icon: '👾',
      desc: 'Port optimizado del clásico DOOM sobre el framebuffer acelerado de LVGL 9.5 con soporte de audio sintetizado y controles táctiles o USB.',
      tech: ['DOOM.WAD Reader', 'DMA2D Blit', '60 FPS'],
      isNative: true,
      hasLuaApi: false
    },
    {
      id: 'Kerberos',
      title: 'Kerberos Passkey FIDO2',
      category: 'security',
      catLabel: 'Seguridad',
      icon: '🛡️',
      desc: 'Módulo de autenticación criptográfica de dos factores (U2F/FIDO2) con almacenamiento seguro en partición NVS cifrada y confirmación física.',
      tech: ['FIDO2 / U2F', 'NVS Encryption', 'AES-256'],
      isNative: true,
      hasLuaApi: false
    }
  ];

  let filteredApps = $derived(
    apps.filter(app => {
      const matchCat = selectedCategory === 'all' || app.category === selectedCategory;
      const matchSearch = searchQuery.trim() === '' || 
        app.title.toLowerCase().includes(searchQuery.toLowerCase()) ||
        app.desc.toLowerCase().includes(searchQuery.toLowerCase()) ||
        app.tech.some(t => t.toLowerCase().includes(searchQuery.toLowerCase()));
      return matchCat && matchSearch;
    })
  );
</script>

<section id="apps" class="apps-section">
  <div class="container">
    <div class="section-header">
      <span class="section-tag">// CATÁLOGO DE HERRAMIENTAS</span>
      <h2 class="section-title">Aplicaciones Integradas</h2>
      <p class="section-desc">
        CBDos incluye una suite completa de aplicaciones nativas en C++ con interfaces LVGL v9.5, extensibles mediante la API de scripts Lua++.
      </p>
    </div>

    <!-- Search and Filters -->
    <div class="filter-bar">
      <div class="search-box">
        <svg viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" stroke-width="2">
          <circle cx="11" cy="11" r="8"></circle>
          <line x1="21" y1="21" x2="16.65" y2="16.65"></line>
        </svg>
        <input 
          type="text" 
          placeholder="Buscar aplicación, protocolo o tecnología..." 
          bind:value={searchQuery}
        />
        {#if searchQuery}
          <button class="clear-btn" onclick={() => searchQuery = ''}>×</button>
        {/if}
      </div>

      <div class="cat-chips">
        <button class="chip" class:active={selectedCategory === 'all'} onclick={() => selectedCategory = 'all'}>
          Todas ({apps.length})
        </button>
        <button class="chip" class:active={selectedCategory === 'system'} onclick={() => selectedCategory = 'system'}>
          Sistema
        </button>
        <button class="chip" class:active={selectedCategory === 'usb'} onclick={() => selectedCategory = 'usb'}>
          USB & HID
        </button>
        <button class="chip" class:active={selectedCategory === 'media'} onclick={() => selectedCategory = 'media'}>
          Multimedia
        </button>
        <button class="chip" class:active={selectedCategory === 'security'} onclick={() => selectedCategory = 'security'}>
          Seguridad
        </button>
      </div>
    </div>

    <!-- App Cards Grid -->
    <div class="apps-grid">
      {#each filteredApps as app (app.id)}
        <div class="app-card cyber-card">
          <div class="app-top">
            <span class="app-icon">{app.icon}</span>
            <div class="app-badges">
              {#if app.isNative}
                <span class="badge badge-cyan">C++ Nativo</span>
              {/if}
              {#if app.hasLuaApi}
                <span class="badge badge-purple">Lua++ API</span>
              {/if}
            </div>
          </div>

          <div class="app-cat-label">{app.catLabel}</div>
          <h3 class="app-title">{app.title}</h3>
          <p class="app-desc">{app.desc}</p>

          <div class="app-tech-list">
            {#each app.tech as t}
              <span class="tech-tag">{t}</span>
            {/each}
          </div>
        </div>
      {/each}
    </div>

    {#if filteredApps.length === 0}
      <div class="empty-state cyber-card">
        <span class="empty-icon">🔍</span>
        <h3>No se encontraron aplicaciones</h3>
        <p>Prueba con otros términos de búsqueda o selecciona la categoría "Todas".</p>
      </div>
    {/if}
  </div>
</section>

<style>
  .apps-section {
    padding: 5rem 0;
    position: relative;
  }

  .filter-bar {
    display: flex;
    flex-direction: column;
    gap: 1.2rem;
    margin-bottom: 2.5rem;
  }

  .search-box {
    display: flex;
    align-items: center;
    gap: 0.8rem;
    background: rgba(14, 20, 32, 0.7);
    border: 1px solid var(--border-subtle);
    border-radius: var(--radius-md);
    padding: 0.8rem 1.2rem;
    color: var(--text-muted);
    transition: border-color 0.2s;
  }

  .search-box:focus-within {
    border-color: var(--cyan-core);
    box-shadow: 0 0 20px var(--cyan-glow);
  }

  .search-box input {
    background: transparent;
    border: none;
    outline: none;
    color: var(--text-main);
    font-size: 1rem;
    width: 100%;
    font-family: var(--font-sans);
  }

  .search-box input::placeholder {
    color: var(--text-dim);
  }

  .clear-btn {
    background: transparent;
    border: none;
    color: var(--text-muted);
    font-size: 1.3rem;
    cursor: pointer;
  }

  .cat-chips {
    display: flex;
    gap: 0.6rem;
    overflow-x: auto;
    padding-bottom: 0.4rem;
  }

  .chip {
    padding: 0.45rem 1rem;
    background: rgba(15, 23, 42, 0.6);
    border: 1px solid var(--border-subtle);
    border-radius: 20px;
    color: var(--text-muted);
    font-size: 0.85rem;
    font-weight: 600;
    cursor: pointer;
    transition: all 0.2s;
    white-space: nowrap;
  }

  .chip:hover {
    border-color: var(--border-cyan);
    color: var(--text-main);
  }

  .chip.active {
    background: var(--cyan-dim);
    border-color: var(--cyan-core);
    color: var(--cyan-core);
    box-shadow: 0 0 15px var(--cyan-glow);
  }

  .apps-grid {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(320px, 1fr));
    gap: 1.8rem;
  }

  .app-card {
    padding: 1.8rem;
    display: flex;
    flex-direction: column;
  }

  .app-top {
    display: flex;
    align-items: center;
    justify-content: space-between;
    margin-bottom: 0.8rem;
  }

  .app-icon {
    font-size: 2rem;
  }

  .app-badges {
    display: flex;
    gap: 0.4rem;
  }

  .app-cat-label {
    font-family: var(--font-mono);
    font-size: 0.72rem;
    color: var(--cyan-core);
    text-transform: uppercase;
    letter-spacing: 0.05em;
    margin-bottom: 0.4rem;
  }

  .app-title {
    font-size: 1.25rem;
    font-weight: 700;
    color: var(--text-main);
    margin-bottom: 0.75rem;
  }

  .app-desc {
    font-size: 0.9rem;
    color: var(--text-muted);
    line-height: 1.6;
    flex: 1;
    margin-bottom: 1.5rem;
  }

  .app-tech-list {
    display: flex;
    flex-wrap: wrap;
    gap: 0.4rem;
  }

  .tech-tag {
    font-family: var(--font-mono);
    font-size: 0.72rem;
    padding: 0.2rem 0.5rem;
    background: rgba(255, 255, 255, 0.04);
    border: 1px solid rgba(255, 255, 255, 0.08);
    border-radius: 4px;
    color: var(--text-dim);
  }

  .empty-state {
    padding: 3rem;
    text-align: center;
    margin-top: 2rem;
  }

  .empty-icon {
    font-size: 2.5rem;
    margin-bottom: 1rem;
    display: block;
  }

  .empty-state h3 {
    font-size: 1.25rem;
    margin-bottom: 0.5rem;
  }

  .empty-state p {
    color: var(--text-muted);
  }
</style>
