# 🍳 Propuesta de Arquitectura: Sistema KDS de Cocina & Terminales Dedicadas TableHub sobre CBDos (ESP32-P4)

**Estado:** 💡 Propuesta de Expansión / Caso de Uso Industrial  
**Target Principal:** ESP32-P4 (Pantallas 7" y 10.1" MIPI-DSI / LVGL 9.5)  
**Perfil de Compilación:** `PROFILE_KIOSK_KDS` / `PROFILE_TABLEHUB`  
**Protocolos:** MQTT / WebSockets / JSON reactivo  
**Ubicación:** `specs/drafts/BORRADOR_KDS_COCINA_TABLEHUB_P4.md`  

---

## 1. Visión y Oportunidad

El **ESP32-P4** es el microcontrolador idóneo para este escenario de grado comercial / hostelería:
1. **Controlador MIPI-DSI Integrado:** Capacidad nativa para manejar paneles de gran formato (7 pulgadas 1024×600 y 10.1 pulgadas 1280×800 o 1920×1200) a 60 FPS estables.
2. **Acelerador de Gráficos 2D (PPA - Pixel Processing Accelerator):** Mezcla de capas y rotación por hardware, permitiendo que LVGL 9.5 mueva decenas de comandas y tarjetas visuales fluidamente.
3. **Memoria y Potencia:** Doble núcleo RISC-V a 400 MHz y soporte de hasta 32 MB de PSRAM de alta velocidad, perfecto para mantener colas de pedidos extensas en memoria sin colapsos.

Con los **Perfiles de Compilación (`Build Profiles`)** de CBDos, convertimos el sistema en un **KDS (Kitchen Display System)** dedicado e inalterable, conectado directamente al ecosistema **TableHub**.

---

## 2. Topología de Red y Flujo de Comandas con MQTT

```
 ┌──────────────────────┐          ┌──────────────────────┐
 │ TableHub Comensal    │          │ POS / Caja Central   │
 │ (ESP32-S3 / WebApp)  │          │ (Servidor TableHub)  │
 └──────────┬───────────┘          └──────────┬───────────┘
            │                                 │
            │ Publica comanda (JSON)          │ Publica pedido mesero
            ▼                                 ▼
   ═══════════════════════════════════════════════════════════════
                    Broker MQTT (Local / LAN)
       Tópicos: tablehub/orders/new , tablehub/kds/status
   ═══════════════════════════════════════════════════════════════
                                  │
                                  │ Suscripción en tiempo real
                                  ▼
                     ┌──────────────────────────┐
                     │   KDS Cocina (ESP32-P4)  │
                     │   Pantalla 7" o 10.1"    │
                     │  - Vista Comandas KDS    │
                     │  - Cronómetros de espera │
                     │  - Interacción táctil    │
                     └──────────────────────────┘
```

### 2.1. Formato de Mensaje Ligero (MQTT Payload)
```json
{
  "order_id": "CMD-104",
  "table": "Mesa 5",
  "timestamp": 1728080000,
  "items": [
    { "name": "Hamburguesa Doble", "qty": 2, "notes": "Sin cebolla" },
    { "name": "Papas Rústicas", "qty": 1, "notes": "Salsa aparte" }
  ],
  "priority": "normal"
}
```

---

## 3. Comportamiento en Modo `PROFILE_KIOSK_KDS`

1. **Arranque Inmediato sin Shell:**
   - Omite el Dashboard genérico y la barra flotante estándar.
   - Entra directamente a la vista `KdsView`.
2. **Interfaz Táctica de Cocina (UI LVGL 9.5):**
   - **Columnas de Comandas:** Tarjetas dinámicas con códigos de color según tiempo de espera:
     - 🟢 Verde: < 10 minutos
     - 🟡 Amarillo: 10 - 20 minutos
     - 🔴 Rojo parpadeante: > 20 minutos (alerta de retraso)
   - **Interacción con un toque:** El cocinero toca un plato o la comanda completa para cambiar su estado (`EN_PREPARACION` -> `LISTO_PARA_ENTREGA`).
   - Al tocar "Listo", el KDS publica por MQTT: `tablehub/orders/ready` notificando al mesero o a la pantalla del cliente.
3. **Bloqueo Total (Zero Manipulación):**
   - No hay acceso a configuración de sistema, terminales ni flasheo.
   - Ajustes de brillo o reconexión Wi-Fi protegidos por PIN táctil en esquina oculta.

---

## 4. Reutilización del Ecosistema CBDos

| Componente CBDos | Reutilización en el KDS |
| :--- | :--- |
| **HAL Display & Touch** | Mismo driver agnóstico escalado a 1024×600 o 1280×800. |
| **Theme Engine** | Paleta de alto contraste para visibilidad con vapor y luz de cocina. |
| **Audio Core (Beeps/WAV)** | Alarma sonora / buzzer I2S cada vez que entra una comanda nueva. |
| **Network HAL** | Reconexión automática Wi-Fi y persistencia de broker en NVS. |
| **Lua Script Engine (Opcional)**| Permite personalizar reglas de cocina o layouts de tickets vía scripts en MicroSD. |

---

## 5. Hoja de Ruta para Integración
1. Integrar cliente MQTT ligero en la capa de red de `core/` (usando el cliente nativo ESP-IDF / lwIP).
2. Crear el flag `CONFIG_CBDOS_PROFILE_KIOSK_KDS`.
3. Diseñar `KdsView` en LVGL 9.5 con diseño de tarjetas deslizables (grid flex).
