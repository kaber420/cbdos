<script lang="ts">
  type Category = 'all' | 'system' | 'radio' | 'media' | 'security';

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
      title: 'Flasheador de Campo Autónomo',
      category: 'system',
      catLabel: 'Sistema & Campo',
      icon: '⚡',
      desc: 'Convierte el Cyberdeck en un programador de microcontroladores autónomo. Flashea chips ESP32-C3, ESP32-C6 y RP2040 por USB-OTG o UART desde la MicroSD.',
      tech: ['USB-OTG Host', 'UART Driver', 'SDMMC FATFS'],
      isNative: true,
      hasLuaApi: true
    },
    {
      id: 'meshchat',
      title: 'MeshChat P2P Cifrado',
      category: 'radio',
      catLabel: 'Radio & Mesh',
      icon: '📡',
      desc: 'Comunicaciones tácticas directas fuera de internet. Protocolo ad-hoc sobre ESP-NOW con paquetes TLV, Short IDs dinámicos y cifrado de canal.',
      tech: ['ESP-NOW 2.4GHz', 'Paquetes TLV', 'Aislamiento RF'],
      isNative: true,
      hasLuaApi: true
    },
    {
      id: 'badusb',
      title: 'BadUSB & DuckyScript Runner',
      category: 'security',
      catLabel: 'BadUSB & Seguridad',
      icon: '⌨️',
      desc: 'Emulador HID USB de alta velocidad. Inyección de keystrokes a partir de scripts DuckyScript v2 con consola serie reactiva (/dev/ttyACM0).',
      tech: ['USB Composite HID', 'DuckyScript v2', 'Zero-Poll'],
      isNative: true,
      hasLuaApi: true
    },
    {
      id: 'audioplayer',
      title: 'Helix MP3 & Visualizador FFT',
      category: 'media',
      catLabel: 'Multimedia & Juegos',
      icon: '🎵',
      desc: 'Reproductor de música de alta fidelidad con decodificador Helix MP3 de punto fijo corriendo en Core 1, codec Everest ES8311 y barra de espectro FFT.',
      tech: ['Helix Fixed-Point', 'Everest ES8311', 'I2S DMA'],
      isNative: true,
      hasLuaApi: true
    },
    {
      id: 'filemanager',
      title: 'Explorador MicroSD & Launcher',
      category: 'system',
      catLabel: 'Sistema & Campo',
      icon: '📁',
      desc: 'Navegación veloz por el sistema de ficheros FAT32/exFAT. Permite inspeccionar logs, lanzar archivos de configuración y ejecutar micro-apps .luapp.',
      tech: ['SDMMC 4-bit', 'FATFS Virtual File System', 'LDO VO4'],
      isNative: true,
      hasLuaApi: true
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
      id: 'fido2',
      title: 'Token FIDO2 & Seguridad Kerberos',
      category: 'security',
      catLabel: 'BadUSB & Seguridad',
      icon: '🛡️',
      desc: 'Módulo de autenticación criptográfica de dos factores (U2F/FIDO2) con almacenamiento seguro en partición NVS cifrada y confirmación física.',
      tech: ['FIDO2 / U2F', 'NVS Encryption', 'AES-256'],
      isNative: true,
      hasLuaApi: false
    },
    {
      id: 'sniffer',
      title: 'RF Packet Sniffer & Spectrum',
      category: 'radio',
      catLabel: 'Radio & Mesh',
      icon: '📶',
      desc: 'Monitor pasivo de tramas de radio Wi-Fi 802.11 y balizas Bluetooth Low Energy con analizador de intensidad RSSI en tiempo real.',
      tech: ['Promiscuous Mode', 'BLE Scanner', 'Visualizador RSSI'],
      isNative: true,
      hasLuaApi: true
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
        <button class="chip" class:active={selectedCategory === 'radio'} onclick={() => selectedCategory = 'radio'}>
          Radio & Mesh
        </button>
        <button class="chip" class:active={selectedCategory === 'media'} onclick={() => selectedCategory = 'media'}>
          Multimedia
        </button>
        <button class="chip" class:active={selectedCategory === 'security'} onclick={() => selectedCategory = 'security'}>
          Seguridad & HID
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
