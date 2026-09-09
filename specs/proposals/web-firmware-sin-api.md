# Plan futuro: firmwares sin depender de la API de GitHub

> Estado: PROPUESTA (v1 usa `api.github.com/repos/kaber420/cbdos/releases` con cache de 30 min).
> Motivo del cambio: la API anónima se limita a 60 peticiones/hora por IP y un bug
> de re-fetch puede quemar la cuota y bloquear el instalador con 403.

## Idea

`scripts/release_firmware.sh` genera además un `firmware.json` estático y lo commitea
en `site/public/firmware.json` (se publica con el sitio, sin rate-limit jamás):

```json
{
  "updated": "2026-09-09",
  "firmwares": [
    {
      "board_id": "jc4880p443",
      "soc": "esp32-p4",
      "version": "0.2.4",
      "file": "cbdos-jc4880p443-esp32-p4-v0.2.4-merged.bin",
      "url": "https://github.com/kaber420/cbdos/releases/download/v0.2.4/cbdos-jc4880p443-esp32-p4-v0.2.4-merged.bin",
      "sha256": "<...>",
      "size": 3478848,
      "flash_offset": "0x0"
    }
  ]
}
```

## Cambios requeridos (cuando se active)

1. `release_firmware.sh`: flag `--manifest` que agregue/actualice la entrada en
   `site/public/firmware.json` (misma data que el `.ini`, en JSON).
2. `site/src/lib/firmware.ts`: `fetchReleasesRaw()` lee primero el json local
   (`./firmware.json`) y solo usa la API como respaldo si el local falla.
3. El `.ini` hermano se mantiene (legible por humanos y por herramientas simples).

## Reglas

- El filtro `soc + board_id` y el naming `cbdos-<board>-<soc>-<ver>-merged.bin`
  NO cambian: el json solo cambia la *fuente* de la lista, no el formato.
- Reservado para el DTB futuro: el json podrá crecer con secciones
  `[display]`, `[gpio]`, `[drivers]` sin romper el lector v1.
