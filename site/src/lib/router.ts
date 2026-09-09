// Hash-router mínimo para GitHub Pages (sin SSR, sin dependencias).
// Cada sección actual pasa a ser una página. Los componentes con tabs
// (FlashingGuide, LuaPlayground, HardwareExplorer) se conservan intactos
// dentro de su página.

export type Route =
  | '/'
  | '/hardware'
  | '/apps'
  | '/luapp'
  | '/flasheo'
  | '/manuales';

export const ROUTES: { path: Route; label: string; title: string }[] = [
  { path: '/', label: 'Inicio', title: 'CBDos | El Sistema Operativo Cyberdeck' },
  { path: '/hardware', label: 'Hardware', title: 'Hardware Compatible | CBDos' },
  { path: '/apps', label: 'Apps', title: 'Aplicaciones Integradas | CBDos' },
  { path: '/luapp', label: 'Lua++', title: 'Micro-Apps Lua++ | CBDos' },
  { path: '/flasheo', label: 'Flasheo', title: 'Guía de Flasheo | CBDos' },
  { path: '/manuales', label: 'Manuales', title: 'Manuales del Sistema | CBDos' },
];

export function parseHash(): Route {
  const raw = window.location.hash.replace(/^#/, '') || '/';
  const path = raw.split('?')[0];
  if ((ROUTES as { path: string }[]).some((r) => r.path === path)) return path as Route;
  return '/';
}

export function navigate(path: Route) {
  window.location.hash = `#${path}`;
}
