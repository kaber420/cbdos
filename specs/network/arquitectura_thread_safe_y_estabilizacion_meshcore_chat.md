# 📡 Arquitectura Thread-Safe, Desacoplamiento de Radio y Estabilización del Chat MeshCore

---

## 📋 1. Introducción y Alcance

Este documento especifica la arquitectura de sincronización, gestión de hilos y optimización de interfaz gráfica para el subsistema de mensajería **MeshCore** en CBDos v0.2.1. Aplica a los microcontroladores **ESP32-S3** (JC3248W535 - 320x480) y **ESP32-P4** (JC4880P443C - 480x800).

El objetivo es garantizar:
1. **Inmunidad a caídas por concurrencia (*Zero Kernel Panics / Race Conditions*):** Separación estricta entre las tareas de interrupción de radio (Wi-Fi / ESP-NOW en Core 0) y el hilo de renderizado gráfico de LVGL 9.5 (Core 1).
2. **Fluidez en la Interfaz Gráfica (Inserción Incremental):** Eliminación del borrado destructivo de widgets y de animaciones de scroll innecesarias.
3. **Control y Persistencia de Canales RF:** Eliminación de cambios automáticos o accidentales de modo/canal, proporcionando un botón explícito de guardado adaptado a pantallas táctiles compactas.

---

## 🔍 2. Diagnóstico Técnico de Fallas Previas

### 2.1. Violación de Concurrencia en LVGL (`Thread-Safety Violation`)
- **Mecanismo:** En microcontroladores ESP32 con FreeRTOS, el stack de Wi-Fi procesa los paquetes entrantes dentro de una tarea del sistema de alta prioridad (`wifiTask` / ISR en Core 0).
- **Falla:** El motor invocaba callbacks que ejecutaban directamente `lv_async_call()`. Como LVGL 9 no protege internamente sus listas enlazadas (`_lv_async_call_ll`) con mutexes entre tareas, cuando el Core 1 ejecutaba `lv_timer_handler()` al mismo tiempo, los punteros de memoria se corrompían, disparando una excepción `LoadProhibited / Guru Meditation Error` y reiniciando el dispositivo.

### 2.2. Reconstrucción Destructiva y Animación de Scroll
- **Mecanismo:** Ante cada paquete entrante, la UI ejecutaba `refreshMessages()`, que llamaba a `lv_obj_clean(m_msgList)`.
- **Falla:** Al destruir todos los objetos visuales, el contenedor reiniciaba su posición de scroll a `Y = 0` (el inicio del chat). Luego recreaba todas las burbujas y ejecutaba `lv_obj_scroll_to_y(..., LV_ANIM_ON)`, forzando una animación que recorría toda la pantalla de arriba a abajo. Con ráfagas de 3 o más mensajes, las animaciones y asignaciones de memoria simultáneas congelaban la CPU.

### 2.3. Fallo de Transmisión Saliente por Ausencia de Peer Broadcast
- **Mecanismo:** `esp_now_send(broadcastMac, ...)` hacia `FF:FF:FF:FF:FF:FF`.
- **Falla:** En la API nativa de Espressif, toda dirección MAC destino (incluida la de broadcast) debe registrarse obligatoriamente con `esp_now_add_peer()`. Al no verificarse en el HAL de S3, la llamada retornaba `ESP_ERR_ESPNOW_NOT_FOUND` (-1) y los paquetes del Cyberdeck no salían al aire.

---

## 🏗️ 3. Arquitectura Thread-Safe (Patrón Productor-Consumidor)

Para aislar por completo la radio de la interfaz gráfica, se adopta el patrón canónico de **Buzón en Memoria C++**.

```
┌─────────────────────────────────────────────────────────────┐
│                 CORE 0: HILO DE RADIO / WI-FI               │
│                  (Productor - Alta Prioridad)               │
└──────────────────────────────┬──────────────────────────────┘
                               │ 1. Recibe paquete ESP-NOW
                               │ 2. Parsea trama binaria MeshCore
                               ▼
            ┌──────────────────────────────────────┐
            │       std::mutex m_queueMutex        │
            │  std::vector<MeshMessage> m_queue    │ ◄── [BUZÓN C++]
            └──────────────────────────────────────┘
                               ▲
                               │ 3. Lee y vacía la cola (swap)
                               │    en cada frame (~33ms / 30 FPS)
┌──────────────────────────────┴──────────────────────────────┐
│                  CORE 1: HILO DE UI (LVGL 9.5)              │
│                (Consumidor - BaseView::onUpdate)             │
│                                                             │
│ • addMessageBubble(msg)  -> Anexa burbuja al final          │
│ • lv_obj_scroll_to_y(..., LV_ANIM_OFF) -> Scroll directo    │
│ • Cero llamadas a LVGL desde tareas externas                │
└─────────────────────────────────────────────────────────────┘
```

### Reglas de Operación del Buzón:
1. **La radio nunca interactúa con LVGL:** La tarea de radio solo hace `lock_guard`, empuja el mensaje a la cola y libera el candado. Tiempo de ejecución < 5 microsegundos.
2. **La UI es la única dueña de la pantalla:** La vista implementa `BaseView::onUpdate()`, la cual es invocada dentro del bucle principal de UI (`loop()` en S3 o `lvgl_task` en P4).
3. **Refresco diferido de nodos (*Debouncing*):** Los anuncios de balizas (beacons) solo marcan la bandera `m_nodesDirty = true`. La lista de radar solo se redibuja en `onUpdate()` si hubo cambios, evitando destruir widgets en ráfagas.

---

## 🎨 4. Optimización de UI y Control de Radio

### 4.1. Inserción Incremental de Mensajes
```cpp
void MeshCoreView::onMessageReceived(const apps::meshcore::MeshMessage& msg) {
    if (!m_msgList || !lv_obj_is_valid(m_msgList)) return;

    uint16_t activeCh = MeshCoreEngine::getInstance().getActiveChannelId();
    if (msg.channelId == activeCh) {
        addMessageBubble(msg);

        // Control de memoria: máximo 50 burbujas simultáneas en pantalla
        if (lv_obj_get_child_count(m_msgList) > 50) {
            lv_obj_t* oldest = lv_obj_get_child(m_msgList, 0);
            if (oldest) lv_obj_delete(oldest);
        }

        // Scroll instantáneo sin animación ni salto
        lv_obj_scroll_to_y(m_msgList, LV_COORD_MAX, LV_ANIM_OFF);
    }
}
```

### 4.2. Control Manual de Radio (Pestaña "Radios")
En pantallas de 3.5 pulgadas (320x480), manipular desplegables táctiles suele disparar eventos no deseados. Se implementa:
- **Desplegable de Modo (ESP-NOW, LR, STA, Off):** No aplica cambios automáticamente al cambiar la selección.
- **Desplegable de Canal (CH 1 .. CH 13):** No aplica cambios automáticamente al cambiar la selección.
- **Botón `[ Guardar ]` de Ancho Completo:**
  - Aplica simultáneamente el Canal y el Modo seleccionados.
  - Sincroniza el HAL de hardware y la configuración de `MeshCoreEngine`.
  - Muestra un Toast de confirmación: `"Slot 0: ESP-NOW (CH 13)"`.

---

## ⚙️ 5. Contratos de HAL y Capa de Soporte (BSP S3 / P4)

### 5.1. Registro de Peer Broadcast en `S3NetworkInterface::sendPacket`
```cpp
int sendPacket(const uint8_t* buffer, size_t len) override {
    if (!isReady() || !buffer || len == 0) return -1;
    uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    if (!esp_now_is_peer_exist(broadcastMac)) {
        esp_now_peer_info_t peerInfo = {};
        memset(&peerInfo, 0, sizeof(peerInfo));
        memcpy(peerInfo.peer_addr, broadcastMac, 6);
        peerInfo.channel = 0; // 0 = Sigue dinámicamente el canal Wi-Fi activo
        peerInfo.ifidx = WIFI_IF_STA;
        peerInfo.encrypt = false;
        esp_now_add_peer(&peerInfo);
    }
    esp_err_t err = esp_now_send(broadcastMac, buffer, len);
    return (err == ESP_OK) ? static_cast<int>(len) : -1;
}
```

### 5.2. Límites de Memoria en `MeshCoreEngine`
| Estructura | Límite Máximo | Comportamiento al desbordar |
| :--- | :--- | :--- |
| `m_messages` (Historial RAM) | 100 mensajes | `m_messages.erase(m_messages.begin())` (Ring Buffer) |
| `m_nodes` (Tabla Radar) | 32 nodos | `m_nodes.erase(m_nodes.begin())` (Ring Buffer) |
| `m_seenPacketIds` (Anti-duplicados) | 128 identificadores | Índice circular modular `% 128` |
| `m_msgList` (Widgets en UI) | 50 widgets | `lv_obj_delete(oldest)` |

---

## 🧪 6. Procedimiento de Validación y Pruebas

### 6.1. Compilación Cruzada Limpia
- **ESP32-S3:** `pio run -d bsp/esp32_s3_jc3248`
- **ESP32-P4:** `. /home/kaber420/esp/esp-idf/export.sh && cd bsp/esp32_p4_jc4880 && idf.py build`

### 6.2. Pruebas de Estrés en Caliente (ESP-NOW)
1. Conectar dongle USB de pruebas en la laptop en `/dev/ttyACM0` ejecutando:
   ```bash
   ./tools/meshcore_container/run.sh /dev/ttyACM0 "Base-Laptop" --channel 13
   ```
2. Flashear y encender el Cyberdeck ESP32-S3.
3. Enviar ráfaga continua de 20 mensajes seguidos desde la laptop hacia el Cyberdeck.
   - **Criterio de Aceptación:** Los mensajes deben aparecer fluidamente al fondo de la pantalla sin congelamiento, sin scroll de recorrido de pantalla y sin reinicios por pánico del núcleo.
4. Enviar un mensaje desde la barra de texto del Cyberdeck:
   - **Criterio de Aceptación:** El mensaje debe recibirse inmediatamente en la consola de la laptop.
5. Ir a la pestaña **Radios**, cambiar a Canal 5 en el desplegable (sin pulsar Guardar) y verificar que no se desconecte. Luego pulsar **`[ Guardar ]`** y verificar el cambio efectivo de canal.
