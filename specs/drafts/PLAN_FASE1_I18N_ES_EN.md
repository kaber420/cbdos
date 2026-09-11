/# Plan Fase 1 i18n ES-EN — Solo ejecutable

**Fecha:** 11-SEP-2026
**Alcance cerrado:** ES-EN compilado en Flash. Nada de SD, nada de LoRa/BBS, nada de Fase 2/3.
**Referencia visión:** `specs/drafts/BORRADOR_MULTILENGUAJE_I18N_FASE1.md` (contexto, no ejecutar).

---

## 1. Qué hacemos (4 vistas, ~35 strings)

| Vista | Archivo | Strings | Cambio |
|:---|:---|---:|:---|
| Dashboard | `core/src/ui/views/DashboardView.cpp` | 2 | `tr()` en título + accesos |
| Config | `core/src/ui/views/ConfigView.cpp` | 11 | `tr()` + selector idioma + persistencia |
| About | `core/src/ui/modals/AboutModal.cpp` | 6 | `tr()` en modal |
| MusicPlayer | `core/src/ui/views/MusicPlayerView.cpp` | 16 | `tr()` + empty-state + estados play/pausa |

## 2. Archivos nuevos (3)
1. `core/include/cbdos/language.hpp` — `enum StrId` (= futuro `E0:param_id`, no renumerar), `tr()`, `setLanguage()`.
2. `core/src/system/language.cpp` — tablas `lang_es[]/lang_en[]` en Flash + `loadSdPack()` stub con fallback.
3. `SystemConfig.language` en `cbdos_sys` (NVS `lang`, default `es`).

## 3. Regla de cambio por vista
`lv_label_set_text(lbl,"...")` → `lv_label_set_text(lbl, tr(STR_...))`. Nada más. Vistas no listadas no se tocan.

## 4. Fuera de alcance (a propósito)
WiFiConfig, RadioView, MeshCoreView, Flasher, FileManager, packs SD, `E2` BBS, iconificación general. Van en Fase 2/3.

## 5. Checklist
- [ ] `language.hpp/cpp` + NVS `lang`
- [ ] Dashboard migrado
- [ ] Config migrado + selector ES/EN con reboot/rebuild
- [ ] About migrado
- [ ] MusicPlayer migrado
- [ ] `idf.py build` (P4) + `pio run` (S3) en verde, sin tofu montserrat

## 6. Salida
Cambio en Config persiste tras reboot, las 4 vistas cambian a EN sin recompilar, resto sigue en ES. Offline-first intacto, LVGL 9.5 estricto.
