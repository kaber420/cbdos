# 📜 Borrador de Plan: Unificación de Teclado del Sistema y Purga de Deuda Técnica (CBDos v0.2.1)

> **Tipo de Documento:** Borrador Técnico de Arquitectura y Refactorización (`specs/drafts/`)  
> **Fecha:** 2026-09-14  
> **Estado:** Propuesta Técnica / En Espera de Aprobación  
> **Objetivo:** Eliminar la duplicación masiva de teclados virtuales ad-hoc en las vistas de UI, centralizar el ciclo de vida en `UIManager::attachKeyboard(lv_obj_t* ta)` y purgar más de **1,000 líneas de código espagueti** redundante.

---

## 1. Declaración del Problema y Deuda Técnica

### La Mala Práctica: Teclados Virtuales Ad-Hoc por Vista
A lo largo de las fases de desarrollo de CBDos, cada vez que una nueva vista o modal requería entrada de texto, se implementó una instancia privada e independiente de `lv_keyboard_create()`. 

Esta práctica generó los siguientes vicios arquitectónicos:
1. **Reinvención de la rueda repetida 8 veces:** Cada vista calcula a mano la altura según la pantalla (`(caps.height >= 800) ? 280 : 190`), define estilos hardcodeados de colores oscuros/bordes, enlaza callbacks de botones (`LV_EVENT_READY`, `LV_EVENT_CANCEL`) y administra flags de visibilidad (`m_kbVisible`, `toggleKbBtnCb`).
2. **Desperdicio de Memoria RAM/PSRAM:** Se mantienen en memoria múltiples estructuras LVGL de teclados ocultos simultáneamente en lugar de un único teclado dinámico en `lv_layer_top()`.
3. **Inconsistencia Visual y UX Rota:** Unos teclados usan esquinas redondeadas de 6px, otros de 0px; unos tienen alturas fijas que solapan el campo de texto y otros desplazan el contenedor.
4. **La Ironía del Sistema:** El sistema **YA cuenta** con una implementación global en `core/src/ui/UIManager.cpp` (`UIManager::attachKeyboard`), pero fue ignorada por la mayoría de las vistas.

---

## 2. Inventario Forense de Teclados Duplicados

A continuación se detalla la radiografía exacta del código redundante en el repositorio:

| Archivo Fuente | Línea | Instancia de Teclado | Líneas de Boilerplate Asociadas | Responsabilidad Duplicada |
| :--- | :---: | :--- | :---: | :--- |
| [`core/src/ui/views/MeshCoreView.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/MeshCoreView.cpp) | L391 | `m_keyboard = lv_keyboard_create(tab)` | **~170 líneas** | Teclado para chat de canales públicos, toggle button, callbacks de envío, redimensionamiento. |
| [`core/src/ui/views/MeshCoreView.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/MeshCoreView.cpp) | L1554 | `m_keyboardDm = lv_keyboard_create(m_convPane)` | **~150 líneas** | Segundo teclado idéntico para DMs/mensajería privada. |
| [`core/src/ui/views/TextEditorView.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/TextEditorView.cpp) | L781 | `m_keyboard = lv_keyboard_create(m_container)` | **~130 líneas** | Teclado principal del editor, estilos propios, gestión de foco y cursor. |
| [`core/src/ui/views/TextEditorView.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/TextEditorView.cpp) | L433 | `kb = lv_keyboard_create(m_modalMask)` | **~90 líneas** | Teclado temporal para modal "Guardar Como...". |
| [`core/src/ui/modals/SshConnectModal.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/modals/SshConnectModal.cpp) | L158 | `m_keyboard = lv_keyboard_create(m_modalMask)` | **~140 líneas** | Teclado con máquina de estados para 4 campos (host, port, user, pass). |
| [`core/src/ui/components/terminal/TerminalCommandBar.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/components/terminal/TerminalCommandBar.cpp) | L113 | `m_keyboard = lv_keyboard_create(parent)` | **~110 líneas** | Teclado de barra de comandos terminal con toggle y flag manual. |
| [`core/src/ui/views/RadioView.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/RadioView.cpp) | L962 | `kb = lv_keyboard_create(m_modalMask)` | **~85 líneas** | Teclado modal para añadir URLs manuales de radio. |
| **Total Estimado de Código Duplicado** | — | **8 instancias independientes** | **~875 - 1,050 líneas** | Código prescindible a eliminar. |

*(Nota: En [`core/src/ui/views/HidView.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/HidView.cpp#L186) el teclado actúa como generador interactivo USB BadUSB enviando paquetes HID en tiempo real; este uso especializado se evaluará para conservar su callback raw).*

---

## 3. Arquitectura del Teclado Global del Sistema (`UIManager`)

### Estado Actual en `UIManager.cpp`
En `core/src/ui/UIManager.cpp` ya existe el soporte base:
```cpp
void UIManager::attachKeyboard(lv_obj_t* ta);
void UIManager::closeKeyboard();
```
Actualmente:
* Crea una única instancia perezosa (*lazy singleton*) `s_activeKeyboard` alojada en `lv_layer_top()`.
* Al hacer click o foco en un `textarea`, el teclado aparece flotante sobre la vista activa sin alterar los contenedores internos.
* Al presionar `LV_EVENT_READY` (Enter) o `LV_EVENT_CANCEL` (Cerrar), oculta el teclado automáticamente.

### Mejoras Requeridas en el Teclado del Sistema (Antes de la Migración)
Para que todas las vistas puedan migrar sin pérdida de funcionalidades, `UIManager` debe incorporar:
1. **Auto-Scroll Inteligente:** Al abrirse el teclado en `lv_layer_top()`, si el `textarea` queda cubierto por el teclado, el contenedor padre debe hacer scroll automático hacia arriba para mantener el cursor a la vista.
2. **Soporte Multi-Campo (Tabs / Siguiente):** En modales como `SshConnectModal`, al presionar `ENTER` en un campo (ej. Host), debe transferir el foco y el teclado automáticamente al siguiente campo (Puerto -> Usuario -> Password) antes de cerrar.
3. **Eventos Despachables:** Emitir un callback opcional `onReady(const char* text)` para que vistas como chat o terminal ejecuten la acción de "Enviar Mensaje" inmediatamente al presionar la tecla Enter del teclado.
4. **Layout Adaptable Multi-Target:** Ajustar dinámicamente el tamaño entre pantallas P4 (480x800 vertical) y S3 (320x480 horizontal o vertical) respetando la densidad táctil.

---

## 4. Plan de Ejecución Quirúrgica por Fases

```
┌────────────────────────────────────────────────────────────────────────┐
│ FASE 1: Robustecer UIManager Keyboard Subsystem                        │
│ - Soporte multi-pantalla, auto-scroll, callbacks de submit             │
└──────────────────────────────────┬─────────────────────────────────────┘
                                   │
                                   ▼
┌────────────────────────────────────────────────────────────────────────┐
│ FASE 2: Purga en Modales y Vistas Secundarias                          │
│ - RadioView (L962), TextEditor modal (L433), SshConnectModal (L158)    │
│ - Ahorro: ~315 líneas                                                  │
└──────────────────────────────────┬─────────────────────────────────────┘
                                   │
                                   ▼
┌────────────────────────────────────────────────────────────────────────┐
│ FASE 3: Purga en Vistas de Edición y Consolas                          │
│ - TextEditorView (L781), TerminalCommandBar (L113)                     │
│ - Ahorro: ~240 líneas                                                  │
└──────────────────────────────────┬─────────────────────────────────────┘
                                   │
                                   ▼
┌────────────────────────────────────────────────────────────────────────┐
│ FASE 4: Refactorización Mayor de MeshCoreView                          │
│ - Eliminar m_keyboard (L391) y m_keyboardDm (L1554)                    │
│ - Purga de handlers manuales de toggle y resize                        │
│ - Ahorro: ~320 líneas                                                  │
└──────────────────────────────────┬─────────────────────────────────────┘
                                   │
                                   ▼
┌────────────────────────────────────────────────────────────────────────┐
│ TOTAL AHORRADO: > 875 - 1,000 LÍNEAS DE CÓDIGO ELIMINADAS              │
│ Verificación multi-target en ESP-IDF (P4) y PlatformIO (S3)           │
└────────────────────────────────────────────────────────────────────────┘
```

### Detalle de las Fases

#### Fase 1: Extensión de la API de `UIManager`
* Ampliar `UIManager::attachKeyboard` para aceptar flags de comportamiento opcionales:
  ```cpp
  struct KeyboardOptions {
      bool autoCloseOnSubmit = true;
      std::function<void(const char*)> onSubmit = nullptr;
      lv_obj_t* nextFocusTarget = nullptr;
  };
  void attachKeyboard(lv_obj_t* ta, const KeyboardOptions& opts = {});
  ```
* Asegurar compatibilidad estricta LVGL 9.5.

#### Fase 2: Purga en Modales (`RadioView`, `SshConnectModal`, `TextEditor modal`)
* Reemplazar las declaraciones locales de teclados por `UIManager::attachKeyboard(ta)`.
* Eliminar código de posicionamiento manual, máscaras duplicadas y destrucción de punteros.

#### Fase 3: Purga en `TextEditorView` y `TerminalCommandBar`
* Desacoplar el teclado estático empotrado de `TextEditorView.cpp`. El área de texto ocupa el 100% de la vista de forma limpia; al tocar el texto, el teclado del sistema emerge en la capa superior.

#### Fase 4: Gran Purga en `MeshCoreView.cpp`
* Eliminar `m_keyboard`, `m_keyboardDm`, `m_btnToggleKb`, `m_btnConvToggleKb`.
* Eliminar callbacks repetidos `toggleKbBtnCb`, `convToggleKbCb`, `kbEventCb`, `dmKbEventCb`.
* Conectar `m_taInput` y `m_taDmInput` al teclado del sistema con la opción `onSubmit` apuntando directamente a `sendMessage()` y `sendDmMessage()`.
* **Resultado:** [MeshCoreView.cpp](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/MeshCoreView.cpp) reduce drásticamente su tamaño de 2,918 a ~2,580 líneas sin perder ninguna funcionalidad.

---

## 5. Balance y Métricas de Éxito

| Métrica | Estado Actual (Deuda Técnica) | Estado Proyectado (Unificado) | Ganancia Neta |
| :--- | :---: | :---: | :---: |
| **Instancias de Teclado en Código** | 8 teclados independientes | 1 teclado global del sistema | **-7 instancias** |
| **Líneas de Código de Teclado** | ~1,200 líneas dispersas | ~180 líneas en `UIManager` | **~1,000 líneas eliminadas** |
| **Consumo de Memoria RAM (Objetos LVGL)** | ~8 objetos asignados permanentemente | 1 objeto reutilizado en `lv_layer_top` | **Reducción de overhead en heap** |
| **Consistencia Visual** | 3 estilos de colores y bordes distintos | 100% integrado con `DefaultTheme` | **Experiencia de usuario homogénea** |
| **Mantenibilidad** | Modificar el teclado requiere tocar 7 archivos | Modificar el teclado requiere tocar 1 archivo | **Mantenimiento centralizado** |

---

## 6. Próximos Pasos

1. **Revisión por el Desarrollador:** Aprobar este borrador técnico y autorizar el inicio de la Fase 1.
2. **Regla de Cero Acciones Destructivas:** Ningún código fuente se modificará hasta que este plan cuente con la aprobación formal del usuario.
3. **Validación de Compilación Obligatoria:** Tras cada fase se verificará la compilación limpia tanto en `idf.py build` (ESP32-P4) como en `pio run` (ESP32-S3).
