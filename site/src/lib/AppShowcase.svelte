<script lang="ts">
  type Category = 'all' | 'system' | 'usb' | 'media' | 'security';

  let selectedCategory = $state<Category>('all');
  let searchQuery = $state('');

  interface AppItem {
    id: string;
    title: string;
    category: Category;
    catLabel: string;
    desc: string;
    tech: string[];
    isNative: boolean;
    hasLuaApi: boolean;
  }

  // Iconos SVG profesionales (stroke 1.8, estilo lucide). Nada de emojis.
  const icons: Record<string, string> = {
    flasher: '<polygon points="13 2 3 14 12 14 11 22 21 10 12 10 13 2"/>',
    filemanager: '<path d="M22 19a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h9a2 2 0 0 1 2 2z"/>',
    texteditor: '<path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"/><polyline points="14 2 14 8 20 8"/><line x1="16" y1="13" x2="8" y2="13"/><line x1="16" y1="17" x2="8" y2="17"/>',
    terminal: '<polyline points="4 17 10 11 4 5"/><line x1="12" y1="19" x2="20" y2="19"/>',
    badusb: '<rect x="2" y="6" width="20" height="12" rx="2"/><line x1="6" y1="10" x2="6.01" y2="10"/><line x1="10" y1="10" x2="10.01" y2="10"/><line x1="14" y1="10" x2="14.01" y2="10"/><line x1="18" y1="10" x2="18.01" y2="10"/><line x1="7" y1="14" x2="17" y2="14"/>',
    audioplayer: '<path d="M9 18V5l12-2v13"/><circle cx="6" cy="18" r="3"/><circle cx="18" cy="16" r="3"/>',
    audiorecorder: '<rect x="9" y="2" width="6" height="12" rx="3"/><path d="M5 10a7 7 0 0 0 14 0"/><line x1="12" y1="19" x2="12" y2="22"/>',
    gallery: '<rect x="3" y="3" width="18" height="18" rx="2"/><circle cx="8.5" cy="8.5" r="1.5"/><polyline points="21 15 16 10 5 21"/>',
    webradio: '<circle cx="12" cy="12" r="2"/><path d="M4.93 19.07a10 10 0 0 1 0-14.14"/><path d="M7.76 16.24a6 6 0 0 1 0-8.49"/><path d="M16.24 7.76a6 6 0 0 1 0 8.49"/><path d="M19.07 4.93a10 10 0 0 1 0 14.14"/>',
    cartridges: '<rect x="2" y="3" width="20" height="7" rx="2"/><rect x="2" y="14" width="20" height="7" rx="2"/><line x1="6" y1="6.5" x2="6.01" y2="6.5"/><line x1="6" y1="17.5" x2="6.01" y2="17.5"/>',
    Kerberos: '<path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z"/>',
  };

  const apps: AppItem[] = [
    {
      id: 'flasher',
      title: 'Flasheador Autónomo',
      category: 'system',
      catLabel: 'Sistema',
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
      desc: 'Edición directa de ficheros de configuración, logs y scripts en la tarjeta MicroSD con teclado táctil en pantalla o teclado físico USB.',
      tech: ['LVGL Textarea', 'MicroSD VFS', 'Teclado Virtual'],
      isNative: true,
      hasLuaApi: true
    },
    {
      id: 'terminal',
      title: 'Terminal Serial & Cliente SSH',
      category: 'system',
      catLabel: 'Sistema',
      desc: 'Terminal gráfica con emulación ANSI. Permite interactuar por puerto serie (UART y USB CDC ACM) con microcontroladores externos, o conectarse remotamente como cliente SSH mediante libssh2.',
      tech: ['Serial UART / USB CDC', 'Cliente SSH (libssh2)', 'Emulador ANSI'],
      isNative: true,
      hasLuaApi: true
    },
    {
      id: 'badusb',
      title: 'BadUSB & DuckyScript Runner',
      category: 'usb',
      catLabel: 'USB & HID',
      desc: 'Emulador HID USB de alta velocidad. Inyección de pulsaciones de teclado a partir de scripts DuckyScript v2 con consola serie reactiva.',
      tech: ['USB Composite HID', 'DuckyScript v2', 'Zero-Poll'],
      isNative: true,
      hasLuaApi: true
    },
    {
      id: 'audioplayer',
      title: 'Reproductor Helix MP3',
      category: 'media',
      catLabel: 'Multimedia',
      desc: 'Reproductor de música con decodificador Helix MP3 de punto fijo corriendo en Core 1, códec Everest ES8311 por I2S y barra de espectro FFT reactiva.',
      tech: ['Helix Fixed-Point', 'Everest ES8311', 'I2S DMA'],
      isNative: true,
      hasLuaApi: true
    },
    {
      id: 'audiorecorder',
      title: 'Grabadora de Audio I2S',
      category: 'media',
      catLabel: 'Multimedia',
      desc: 'Captura y grabación de audio en tiempo real directamente a la MicroSD utilizando el códec Everest ES8311 a través de canales DMA.',
      tech: ['Everest ES8311', 'I2S DMA ADC', 'WAV/PCM'],
      isNative: true,
      hasLuaApi: true
    },
    {
      id: 'gallery',
      title: 'Galería & Visor de Imágenes',
      category: 'media',
      catLabel: 'Multimedia',
      desc: 'Visor de imágenes con vista de miniaturas y navegación a pantalla completa desde la tarjeta MicroSD con aceleración DMA2D.',
      tech: ['DMA2D Blit', 'Decodificador PNG/BMP', 'Touch Gestures'],
      isNative: true,
      hasLuaApi: false
    },
    {
      id: 'webradio',
      title: 'Radio Online & Streaming',
      category: 'media',
      catLabel: 'Multimedia',
      desc: 'Sintonizador de emisoras de radio por internet en streaming. Cuenta con buscador por tags y géneros, gestión de listas de reproducción y guardado de favoritos en la MicroSD.',
      tech: ['HTTP Audio Stream', 'Búsqueda por Tags', 'Playlists MicroSD'],
      isNative: true,
      hasLuaApi: true
    },
    {
      id: 'cartridges',
      title: 'Gestor de Cartuchos OTA',
      category: 'system',
      catLabel: 'Sistema',
      desc: 'Instalador y lanzador de cartuchos en slots OTA de la memoria Flash desde la MicroSD. Permite alternar y arrancar firmwares externos (como emuladores con soporte para gamepad) sin alterar el SO principal.',
      tech: ['Particiones OTA Flash', 'SDMMC Flasher', 'Dual-Boot Switch'],
      isNative: true,
      hasLuaApi: false
    },
    {
      id: 'Kerberos',
      title: 'Kerberos Passkey FIDO2',
      category: 'security',
      catLabel: 'Seguridad',
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
            <span class="app-icon">
              <svg viewBox="0 0 24 24" width="22" height="22" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">{@html icons[app.id] ?? ''}</svg>
            </span>
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
    width: 40px;
    height: 40px;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    color: var(--cyan-core);
    background: rgba(0, 240, 255, 0.08);
    border: 1px solid var(--border-cyan);
    border-radius: var(--radius-sm);
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

  .empty-state h3 {
    font-size: 1.25rem;
    margin-bottom: 0.5rem;
  }

  .empty-state p {
    color: var(--text-muted);
  }
</style>
