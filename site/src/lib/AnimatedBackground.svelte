<script lang="ts">
  import { onMount, onDestroy } from 'svelte';

  type Effect = 'fireflies' | 'touchSwarm' | 'matrix';

  let canvas: HTMLCanvasElement;
  let ctx: CanvasRenderingContext2D;
  let activeEffect: Effect = $state('touchSwarm');
  let W = $state(0);
  let H = $state(0);
  let mouseX = -9999;
  let mouseY = -9999;
  let mouseActive = $state(false);
  let lastMoveTime = 0;
  let animId = 0;
  let tick = 0;

  const BG = '#0a0d1a';

  // --- Fireflies ---
  let forestOffset = 0;
  const FOREST_SPEED = 0.15;
  const FF_COUNT = 24;
  interface Firefly {
    x: number; y: number; vx: number; vy: number;
    radius: number; alpha: number; pulseSpeed: number; phase: number;
  }
  let fireflies: Firefly[] = [];

  function initFireflies() {
    fireflies = Array.from({ length: FF_COUNT }, () => ({
      x: Math.random() * W,
      y: Math.random() * H,
      vx: (Math.random() - 0.5) * 0.8,
      vy: (Math.random() - 0.5) * 0.8,
      radius: 2 + Math.random() * 4,
      alpha: 0.35 + Math.random() * 0.45,
      pulseSpeed: 0.02 + Math.random() * 0.04,
      phase: Math.random() * Math.PI * 2,
    }));
  }

  function drawForest(c: CanvasRenderingContext2D) {
    const forestY = H * 0.72;
    c.fillStyle = '#05130b';
    c.fillRect(0, forestY, W, H - forestY);

    // Layer 1: distant mountains (slowest)
    c.fillStyle = '#030a06';
    for (let i = -1; i < 5; i++) {
      const mx = (i * W * 0.35 - forestOffset * 0.3) % (W * 1.4) - W * 0.2;
      const mh = H * 0.12;
      c.beginPath();
      c.moveTo(mx, forestY);
      c.lineTo(mx + W * 0.17, forestY - mh);
      c.lineTo(mx + W * 0.35, forestY);
      c.closePath();
      c.fill();
    }

    // Layer 2: mid trees (medium speed)
    const midTrees = [0.0, 0.15, 0.30, 0.45, 0.60, 0.75, 0.90];
    const midHeights = [0.22, 0.16, 0.25, 0.18, 0.23, 0.17, 0.20];
    for (let i = 0; i < midTrees.length; i++) {
      const baseX = midTrees[i] * W * 1.4;
      const cx = ((baseX - forestOffset * 0.6) % (W * 1.4)) - W * 0.2;
      const th = midHeights[i] * H;
      c.fillStyle = '#06150c';
      for (let l = 0; l < 3; l++) {
        const lw = th / 3 + (2 - l) * (W * 0.03);
        const ly = forestY - 10 - (l + 1) * (th / 4);
        c.beginPath();
        c.moveTo(cx - lw / 2, ly + 25);
        c.lineTo(cx, ly);
        c.lineTo(cx + lw / 2, ly + 25);
        c.closePath();
        c.fill();
      }
    }

    // Layer 3: close trees (fastest)
    const trees = [0.0, 0.13, 0.26, 0.40, 0.54, 0.68, 0.82, 0.96];
    const heights = [0.28, 0.35, 0.22, 0.40, 0.30, 0.38, 0.25, 0.32];
    for (let i = 0; i < trees.length; i++) {
      const baseX = trees[i] * W * 1.6;
      const cx = ((baseX - forestOffset) % (W * 1.6)) - W * 0.3;
      const th = heights[i] * H;
      const ty = forestY;

      c.fillStyle = '#080f0a';
      c.fillRect(cx - 3, ty - 20, 6, 20);

      c.fillStyle = i % 2 === 0 ? '#0a1e12' : '#06150c';
      for (let l = 0; l < 3; l++) {
        const lw = th / 3 + (2 - l) * (W * 0.035);
        const ly = ty - 20 - (l + 1) * (th / 4);
        c.beginPath();
        c.moveTo(cx - lw / 2, ly + 30);
        c.lineTo(cx, ly);
        c.lineTo(cx + lw / 2, ly + 30);
        c.closePath();
        c.fill();
      }
    }
  }

  function renderFireflies(c: CanvasRenderingContext2D) {
    forestOffset += FOREST_SPEED;
    drawForest(c);
    for (const f of fireflies) {
      f.x += f.vx; f.y += f.vy; f.phase += f.pulseSpeed;
      if (f.x < 0 || f.x > W) f.vx *= -1;
      if (f.y < 0 || f.y > H) f.vy *= -1;
      f.x = Math.max(0, Math.min(W, f.x));
      f.y = Math.max(0, Math.min(H, f.y));

      const pulse = Math.sin(f.phase) * 0.45 + 0.55;
      const r = f.radius * 0.6 + 2;

      const auraR = r + 8;
      const auraGrad = c.createRadialGradient(f.x, f.y, 0, f.x, f.y, auraR);
      auraGrad.addColorStop(0, `rgba(212,255,0,${f.alpha * pulse * 0.35})`);
      auraGrad.addColorStop(1, 'rgba(212,255,0,0)');
      c.beginPath();
      c.arc(f.x, f.y, auraR, 0, Math.PI * 2);
      c.fillStyle = auraGrad;
      c.fill();

      const coreGrad = c.createRadialGradient(f.x, f.y, 0, f.x, f.y, r);
      coreGrad.addColorStop(0, `rgba(255,255,200,${f.alpha * pulse})`);
      coreGrad.addColorStop(0.5, `rgba(255,255,153,${f.alpha * pulse * 0.7})`);
      coreGrad.addColorStop(1, 'rgba(255,255,153,0)');
      c.beginPath();
      c.arc(f.x, f.y, r, 0, Math.PI * 2);
      c.fillStyle = coreGrad;
      c.fill();
    }
  }

  // --- Touch Swarm ---
  const SWARM_COUNT = 16;
  interface SwarmParticle {
    x: number; y: number; vx: number; vy: number;
    radius: number; alpha: number; phase: number; pulseSpeed: number;
  }
  let swarm: SwarmParticle[] = [];

  function initSwarm() {
    swarm = Array.from({ length: SWARM_COUNT }, () => ({
      x: Math.random() * W,
      y: Math.random() * H,
      vx: (Math.random() - 0.5) * 1.2,
      vy: (Math.random() - 0.5) * 1.2,
      radius: 3 + Math.random() * 5,
      alpha: 0.5 + Math.random() * 0.4,
      phase: Math.random() * Math.PI * 2,
      pulseSpeed: 0.02 + Math.random() * 0.03,
    }));
  }

  // Velocidad base tipo luciérnaga: deriva continua + límites para que
  // nunca se queden fijas ni se peguen en un solo punto.
  const SWARM_MIN_SPEED = 0.35;
  const SWARM_MAX_SPEED = 2.0;

  function renderSwarm(c: CanvasRenderingContext2D) {
    if (mouseActive) {
      const rPulse = Math.sin(tick * 0.2) * 6 + 25;
      c.beginPath();
      c.arc(mouseX, mouseY, rPulse, 0, Math.PI * 2);
      c.strokeStyle = 'rgba(0,245,212,0.25)';
      c.lineWidth = 2;
      c.stroke();

      for (const p of swarm) {
        const dx = mouseX - p.x;
        const dy = mouseY - p.y;
        if (dx * dx + dy * dy < 200 * 200) {
          c.beginPath();
          c.moveTo(mouseX, mouseY);
          c.lineTo(p.x, p.y);
          c.strokeStyle = 'rgba(255,0,127,0.35)';
          c.lineWidth = 1;
          c.stroke();
        }
      }
    }

    // 1. Repulsión entre partículas cercanas para que no se peguen en un punto
    for (let i = 0; i < swarm.length; i++) {
      for (let j = i + 1; j < swarm.length; j++) {
        const a = swarm[i], b = swarm[j];
        const dx = a.x - b.x;
        const dy = a.y - b.y;
        const distSq = dx * dx + dy * dy;
        const minDist = 42;
        if (distSq > 0.01 && distSq < minDist * minDist) {
          const dist = Math.sqrt(distSq);
          const push = ((minDist - dist) / minDist) * 0.12;
          const nx = dx / dist, ny = dy / dist;
          a.vx += nx * push; a.vy += ny * push;
          b.vx -= nx * push; b.vy -= ny * push;
        }
      }
    }

    for (const p of swarm) {
      // 2. Deriva errante permanente (como luciérnagas) aunque no haya mouse
      p.vx += Math.cos(tick * 0.012 + p.phase) * 0.015;
      p.vy += Math.sin(tick * 0.014 + p.phase * 1.7) * 0.015;

      if (mouseActive) {
        const dx = mouseX - p.x;
        const dy = mouseY - p.y;
        const distSq = dx * dx + dy * dy;
        if (distSq > 1 && distSq < 260 * 260) {
          const dist = Math.sqrt(distSq);
          const force = 1.2 / (dist + 15);
          p.vx += (dx / dist) * force * 6;
          p.vy += (dy / dist) * force * 6;
        }
      }
      // Fricción suave (antes 0.97 las mataba en ~100 frames y quedaban fijas)
      p.vx *= 0.995;
      p.vy *= 0.995;

      // 3. Clamp de velocidad: ni fijas ni disparadas
      const speed = Math.hypot(p.vx, p.vy) || 0.0001;
      if (speed < SWARM_MIN_SPEED) {
        p.vx = (p.vx / speed) * SWARM_MIN_SPEED;
        p.vy = (p.vy / speed) * SWARM_MIN_SPEED;
      } else if (speed > SWARM_MAX_SPEED) {
        p.vx = (p.vx / speed) * SWARM_MAX_SPEED;
        p.vy = (p.vy / speed) * SWARM_MAX_SPEED;
      }

      p.x += p.vx; p.y += p.vy; p.phase += p.pulseSpeed;
      if (p.x < 0 || p.x > W) p.vx *= -1;
      if (p.y < 0 || p.y > H) p.vy *= -1;
      p.x = Math.max(0, Math.min(W, p.x));
      p.y = Math.max(0, Math.min(H, p.y));
    }

    // Lineas de constelacion entre particulas cercanas
    for (let i = 0; i < swarm.length; i++) {
      for (let j = i + 1; j < swarm.length; j++) {
        const dx = swarm[i].x - swarm[j].x;
        const dy = swarm[i].y - swarm[j].y;
        const distSq = dx * dx + dy * dy;
        if (distSq < 120 * 120) {
          const dist = Math.sqrt(distSq);
          const alpha = (1 - dist / 120) * 0.35;
          c.beginPath();
          c.moveTo(swarm[i].x, swarm[i].y);
          c.lineTo(swarm[j].x, swarm[j].y);
          c.strokeStyle = `rgba(0,245,212,${alpha})`;
          c.lineWidth = 1;
          c.stroke();
        }
      }
    }

    for (const p of swarm) {
      const pulse = Math.sin(p.phase) * 0.3 + 0.7;
      const r = p.radius * pulse;
      const idx = swarm.indexOf(p);
      const col = idx % 2 === 0 ? '0,245,212' : '255,0,127';

      const grad = c.createRadialGradient(p.x, p.y, 0, p.x, p.y, r * 3);
      grad.addColorStop(0, `rgba(${col},${p.alpha * 0.6})`);
      grad.addColorStop(1, `rgba(${col},0)`);
      c.beginPath();
      c.arc(p.x, p.y, r * 3, 0, Math.PI * 2);
      c.fillStyle = grad;
      c.fill();

      c.beginPath();
      c.arc(p.x, p.y, r, 0, Math.PI * 2);
      c.fillStyle = `rgba(${col},${p.alpha})`;
      c.fill();
    }
  }

  // --- Matrix Rain ---
  const GLYPHS = '0123456789ABCDEFHIJKLMNOPQRSTUVWXYZ*#@$%&+-=<>?:/';
  const FONT_SIZE = 16;
  const COL_SPACING = 20;
  interface MatrixCol {
    x: number; y: number; speed: number; length: number;
    mutateCounter: number; chars: string[];
  }
  let matrixCols: MatrixCol[] = [];

  function initMatrix() {
    matrixCols = [];
    const numCols = Math.floor(W / COL_SPACING);
    for (let i = 0; i < numCols; i++) {
      matrixCols.push({
        x: i * COL_SPACING + COL_SPACING / 2,
        y: -(Math.random() * 400),
        speed: 1.8 + Math.random() * 2.5,
        length: 8 + Math.floor(Math.random() * 16),
        mutateCounter: Math.floor(Math.random() * 6),
        chars: Array.from({ length: 24 }, () => GLYPHS[Math.floor(Math.random() * GLYPHS.length)]),
      });
    }
  }

  function renderMatrix(c: CanvasRenderingContext2D) {
    c.fillStyle = 'rgba(0,0,0,0.08)';
    c.fillRect(0, 0, W, H);

    c.font = `${FONT_SIZE}px "Courier New", monospace`;

    for (const col of matrixCols) {
      col.y += col.speed;

      col.mutateCounter++;
      if (col.mutateCounter >= 3) {
        col.mutateCounter = 0;
        col.chars[Math.floor(Math.random() * col.length)] =
          GLYPHS[Math.floor(Math.random() * GLYPHS.length)];
      }

      if (mouseActive) {
        const dx = mouseX - col.x;
        const dy = mouseY - col.y;
        const dist = Math.sqrt(dx * dx + dy * dy);
        if (dist < 120) {
          col.y += 4;
          col.chars[0] = GLYPHS[Math.floor(Math.random() * GLYPHS.length)];
        }
      }

      const totalH = col.length * FONT_SIZE;
      if (col.y - totalH > H) {
        col.y = -(Math.random() * 400);
        col.speed = 1.8 + Math.random() * 2.5;
        col.length = 8 + Math.floor(Math.random() * 16);
        col.chars = Array.from({ length: 24 }, () => GLYPHS[Math.floor(Math.random() * GLYPHS.length)]);
      }

      for (let j = 0; j < col.length; j++) {
        const gy = Math.floor(col.y - j * FONT_SIZE);
        if (gy < -FONT_SIZE || gy > H + FONT_SIZE) continue;

        if (j === 0) { c.fillStyle = '#eaffea'; c.globalAlpha = 1; }
        else if (j < 3) { c.fillStyle = '#00ff41'; c.globalAlpha = 0.9; }
        else if (j < col.length / 2) { c.fillStyle = '#00b82b'; c.globalAlpha = 0.7; }
        else {
          const fade = 1 - j / col.length;
          c.fillStyle = '#004d13';
          c.globalAlpha = fade * 0.65 + 0.12;
        }

        c.fillText(col.chars[j], col.x, gy);
      }
    }
    c.globalAlpha = 1;
  }

  // --- Main loop ---
  function animate() {
    tick++;
    ctx.fillStyle = BG;
    ctx.fillRect(0, 0, W, H);

    if (activeEffect === 'fireflies') renderFireflies(ctx);
    else if (activeEffect === 'touchSwarm') renderSwarm(ctx);
    else if (activeEffect === 'matrix') renderMatrix(ctx);

    animId = requestAnimationFrame(animate);
  }

  function handleResize() {
    W = canvas.width = window.innerWidth;
    H = canvas.height = window.innerHeight;
    if (activeEffect === 'matrix') initMatrix();
  }

  function handleMouseMove(e: MouseEvent) {
    mouseX = e.clientX;
    mouseY = e.clientY;
    mouseActive = true;
    lastMoveTime = Date.now();
  }

  function handleMouseLeave() {
    mouseActive = false;
  }

  function switchEffect(eff: Effect) {
    activeEffect = eff;
    tick = 0;
    if (eff === 'matrix') initMatrix();
  }

  onMount(() => {
    ctx = canvas.getContext('2d')!;
    handleResize();
    initFireflies();
    initSwarm();
    initMatrix();

    window.addEventListener('resize', handleResize);
    window.addEventListener('mousemove', handleMouseMove);
    window.addEventListener('mouseleave', handleMouseLeave);

    // deactivate mouse after 2s idle
    const interval = setInterval(() => {
      if (Date.now() - lastMoveTime > 2000) mouseActive = false;
    }, 200);

    animId = requestAnimationFrame(animate);

    return () => {
      clearInterval(interval);
    };
  });

  onDestroy(() => {
    cancelAnimationFrame(animId);
    window.removeEventListener('resize', handleResize);
    window.removeEventListener('mousemove', handleMouseMove);
    window.removeEventListener('mouseleave', handleMouseLeave);
  });
</script>

<canvas bind:this={canvas}></canvas>

<div class="effect-selector">
  <select class="fx-select" value={activeEffect} onchange={(e) => switchEffect((e.target as HTMLSelectElement).value as Effect)}>
    <option value="touchSwarm">Particulas</option>
    <option value="fireflies">Luciernagas</option>
    <option value="matrix">Matrix</option>
  </select>
</div>

<style>
  canvas {
    position: fixed;
    top: 0;
    left: 0;
    width: 100%;
    height: 100%;
    z-index: -1;
    pointer-events: none;
  }

  .effect-selector {
    position: fixed;
    top: 80px;
    right: 20px;
    z-index: 100;
  }

  .fx-select {
    font-family: var(--font-mono, 'JetBrains Mono', monospace);
    font-size: 0.7rem;
    font-weight: 600;
    color: var(--text-muted, #94a3b8);
    background: rgba(10, 14, 23, 0.85);
    border: 1px solid rgba(255, 255, 255, 0.1);
    border-radius: 6px;
    padding: 6px 28px 6px 10px;
    cursor: pointer;
    backdrop-filter: blur(8px);
    -webkit-backdrop-filter: blur(8px);
    transition: all 0.2s ease;
    text-transform: uppercase;
    letter-spacing: 0.05em;
    appearance: none;
    -webkit-appearance: none;
    background-image: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='10' height='6' fill='none'%3E%3Cpath d='M1 1l4 4 4-4' stroke='%2394a3b8' stroke-width='1.5' stroke-linecap='round' stroke-linejoin='round'/%3E%3C/svg%3E");
    background-repeat: no-repeat;
    background-position: right 8px center;
  }

  .fx-select:hover {
    color: var(--text-main, #f8fafc);
    border-color: rgba(0, 240, 255, 0.28);
    background-color: rgba(0, 240, 255, 0.08);
  }

  .fx-select:focus {
    outline: none;
    border-color: rgba(0, 240, 255, 0.4);
    box-shadow: 0 0 12px rgba(0, 240, 255, 0.15);
  }

  .fx-select option {
    background: #0a0d1a;
    color: var(--text-main, #f8fafc);
    padding: 8px;
  }

  @media (max-width: 640px) {
    .effect-selector {
      top: auto;
      bottom: 16px;
      right: 50%;
      transform: translateX(50%);
    }
  }
</style>
