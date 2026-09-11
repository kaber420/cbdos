# Borrador Soporte Multilenguaje i18n CBDos — Plan por Fases

**Fecha:** 11 de Septiembre de 2026
**Estado:** Planificación (sin código)
**Objetivo:** Soporte ES+EN incremental, vista por vista, sin romper dual-target P4/S3.

---

## 1. Estado actual (medido)

- **Sin infra i18n:** cero `LanguageManager`, `locale`, `gettext`. Todo hardcodeado en español.
- **Volumen real:** ~`619` x `lv_label_set_text` en `core/src` (`589` solo en `core/src/ui`), `40` archivos, `28` vistas + modales + componentes. Más `~64` usos de `dropdown/textarea/checkbox/msgbox/toast`.
- Conteo único: `846` llamadas brutas, `370` strings únicos, promedio `24.0` bytes/string.
- `ConfigManager` (`core/include/cbdos/config_manager.hpp`) ya persiste en NVS `cbdos_sys` — añadir `language` es trivial.
- Fonts actuales: solo `lv_font_montserrat_12/14/16/24`. Suficiente para ES/EN/FR/DE/PT (latin1 + UTF-8 LVGL 9.5). CJK queda fuera de alcance.

## 2. Costo en Flash ES+EN (medido, no estimado a ciegas)

| | Español | Inglés | Total |
|:---|---:|---:|---:|
| Texto (únicos 9.23 KB ES) | ~9.2 KB | ~8-9 KB | ~17-18 KB |
| `\0` + punteros (370 x 4B) | ~1.8 KB | ~1.8 KB | ~3.6 KB |
| `LanguageManager` + clave NVS | — | — | ~2-3 KB |
| **Total** | **~11-13 KB** | **~10-12 KB** | **~22-26 KB (+30% margen = 25-35 KB)** |

- El español ya ocupa ~11 KB en `.rodata` hoy. El costo incremental del inglés es solo **+11-13 KB = 0.18% de 16 MB Flash** (P4 y S3).
- RAM extra: `0` si tablas como `const char*` en Flash. Alternativa SD (`/sdcard/lang/*.json`): `0 KB` Flash, solo parser ~3 KB.
- No usar `gettext`: muy pesado para S3. Usar catálogo por IDs: `enum StrId` + arrays.

## 3. Costo por vista (orden de migración)

Fáciles (<15): `Dashboard(2)`, `AboutModal(6)`, `Config(11)`, `GalleryList(12)`, `Lottie(12)`, `WiFi(15)`, `MusicPlayer(16)`
Medias (15-33): `Power/Time/Storage(18-21)`, `Hid/SshModal(22)`, `TextEditor(28)`, `NetworkManager(32)`, `RadioConfig(33)`
Pesadas: `Flasher(37)`, `FileManager(39)`, `RadioView(67)`, `MeshCoreView(151, ~20% del total)`

Nota: `GalleryView/SerialTerminal/Utilities` cuentan 0 con regex simple (labels dinámicos) — no sirven como piloto.

## 4. Caso RadioView: por qué 67 y propuesta de iconos

Lo visible son 4 (`Listas/Explorar/Agregar/Buscar`), el resto escondido:
- Player bar + estados: `Selecciona una emisora`, `Reproduciendo stream...`, `Detenido`, `Conectando...`, `Listo`.
- Tab Agregar (6): `Nombre/URL/Genero`, placeholders `Ej. ...`, `Guardar en Favoritas`.
- Modales gestión (12): `Gestion de Listas...`, `Crear/Exportar/Importar/Eliminar/Cerrar`, etc.
- Toasts/empty/paginación (~20): `Sin conexion WiFi...`, `Buscando...`, `Pagina 1`, etc.

Iconificar (ahorra UI + poco i18n, `67 → ~60`):
- `Buscar` texto (22% ancho) → botón cuadrado `LV_SYMBOL_SEARCH` (lupa). Mantener placeholder para accesibilidad.
- `Ant/Sig` → solo `LV_SYMBOL_LEFT/RIGHT`.
- Ya iconizados: Play/Pause/Trash/Plus/Settings — costo cero.
- NO iconificar: `Nombre/URL/Genero`, empty states, errores, toasts, títulos de modal. Ni `Listas/Explorar/Agregar` (3 palabras triviales `Lists/Explore/Add`, quitarlas pierde claridad).

## 5. Plan propuesto

### Fase 0 — Infra (una vez, sin tocar vistas)
- `core/include/cbdos/language.hpp`: `enum class StrId`, `tr(StrId)`, `setLanguage("es"/"en")`, carga Flash + NVS + rebuild UI.
- `SystemConfig.language` en `cbdos_sys`.
- Fallback a ES si falta clave. Vistas no migradas siguen con literal — cero regresión.

### Fase 1 — Piloto útil + fácil (~35 strings, 4 archivos)
| Vista | N | Patrón que valida |
|:---|---:|:---|
| `DashboardView` | 2 | Label simple, primera pantalla (máximo impacto) |
| `ConfigView` | 11 | Hub, navegación + toast + `snprintf` dinámico |
| `AboutModal` | 6 | Patrón modal reutilizable |
| `MusicPlayerView` | 16 | Empty-state + estado dinámico + SD |

Fuera de Fase 1: `WiFiConfig` (Fase 2, flujo crítico), `Radio/MeshCore/Flasher/FileManager` (Fase 3, pesadas).

### Fase 2 — Flujo diario
`WiFiConfig + Storage + Power + Time + FileManager + NetworkManager + TextEditor`

### Fase 3 — Pesadas + iconificación Radio
`RadioView (con lupa + flechas) + MeshCoreView + FlasherView + RadioConfig + Lua++ binding tr()`

## 6. Criterios de salida Fase 1
1. Cambio en `Config` persiste tras reboot (NVS).
2. Las 4 vistas piloto se redibujan en EN sin recompilar.
3. Compila dual-target: `idf.py build` (P4) + `pio run` (S3), sin tofu en montserrat.
4. Bilingüe parcial permitido: resto sigue en ES.
5. Reglas CBDos intactas: offline-first, LVGL 9.5 estricto, sin secrets en repo.

## 7. Riesgos
- `snprintf` con formato y plurales (`Pagina %d`, `Soltar (%.1fs)`).
- `dropdown` con `\n`, placeholders y teclado virtual.
- Crecimiento futuro: cada idioma extra ~+12 KB. CJK requeriría fuente custom + PSRAM — no contemplado.

---
**Siguiente decisión pendiente:** incluir o no `WiFiConfig` ya en Fase 1 (sube utilidad, sube riesgo en flujo crítico).
