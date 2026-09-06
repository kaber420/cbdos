# Auditoría Técnica: Subsistema USB Host, Eventos Nativos de Hardware y Erradicación de Simulaciones en CBDos

**Fecha:** 2 de Septiembre, 2026  
**Documento:** `docs/network/auditoria_usb_host_eventos_nativos_y_cero_simulaciones.md`  
**Estado:** Propuesta Arquitectónica y Corrección de Malas Prácticas  
**Objetivo:** Erradicar todo comportamiento simulado o estático en el puerto USB OTG del ESP32-P4, implementando la gestión por eventos nativos de hardware (*hot-plug*) y garantizando que el sistema refleje exclusivamente la verdad física del hardware.

---

## 🛑 1. Principio Fundamental del Proyecto

> [!IMPORTANT]
> **CBDos es un Sistema Operativo para Hardware Real, no una Maqueta Visual.**
> Queda terminantemente prohibido en cualquier capa del sistema:
> 1. Hardcodear direcciones MAC, nombres de nodos o estados de enlace de respaldo (*fallbacks* engañosos) que simulen que un dispositivo periférico está respondiendo cuando no lo está.
> 2. Tratar puertos periféricos dinámicos (USB) como si fueran buses estáticos soldados a la placa madre (SPI, I2C).
> 3. Recurrir a bucles de sondeo (*polling*) o temporizadores pesados cuando el hardware y el sistema operativo (ESP-IDF) ya proveen interrupciones y callbacks nativos de conexión y desconexión.

---

## 🔍 2. Auditoría de Malas Prácticas Encontradas en el Código

Durante la auditoría del subsistema de radio módem USB (`bsp/esp32_p4_jc4880/hal/` y `core/`), se identificaron fallas graves de arquitectura:

### Falla 1: La MAC Falsa de Respaldo (`hal_mesh_p4.cpp`)
* **Código encontrado:**
  ```cpp
  // Código defectuoso original en getMacAddress():
  out_mac[0] = 0x9C; out_mac[1] = 0xCC; out_mac[2] = 0x01;
  out_mac[3] = 0x7C; out_mac[4] = 0x0C; out_mac[5] = 0x94;
  return true;
  ```
* **Impacto:** Si el módem C3 no estaba conectado, o si fallaba la negociación USB, la función entregaba `true` y copiaba esta MAC inventada. Esto provocó que la pantalla mostrara una MAC fija, engañando al usuario y haciendo creer que el enlace funcionaba cuando el driver ni siquiera había abierto el puerto.
* **Corrección obligatoria:** Si el dispositivo no ha respondido legítimamente con su MAC física, la función DEBE devolver `false` y ceros.

### Falla 2: Supresión Deliberada de los Callbacks Nativos de Hardware (`usb_cdc_loader_port.cpp`)
* **Código encontrado (Líneas 91-97):**
  ```cpp
  const cdc_acm_host_driver_config_t driver_config = {
      .driver_task_stack_size = 4096,
      .driver_task_priority = 5,
      .xCoreID = 0,
      .new_dev_cb = NULL,  // ❌ DESACTIVADO: Se anuló el callback de inserción nativa de ESP-IDF
  };
  ```
* **Impacto:** Al colocar `.new_dev_cb = NULL`, el stack USB Host de ESP-IDF no notifica a la aplicación cuando un usuario enchufa un dispositivo. El sistema quedó ciego ante eventos de inserción física.

### Falla 3: Tratamiento Estático de un Puerto USB Dinámico (`hal_mesh_p4.cpp`)
* **Código encontrado:** La función `initMeshTransportP4()` llamaba a `init(1)` únicamente una vez en `app_main()`.
* **Impacto:** Si el ESP32-C3 se conecta 2 segundos después del encendido del P4, o si tarda más de 1.5 s en estabilizar su oscilador y negociar el bus USB, el intento de apertura da *timeout* en el boot y el sistema jamás vuelve a intentar vincular el puerto.

### Falla 4: Desconexión Física Ignorada (Fuga de Handle USB)
* **Código encontrado (Líneas 51-60):**
  ```cpp
  case CDC_ACM_HOST_DEVICE_DISCONNECTED:
      ESP_LOGW(TAG, "Dispositivo CDC-ACM desconectado");
      break; // ❌ Solo imprime texto; no cierra s_cdc_dev ni resetea s_usb_active
  ```
* **Impacto:** Al desconectar un C3, el handle `s_cdc_dev` queda en un estado huérfano. La variable `s_usb_active` permanece en `true`. Cuando el usuario conecta otro C3 (o el mismo de nuevo), el driver cree que el dispositivo viejo sigue ahí, se niega a reabrirlo y cualquier escritura falla de forma silenciosa o por timeout.

### Falla 5: Interfaz Gráfica Congelada sin Eventos (`MeshCoreView.cpp` y `NetworkManagerView.cpp`)
* **Impacto:** Las tarjetas de red en LVGL creaban las etiquetas (`lblMac`, `lblStatus`) una sola vez al construir la vista. Al no estar conectadas a un sistema de eventos del `NetworkInterfaceManager`, la UI permanecía congelada para siempre con el texto que evaluó en el milisegundo de su creación.

### Falla 6: Protocolo de Módem "Ciego" (Sin Identificación de Firmware ni Versión)
* **Impacto:** En [`tools/espnow_usb_bridge/src/main.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/tools/espnow_usb_bridge/src/main.cpp#L348), la respuesta a `RADIO_CMD_GET_STATUS` enviaba MAC, modo, canal, potencia y alias, pero **no enviaba el nombre ni la versión del firmware instalado**. Al conectar uno de varios ESP32-C3, el usuario no tenía forma de saber qué versión o qué software estaba corriendo en ese chip.

---

## 🏛️ 3. Arquitectura Correcta: Event-Driven Hardware Nativo

La solución real no utiliza *polling* ni bucles infinitos de comprobación. Utiliza el modelo de eventos por interrupción provisto nativamente por ESP-IDF y FreeRTOS:

```
                  ┌─────────────────────────────────────┐
                  │    Conexión Física USB (Hot-Plug)   │
                  └──────────────────┬──────────────────┘
                                     │ (Interrupción D+/D-)
                                     ▼
                  ┌─────────────────────────────────────┐
                  │  ESP-IDF USB Host / cdc_acm_host    │
                  └──────────────────┬──────────────────┘
                                     │
           ┌─────────────────────────┴─────────────────────────┐
           ▼                                                   ▼
┌─────────────────────────────┐             ┌─────────────────────────────────────┐
│ Evento: Inserción           │             │ Evento: Extracción                  │
│ cdc_acm_new_dev_callback()  │             │ CDC_ACM_HOST_DEVICE_DISCONNECTED    │
└──────────┬──────────────────┘             └──────────────────┬──────────────────┘
           │                                                   │
           ▼                                                   ▼
┌─────────────────────────────┐             ┌─────────────────────────────────────┐
│ 1. Abrir CDC Device         │             │ 1. cdc_acm_host_close(dev)          │
│ 2. Enviar Handshake (STATUS)│             │ 2. Limpiar m_macValid = false       │
│ 3. Arrancar RX Task L2      │             │ 3. Detener RX Task L2               │
│ 4. Notificar a NetworkMgr   │             │ 4. Notificar a NetworkMgr (OFFLINE) │
└──────────┬──────────────────┘             └──────────────────┬──────────────────┘
           │                                                   │
           └─────────────────────────┬─────────────────────────┘
                                     │ Evento de Cambio de Estado
                                     ▼
                  ┌─────────────────────────────────────┐
                  │ NetworkInterfaceManager (Slot 2)    │
                  └──────────────────┬──────────────────┘
                                     │ Callback de UI
                                     ▼
                  ┌─────────────────────────────────────┐
                  │ UI LVGL 9.5 (MeshCore / NetManager) │
                  │ Actualización inmediata de etiquetas│
                  └─────────────────────────────────────┘
```

---

## 📋 4. Especificación Técnica de los Cambios

### 4.1. Driver USB Host (`usb_cdc_loader_port.cpp` y `.hpp`)
1. **Activar `.new_dev_cb`:**
   Implementar `cdc_acm_new_dev_callback(cdc_acm_dev_hdl_t dev)` en la configuración de `cdc_acm_host_install`.
2. **Manejo Estricto de Desconexión:**
   En `cdc_event_callback`, ante `CDC_ACM_HOST_DEVICE_DISCONNECTED`:
   * Ejecutar `cdc_acm_host_close(s_cdc_dev)`.
   * Resetear `s_cdc_dev = NULL`, `s_usb_active = false`.
   * Invocar el callback de desconexión del HAL de red.

### 4.2. HAL de Red Módem (`hal_mesh_p4.cpp`)
1. Registrarse como escucha de los eventos de inserción/extracción de `usb_cdc_loader_port`.
2. Al conectarse:
   * Enviar inmediatamente `RADIO_CMD_GET_STATUS`.
   * Si el dispositivo contesta la trama `0xAA 0x55`, extraer MAC, versión de firmware, modelo y alias.
   * Si no contesta tras 3 intentos (500 ms cada uno), marcar como `Dispositivo USB No Reconocido / Requiere Flasheo` (sin congelarse ni fingir datos).
3. Al desconectarse:
   * Limpiar estado, cancelar buffers y notificar a la UI.

### 4.3. Firmware Módem ESP32-C3 (`tools/espnow_usb_bridge`)
1. **Nuevo campo en `RADIO_CMD_GET_STATUS`:**
   Extender el payload de respuesta para incluir:
   * `uint8_t fw_major` (ej: 0)
   * `uint8_t fw_minor` (ej: 2)
   * `uint8_t fw_patch` (ej: 0)
   * `char fw_name[16]` (ej: `"CBDos-Modem"`)
2. **Trama de Anuncio al Arrancar (USB Beacon):**
   Al terminar el `setup()`, el C3 emite automáticamente su trama de estado por USB para que el P4 no tenga que esperar a que el usuario interactúe.

### 4.4. Interfaz Gráfica (`MeshCoreView.cpp` y `NetworkManagerView.cpp`)
1. Eliminar labels estáticas sin refresco.
2. Implementar un método de actualización reactiva (`updateSlot2Status()`) que se invoque cuando el `NetworkInterfaceManager` emita un evento de cambio de interfaz, o mediante el ciclo de eventos de LVGL.
3. Mostrar tres estados reales y honestos:
   * **🔌 Desconectado:** *"Puerto USB libre. Conecta un módem C3/C6."*
   * **⚠️ Dispositivo No Reconocido:** *"Dispositivo USB detectado pero no responde como módem. ¿Deseas flashearlo?"*
   * **✅ Conectado:** *"Módem: PoP1a | FW: CBDos-Modem v0.2.0 | MAC: 84:F7:03:XX:XX:XX"*

---

## 🎯 5. Conclusión y Próximo Paso

Con este documento queda formalizada la auditoría y el rechazo explícito a cualquier técnica de simulación o hardcodeo. La implementación respetará la arquitectura nativa basada en eventos del ESP32-P4.

El siguiente paso es revisar y aprobar esta especificación antes de tocar el código fuente.
