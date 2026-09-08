# MeshCore: teclado virtual en chat privado (DM)

Fecha: 2026-09-08
Alcance: `core/src/ui/views/MeshCoreView.{hpp,cpp}`
Targets afectados: JC4880 (ESP32-P4, `bsp/esp32_p4_jc4880`) y JC3248W535 (ESP32-S3).

## 1. Problema

En la vista MeshCore, tab `Contactos > Conversación`, el campo `m_taDmInput`
no tenía teclado virtual asociado. El tab `Chat` (canal) sí tenía
`m_keyboard` local (`buildChatsTab`, `MeshCoreView.cpp:365`), por lo que
no se podía escribir un DM a un contacto desde el táctil.

## 2. Causa

`buildContactsTab` creaba `m_taDmInput` + botones Enviar/Retry sin
`lv_keyboard`, sin botón `LV_SYMBOL_KEYBOARD` y sin event callbacks.
El teclado de canal vive dentro del tab Chat (`lv_keyboard_create(tab)`),
que queda oculto al cambiar de tab (tabview), así que no era reutilizable.

## 3. Solución aplicada (código, ya en working tree sin commitear)

`MeshCoreView.hpp`:
- `m_btnDmKb`, `m_keyboardDm`, `m_keyboardDmVisible`.
- Callbacks: `dmInputEventCb`, `toggleDmKbBtnCb`, `dmKbEventCb`.

`MeshCoreView.cpp` (`buildContactsTab`):
- Botón teclado en la fila DM + `lv_keyboard_create(m_convPane)` con la
  misma altura adaptativa que el de canal (280 px si `caps.height >= 800`,
  190 px en caso contrario), bindeado a `m_taDmInput`, oculto por defecto.
- `dmInputEventCb`: `LV_EVENT_READY` → `sendDirectMessage()`;
  `CLICKED/FOCUSED` → muestra el teclado.
- `toggleDmKbBtnCb`: alterna visible/oculto.
- `dmKbEventCb`: `READY` → envía; `CANCEL` → oculta.
- `onDestroy`: nulifica los nuevos punteros.
- El teclado es hijo de `m_convPane`, así que se oculta solo al volver a
  la lista (el padre queda en `HIDDEN`).

Campos auxiliares conectados al teclado global (`UIManager::attachKeyboard`,
patrón ya usado en `RadioView`, `WiFiConfigView`): `m_taSearch`,
`m_taDetailName`, `m_taDetailPath`, `m_taNewName`, `m_taNewSecret`,
`m_taAlias`, campos de radio (`makeField`), `m_taManualKey`,
`m_taManualName`, `m_taImportCard`. No se aplica a `m_taInput`/`m_taDmInput`
para evitar doble teclado (ya tienen teclado local).

## 4. Alcance firmware JC4880

No hay fork por target. `MeshCoreView.cpp` está en `core/CMakeLists.txt:55`
y `bsp/esp32_p4_jc4880/CMakeLists.txt:4` incluye `../../core`, por lo que
el fix compila dentro del firmware JC4880 (P4) y también en S3
(`bsp/esp32_s3_jc3248`). Verificado a nivel de fuentes, no en binario
flasheado.

## 5. Verificación pendiente

```bash
. /home/kaber420/esp/esp-idf/export.sh
cd bsp/esp32_p4_jc4880
idf.py build
pio run -d bsp/esp32_s3_jc3248
```

Test manual en JC4880: MeshCore → Contactos → abrir conversación →
tocar campo → debe aparecer el teclado → escribir → Enviar → el DM sale
y `READY` también envía.

## 6. Regla a futuro

Todo `lv_textarea` nuevo en `MeshCoreView` debe llevar teclado: o bien
teclado local dedicado (inputs principales de chat) o bien
`UIManager::attachKeyboard()` (formularios auxiliares). Sin esto la vista
queda inutilizable en modo táctil puro.

## 7. Causa raíz del `NOT_FOUND` en DM (2026-09-08, fw S3 1.17.1)

El `No encontrado` no era contacto ausente: era trama `SEND_DM` malformada.
El firmware (`MyMesh.cpp`, `CMD_SEND_TXT_MSG = 0x02`) exige
`[txt_type][attempt][timestamp LE32][pubkey prefix 6B][texto]` y busca el
contacto por esos 6 bytes. `MeshCoreClient::sendDM`/`retryPendingDm`
mandaban `[timestamp][pubkey 32B][texto]`: prefijo desplazado → `NOT_FOUND`
siempre. Los mensajes de canal funcionaban porque su `[0x00]` inicial
coincide con `TXT_TYPE_PLAIN`. Corregido en `core/src/meshcore/
meshcore_client.cpp` (ambos sitios) y verificado en aire contra dongle
con firmware de ejemplo (Terminal Chat CLI): DM bidireccional OK.
