<script lang="ts">
  import Navbar from './lib/Navbar.svelte';
  import Hero from './lib/Hero.svelte';
  import Pillars from './lib/Pillars.svelte';
  import HardwareExplorer from './lib/HardwareExplorer.svelte';
  import AppShowcase from './lib/AppShowcase.svelte';
  import LuaPlayground from './lib/LuaPlayground.svelte';
  import FlashingGuide from './lib/FlashingGuide.svelte';
  import ManualsSection from './lib/ManualsSection.svelte';
  import Footer from './lib/Footer.svelte';
</script>

<main class="app-layout">
  <Navbar />
  <Hero />
  <Pillars />
  <HardwareExplorer />
  <AppShowcase />
  <LuaPlayground />
  <FlashingGuide />
  <ManualsSection />
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
