<footer class="footer-wrapper">
  <div class="container">
    <div class="footer-grid">
      <div class="footer-brand-col">
        <div class="footer-brand">
          <span class="brand-name">CBD<span class="brand-accent">os</span></span>
          <span class="brand-badge">v0.2.3</span>
        </div>
        <p class="footer-tagline">
          El sistema operativo embebido y offline-first para cyberdecks, consolas portátiles y hardware táctico ESP32.
        </p>
        <div class="footer-badges">
          <span class="badge badge-cyan">ESP32-P4 RISC-V</span>
          <span class="badge badge-purple">LVGL 9.5</span>
          <span class="badge badge-green">GPL-3.0</span>
        </div>
      </div>

      <div class="footer-links-col">
        <h4>Navegación</h4>
        <ul>
          <li><a href="#hero">Inicio</a></li>
          <li><a href="#pilares">Pilares Tecnológicos</a></li>
          <li><a href="#hardware">Hardware & Placas</a></li>
          <li><a href="#apps">Catálogo de Apps</a></li>
          <li><a href="#luapp">Ecosistema Lua++</a></li>
          <li><a href="#flashing">Guía de Flasheo</a></li>
          <li><a href="#manuales">Manuales</a></li>
        </ul>
      </div>

      <div class="footer-links-col">
        <h4>Ingeniería & Specs</h4>
        <ul>
          <li><a href="https://github.com/kaber420/CBD-os/tree/main/specs" target="_blank" rel="noopener noreferrer">Especificaciones Técnicas</a></li>
          <li><a href="https://github.com/kaber420/CBD-os/blob/main/specs/hardware/pinouts_and_ports.md" target="_blank" rel="noopener noreferrer">Mapa de Pines & Buses</a></li>
          <li><a href="https://github.com/kaber420/CBD-os/blob/main/specs/architecture/hal_and_core_architecture.md" target="_blank" rel="noopener noreferrer">Arquitectura HAL</a></li>
          <li><a href="https://github.com/kaber420/CBD-os/blob/main/specs/api/core_apis_reference.md" target="_blank" rel="noopener noreferrer">Referencia de APIs</a></li>
        </ul>
      </div>

      <div class="footer-links-col">
        <h4>Código & Comunidad</h4>
        <ul>
          <li><a href="https://github.com/kaber420/CBD-os" target="_blank" rel="noopener noreferrer">Repositorio GitHub</a></li>
          <li><a href="https://github.com/kaber420/CBD-os/tree/v0.2.3-dev" target="_blank" rel="noopener noreferrer">Rama de Desarrollo (v0.2.3-dev)</a></li>
          <li><a href="https://github.com/kaber420/CBD-os/issues" target="_blank" rel="noopener noreferrer">Reportar un Bug</a></li>
          <li><a href="https://github.com/kaber420/CBD-os/blob/main/LICENSE" target="_blank" rel="noopener noreferrer">Licencia GPLv3</a></li>
        </ul>
      </div>
    </div>

    <div class="footer-bottom">
      <div class="footer-copyright">
        © 2026 <strong>CyBerDeck OS</strong>. Diseñado con pasión Cyberpunk.
      </div>
      <div class="footer-extra">
        Compilación Estática Svelte • Despliegue Directo GitHub Pages
      </div>
    </div>
  </div>
</footer>

<style>
  .footer-wrapper {
    background: #040508;
    border-top: 1px solid var(--border-subtle);
    padding: 4.5rem 0 2rem;
    position: relative;
  }

  .footer-wrapper::before {
    content: '';
    position: absolute;
    top: 0;
    left: 20%;
    right: 20%;
    height: 1px;
    background: linear-gradient(90deg, transparent, var(--cyan-core), transparent);
    box-shadow: 0 0 10px var(--cyan-glow);
  }

  .footer-grid {
    display: grid;
    grid-template-columns: 1.6fr 1fr 1fr 1fr;
    gap: 3rem;
    margin-bottom: 3.5rem;
  }

  .footer-brand {
    display: flex;
    align-items: center;
    gap: 0.5rem;
    margin-bottom: 1rem;
  }

  .brand-name {
    font-size: 1.6rem;
    font-weight: 800;
    color: var(--text-main);
  }

  .brand-accent {
    color: var(--cyan-core);
  }

  .brand-badge {
    font-family: var(--font-mono);
    font-size: 0.7rem;
    padding: 0.15rem 0.5rem;
    background: var(--purple-dim);
    border: 1px solid var(--border-purple);
    color: #c084fc;
    border-radius: 4px;
  }

  .footer-tagline {
    font-size: 0.95rem;
    color: var(--text-muted);
    line-height: 1.6;
    margin-bottom: 1.2rem;
  }

  .footer-badges {
    display: flex;
    flex-wrap: wrap;
    gap: 0.5rem;
  }

  .footer-links-col h4 {
    font-size: 0.95rem;
    font-weight: 700;
    color: var(--text-main);
    margin-bottom: 1.2rem;
    letter-spacing: 0.02em;
  }

  .footer-links-col ul {
    list-style: none;
    display: flex;
    flex-direction: column;
    gap: 0.75rem;
  }

  .footer-links-col a {
    font-size: 0.88rem;
    color: var(--text-muted);
    transition: all 0.2s;
  }

  .footer-links-col a:hover {
    color: var(--cyan-core);
    padding-left: 4px;
  }

  .footer-bottom {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding-top: 2rem;
    border-top: 1px solid rgba(255, 255, 255, 0.05);
    font-size: 0.82rem;
    color: var(--text-dim);
    flex-wrap: wrap;
    gap: 1rem;
  }

  .footer-copyright strong {
    color: var(--text-main);
  }

  @media (max-width: 900px) {
    .footer-grid {
      grid-template-columns: 1fr 1fr;
      gap: 2rem;
    }
  }

  @media (max-width: 600px) {
    .footer-grid {
      grid-template-columns: 1fr;
    }
  }
</style>
