<script lang="ts">
  import Hero from '../lib/Hero.svelte';
  import Pillars from '../lib/Pillars.svelte';
</script>

<Hero />
<Pillars />

<section class="home-nav-section">
  <div class="container">
    <div class="section-header">
      <span class="section-tag">// EXPLORA POR SECCIÓN</span>
      <h2 class="section-title">Cada tema tiene su propia página</h2>
      <p class="section-desc">
        Sin scroll infinito. Entra a la sección que te interese, cada guía conserva
        su selector de pestañas en un solo cuadro.
      </p>
    </div>

    <div class="home-grid">
      <a href="#/hardware" class="cyber-card home-card">
        <span class="home-icon">🔌</span>
        <h3>Hardware</h3>
        <p>JC4880 P4 vs JC3248 S3. Pinout, buses y comandos de compilación.</p>
        <span class="home-link">Ver placas →</span>
      </a>
      <a href="#/apps" class="cyber-card home-card">
        <span class="home-icon">🕹️</span>
        <h3>Apps</h3>
        <p>Catálogo filtrable: Flasher, BadUSB, Audio Helix, Kerberos…</p>
        <span class="home-link">Ver catálogo →</span>
      </a>
      <a href="#/luapp" class="cyber-card home-card">
        <span class="home-icon">💻</span>
        <h3>Lua++</h3>
        <p>4 ejemplos (.luapp) con pestañas + referencia del SDK.</p>
        <span class="home-link">Ver playground →</span>
      </a>
      <a href="#/flasheo" class="cyber-card home-card highlight">
        <span class="home-icon">⚡</span>
        <h3>Flasheo</h3>
        <p>P4 · S3 · C6 en un solo cuadro con selector. Tal como está, intacto.</p>
        <span class="home-link">Abrir guía →</span>
      </a>
      <a href="#/manuales" class="cyber-card home-card">
        <span class="home-icon">📖</span>
        <h3>Manuales</h3>
        <p>Documentación .md portable para terminal y CBDos.</p>
        <span class="home-link">Leer manuales →</span>
      </a>
    </div>
  </div>
</section>

<style>
  .home-nav-section { padding: 2rem 0 5rem; }
  .home-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
    gap: 1.2rem;
  }
  .home-card { padding: 1.6rem; display: flex; flex-direction: column; gap: 0.5rem; }
  .home-card.highlight { border-color: var(--border-cyan); }
  .home-icon { font-size: 1.8rem; }
  .home-card h3 { font-size: 1.15rem; }
  .home-card p { font-size: 0.88rem; color: var(--text-muted); flex: 1; }
  .home-link { font-size: 0.85rem; font-weight: 700; color: var(--cyan-core); }
</style>
