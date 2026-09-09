<script lang="ts">
  import { onMount } from 'svelte';
  import AnimatedBackground from './lib/AnimatedBackground.svelte';
  import Navbar from './lib/Navbar.svelte';
  import Footer from './lib/Footer.svelte';
  import Home from './pages/Home.svelte';
  import HardwareExplorer from './lib/HardwareExplorer.svelte';
  import AppShowcase from './lib/AppShowcase.svelte';
  import LuaPlayground from './lib/LuaPlayground.svelte';
  import FlashingGuide from './lib/FlashingGuide.svelte';
  import ManualsSection from './lib/ManualsSection.svelte';
  import { parseHash, type Route } from './lib/router';

  let route = $state<Route>('/');

  function sync() {
    route = parseHash();
    // Cada cambio de página vuelve arriba y actualiza el título
    window.scrollTo({ top: 0 });
    const titles: Record<Route, string> = {
      '/': 'CBDos | El Sistema Operativo Cyberdeck (ESP32-P4 & ESP32-S3)',
      '/hardware': 'Hardware Compatible | CBDos',
      '/apps': 'Aplicaciones Integradas | CBDos',
      '/luapp': 'Micro-Apps Lua++ | CBDos',
      '/flasheo': 'Guía de Flasheo | CBDos',
      '/manuales': 'Manuales del Sistema | CBDos',
    };
    document.title = titles[route];
  }

  onMount(() => {
    if (!window.location.hash) window.location.hash = '#/';
    sync();
    window.addEventListener('hashchange', sync);
    return () => window.removeEventListener('hashchange', sync);
  });
</script>

<AnimatedBackground />

<main class="app-layout">
  <Navbar {route} />
  {#if route === '/'}
    <Home />
  {:else if route === '/hardware'}
    <HardwareExplorer />
  {:else if route === '/apps'}
    <AppShowcase />
  {:else if route === '/luapp'}
    <LuaPlayground />
  {:else if route === '/flasheo'}
    <FlashingGuide />
  {:else if route === '/manuales'}
    <ManualsSection />
  {/if}
  <Footer />
</main>

<style>
  .app-layout {
    min-height: 100vh;
    display: flex;
    flex-direction: column;
    position: relative;
  }
</style>
