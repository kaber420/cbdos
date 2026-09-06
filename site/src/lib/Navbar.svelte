<script lang="ts">
  let mobileMenuOpen = $state(false);

  function toggleMobile() {
    mobileMenuOpen = !mobileMenuOpen;
  }
</script>

<header class="navbar-wrapper">
  <div class="container navbar-inner">
    <a href="#hero" class="brand">
      <div class="brand-icon">
        <svg viewBox="0 0 24 24" fill="none" class="icon-svg">
          <path d="M4 6h16M4 12h10M4 18h16" stroke="var(--cyan-core)" stroke-width="2.5" stroke-linecap="round"/>
          <circle cx="19" cy="12" r="2.5" fill="var(--pink-neon)"/>
        </svg>
      </div>
      <div class="brand-text">
        <span class="brand-name">CBD<span class="brand-accent">os</span></span>
        <span class="brand-version">v0.2.3</span>
      </div>
    </a>

    <nav class="nav-links" class:active={mobileMenuOpen}>
      <a href="#pilares" onclick={() => mobileMenuOpen = false}>Pilares</a>
      <a href="#hardware" onclick={() => mobileMenuOpen = false}>Hardware</a>
      <a href="#apps" onclick={() => mobileMenuOpen = false}>Apps</a>
      <a href="#luapp" onclick={() => mobileMenuOpen = false}>Lua++</a>
      <a href="#flashing" onclick={() => mobileMenuOpen = false}>Flasheo</a>
      <a href="#manuales" onclick={() => mobileMenuOpen = false}>Manuales</a>
    </nav>

    <div class="navbar-actions">
      <div class="live-status">
        <span class="pulse-dot"></span>
        <span class="status-text">ESP32-P4 400MHz</span>
      </div>
      <a href="https://github.com/kaber420/CBD-os" target="_blank" rel="noopener noreferrer" class="btn-github" aria-label="Repositorio GitHub">
        <svg viewBox="0 0 24 24" width="20" height="20" fill="currentColor">
          <path d="M12 0C5.37 0 0 5.37 0 12c0 5.31 3.435 9.795 8.205 11.385.6.105.825-.255.825-.57 0-.285-.015-1.23-.015-2.235-3.015.555-3.795-.735-4.035-1.41-.135-.345-.72-1.41-1.23-1.695-.42-.225-1.02-.78-.015-.795.945-.015 1.62.87 1.845 1.23 1.08 1.815 2.805 1.305 3.495.99.105-.78.42-1.305.765-1.605-2.67-.3-5.46-1.335-5.46-5.925 0-1.305.465-2.385 1.23-3.225-.12-.3-.54-1.53.12-3.18 0 0 1.005-.315 3.3 1.23.96-.27 1.98-.405 3-.405s2.04.135 3 .405c2.295-1.56 3.3-1.23 3.3-1.23.66 1.65.24 2.88.12 3.18.765.84 1.23 1.905 1.23 3.225 0 4.605-2.805 5.625-5.475 5.925.435.375.81 1.095.81 2.22 0 1.605-.015 2.895-.015 3.3 0 .315.225.69.825.57A12.02 12.02 0 0024 12c0-6.63-5.37-12-12-12z"/>
        </svg>
        <span>GitHub</span>
      </a>
      <button class="mobile-toggle" onclick={toggleMobile} aria-label="Menú">
        <svg viewBox="0 0 24 24" width="24" height="24" stroke="currentColor" stroke-width="2" fill="none">
          <line x1="3" y1="6" x2="21" y2="6"/>
          <line x1="3" y1="12" x2="21" y2="12"/>
          <line x1="3" y1="18" x2="21" y2="18"/>
        </svg>
      </button>
    </div>
  </div>
</header>

<style>
  .navbar-wrapper {
    position: sticky;
    top: 0;
    left: 0;
    right: 0;
    z-index: 1000;
    background: rgba(6, 7, 10, 0.85);
    backdrop-filter: blur(16px);
    -webkit-backdrop-filter: blur(16px);
    border-bottom: 1px solid var(--border-subtle);
  }

  .navbar-inner {
    display: flex;
    align-items: center;
    justify-content: space-between;
    height: 70px;
  }

  .brand {
    display: flex;
    align-items: center;
    gap: 0.75rem;
    font-weight: 800;
  }

  .brand-icon {
    width: 38px;
    height: 38px;
    display: flex;
    align-items: center;
    justify-content: center;
    background: rgba(0, 240, 255, 0.1);
    border: 1px solid var(--border-cyan);
    border-radius: var(--radius-sm);
  }

  .icon-svg {
    width: 22px;
    height: 22px;
  }

  .brand-name {
    font-size: 1.35rem;
    letter-spacing: -0.03em;
    color: var(--text-main);
  }

  .brand-accent {
    color: var(--cyan-core);
    text-shadow: 0 0 10px var(--cyan-glow);
  }

  .brand-version {
    font-family: var(--font-mono);
    font-size: 0.65rem;
    padding: 0.15rem 0.45rem;
    background: rgba(139, 92, 246, 0.2);
    border: 1px solid var(--border-purple);
    color: #c084fc;
    border-radius: 4px;
    margin-left: 0.4rem;
  }

  .nav-links {
    display: flex;
    align-items: center;
    gap: 1.8rem;
  }

  .nav-links a {
    font-size: 0.9rem;
    font-weight: 500;
    color: var(--text-muted);
    transition: color 0.2s, text-shadow 0.2s;
  }

  .nav-links a:hover {
    color: var(--cyan-core);
    text-shadow: 0 0 8px var(--cyan-glow);
  }

  .navbar-actions {
    display: flex;
    align-items: center;
    gap: 1rem;
  }

  .live-status {
    display: flex;
    align-items: center;
    gap: 0.5rem;
    padding: 0.35rem 0.75rem;
    background: rgba(0, 255, 102, 0.08);
    border: 1px solid rgba(0, 255, 102, 0.25);
    border-radius: var(--radius-sm);
    font-family: var(--font-mono);
    font-size: 0.75rem;
    color: var(--green-matrix);
  }

  .btn-github {
    display: flex;
    align-items: center;
    gap: 0.5rem;
    padding: 0.45rem 0.9rem;
    background: rgba(255, 255, 255, 0.05);
    border: 1px solid var(--border-subtle);
    border-radius: var(--radius-md);
    font-size: 0.85rem;
    font-weight: 600;
    color: var(--text-main);
    transition: all 0.2s;
  }

  .btn-github:hover {
    background: rgba(255, 255, 255, 0.1);
    border-color: rgba(255, 255, 255, 0.2);
    transform: translateY(-1px);
  }

  .mobile-toggle {
    display: none;
    background: transparent;
    border: none;
    color: var(--text-main);
    cursor: pointer;
  }

  @media (max-width: 900px) {
    .live-status {
      display: none;
    }

    .mobile-toggle {
      display: block;
    }

    .nav-links {
      display: none;
      position: absolute;
      top: 70px;
      left: 0;
      right: 0;
      flex-direction: column;
      background: var(--bg-surface);
      border-bottom: 1px solid var(--border-subtle);
      padding: 1.5rem;
      gap: 1.2rem;
    }

    .nav-links.active {
      display: flex;
    }
  }
</style>
