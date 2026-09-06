<script lang="ts">
  const pillars = [
    {
      icon: '🖥️',
      title: 'UI Cyberpunk & 60 FPS',
      tag: 'LVGL v9.5 + DMA2D',
      desc: 'Motor gráfico de alta tasa de refresco, aceleración por hardware 2D (PPA / DMA2D) en ESP32-P4, widgets vectoriales táctiles y animaciones Lottie fluidas.',
      link: '#apps'
    },
    {
      icon: '🔲',
      title: 'Multi-Target Desacoplado',
      tag: 'ESP32-P4 & ESP32-S3',
      desc: 'Arquitectura pura en C++ agnóstico (core/) sin contaminación de frameworks. Compilación simultánea y limpia para ESP-IDF 5.5 y PlatformIO Arduino.',
      link: '#hardware'
    },
    {
      icon: '💻',
      title: 'Ecosistema Lua++ (.luapp)',
      tag: 'Sandboxed Micro-Apps',
      desc: 'Carga y ejecuta micro-aplicaciones dinámicas desde la tarjeta MicroSD sin recompilar firmware. Acceso seguro a pantalla, táctil, audio y radio.',
      link: '#luapp'
    },
    {
      icon: '📡',
      title: 'Redes Mesh & Modo Sigilo',
      tag: 'ESP-NOW + Paquetes TLV',
      desc: 'Comunicaciones directas P2P fuera de internet, descubrimiento de nodos con Short IDs dinámicos y cifrado de transmisiones de radio.',
      link: '#apps'
    },
    {
      icon: '⚡',
      title: 'Flasheador de Campo Autónomo',
      tag: 'USB-OTG & UART Programmer',
      desc: 'Convierte el cyberdeck en un programador físico: flashea microcontroladores externos (ESP32-C3, C6, RP2040) desde la MicroSD sin ordenador.',
      link: '#flashing'
    },
    {
      icon: '🛡️',
      title: 'Seguridad FIDO2 & BadUSB',
      tag: 'Hardware Security Module',
      desc: 'Emulador HID USB para payloads DuckyScript, autenticación con token de seguridad, cifrado simétrico y aislamiento de secretos.',
      link: '#apps'
    }
  ];
</script>

<section id="pilares" class="pillars-section">
  <div class="container">
    <div class="section-header">
      <span class="section-tag">// ARQUITECTURA & FILOSOFÍA</span>
      <h2 class="section-title">Pilares Tecnológicos</h2>
      <p class="section-desc">
        Diseñado bajo dos directrices innegociables: <strong>Offline-First Estricto</strong> (100% autónomo sin internet) y <strong>Arquitectura Reactiva</strong> (Cero bucles de sondeo innecesarios).
      </p>
    </div>

    <div class="pillars-grid">
      {#each pillars as p}
        <div class="pillar-card cyber-card">
          <div class="card-top">
            <span class="pillar-icon">{p.icon}</span>
            <span class="badge badge-cyan">{p.tag}</span>
          </div>
          <h3 class="pillar-title">{p.title}</h3>
          <p class="pillar-desc">{p.desc}</p>
          <a href={p.link} class="pillar-link">
            <span>Saber más</span>
            <svg viewBox="0 0 24 24" width="16" height="16" fill="none" stroke="currentColor" stroke-width="2">
              <line x1="5" y1="12" x2="19" y2="12"></line>
              <polyline points="12 5 19 12 12 19"></polyline>
            </svg>
          </a>
        </div>
      {/each}
    </div>
  </div>
</section>

<style>
  .pillars-section {
    padding: 5rem 0;
    position: relative;
  }

  .pillars-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(340px, 1fr));
    gap: 1.8rem;
  }

  .pillar-card {
    padding: 2rem;
    display: flex;
    flex-direction: column;
    height: 100%;
  }

  .card-top {
    display: flex;
    align-items: center;
    justify-content: space-between;
    margin-bottom: 1.2rem;
  }

  .pillar-icon {
    font-size: 2rem;
  }

  .pillar-title {
    font-size: 1.35rem;
    font-weight: 700;
    margin-bottom: 0.8rem;
    color: var(--text-main);
  }

  .pillar-desc {
    font-size: 0.95rem;
    color: var(--text-muted);
    line-height: 1.6;
    flex: 1;
    margin-bottom: 1.5rem;
  }

  .pillar-link {
    display: inline-flex;
    align-items: center;
    gap: 0.4rem;
    font-size: 0.85rem;
    font-weight: 700;
    color: var(--cyan-core);
    transition: gap 0.2s, text-shadow 0.2s;
  }

  .pillar-link:hover {
    gap: 0.7rem;
    text-shadow: 0 0 8px var(--cyan-glow);
  }
</style>
