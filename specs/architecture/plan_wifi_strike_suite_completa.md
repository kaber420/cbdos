# Plan de Arquitectura: Suite Completa Wi-Fi Strike (Beacon Spam, Deauth Testing y Evil Portal)

**Fecha:** 2026-10-18  
**Autor:** Kaber420 & Antigravity (Senior Embedded Systems Developer)  
**Objetivo:** Desarrollar la suite completa de transmisión y auditoría Wi-Fi Strike en CBDos, alcanzando paridad técnica con GhostESP, Bruce y Marauder. Todo el tráfico generado debe ser 100% real (tramas 802.11 vía `esp_wifi_80211_tx`, sin simulaciones ni datos de engaño).

---

## 1. Justificación y Fundamentos Técnicos

Siguiendo la Regla 6 (Prohibición Absoluta de Simulaciones), toda la suite Wi-Fi Strike interactúa directamente con el controlador Wi-Fi de Espressif:
1. **Beacon Spam (Inyección de Tramas de Gestión 802.11 Subtipo 0x08):**
   - Transmisión continua de balizas con SSIDs configurables, listas clonadas de APs locales escaneados, letras de Rickroll y generadores aleatorios.
   - Rotación dinámica de MAC BSSID localmente administrada (`mac[0] &= 0xFE; mac[0] |= 0x02;`).
   - Salto de canal (Channel Hopping) sincronizado para poblar el espectro 2.4 GHz (canales 1 al 13).
2. **Deauth / Disassoc Testing (Subtipos 0x0C y 0x0A):**
   - Evaluación de resiliencia ante ataques de desasociación y desautenticación 802.11 (WPA2 vs WPA3 802.11w Management Frame Protection - MFP).
   - Generación bidireccional (AP -> Station y Station -> AP) con código de razón estándar `0x0007` (*Class 3 frame received from nonassociated STA*) y secuencias aleatorias.
   - Inyección sin rate-limiting artificial para maximizar la eficacia en pruebas de penetración autorizadas.
3. **Evil Portal (Servidor Cautivo y Spoofing DNS):**
   - Modo SoftAP transparente con SSID personalizable o clonado.
   - Servidor DNS cautivo (captura de consultas UDP puerto 53 respondiendo con la IP local del ESP32).
   - Servidor HTTP ligero embebido con plantillas de auditoría (Login genérico, Router Update, Portal de Términos) y guardado seguro de interacciones en MicroSD / NVS.
4. **PMKID Sniffing (EAPOL 4-Way Handshake / RSN IE):**
   - Detección de tramas de asociación inicial para capturar el hash PMKID en routers WPA/WPA2 y volcado a archivo compatible con Hashcat (`.22000` / `.pcap`).

---

## 2. Arquitectura de Módulos

```
┌─────────────────────────────────────────────────────────────┐
│                       UI Layer (LVGL v9.5)                  │
│                     WifiStrikeView.cpp                      │
│      [Deauth Tab]   [Beacon Tab]   [Evil Portal]   [PMKID]  │
└──────────────────────────────┬──────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────┐
│                      Core Service Layer                     │
│                   WifiStrikeService.cpp                     │
│  - Control de estados (Idle, Emitting, PortalActive)       │
│  - Generador de tramas 802.11 (Beacon, Deauth, Disassoc)    │
│  - Estadísticas reactivas (frames sent, clients connected)  │
└──────────────────────────────┬──────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────┐
│                        HAL Layer                            │
│  - hal_wifi_s3.cpp (esp_wifi_80211_tx / SoftAP + DNS)       │
│  - hal_wifi_p4.cpp (Stubs / ESP-Hosted C6)                  │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. Plan de Ejecución por Fases

### Fase 1: Módulo Core `WifiStrikeService` y Generadores 802.11
- Crear `core/include/cbdos/wifi_strike.hpp` con las estructuras de configuración, tipos de ataque y contadores.
- Implementar `core/src/security/WifiStrikeService.cpp` con:
  - Generador de beacons (Single, Random, Rickroll, List Cloner).
  - Generador de deauth/disassoc bidireccional.
  - Tarea en segundo plano FreeRTOS para la ráfaga de paquetes sin congelar la UI.

### Fase 2: Soporte HAL S3 (`hal_wifi_s3.cpp`)
- Exponer e implementar hooks para:
  - `esp_wifi_80211_tx(iface, frame, len, false)`
  - Cambio rápido de canal `esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE)`.
  - Configuración de SoftAP + DNS server para el Evil Portal.

### Fase 3: UI LVGL v9.5 (`WifiStrikeView.cpp`)
- Reemplazar los placeholders estáticos con controles completos:
  - **Pestaña Deauth:** Selector de target (Broadcast vs AP escaneado en Recon), botón Iniciar/Detener, contador de paquetes enviados y log reactivo.
  - **Pestaña Beacon Spam:** Selector de modo (Rickroll, Random, Clonar APs, Custom), slider de velocidad/intervalo, botón de emisión y estado.
  - **Pestaña Evil Portal:** Selector de plantilla (Login, Update, WiFi), campo de SSID AP, botón Iniciar Servidor, lista de clientes y credenciales capturadas.
  - **Pestaña PMKID:** Sniffer de PMKID con indicador de captura de handshake.

### Fase 4: Validación y Compilación Multi-Target
- Compilación PlatformIO `esp32s3_red`.
- Flasheo manos libres.
- Verificación con analizador de espectro / Wireshark en Linux.

---

## 4. Registro de Avance

### 2026-10-19 — Sección: Tramas de Gestión 802.11 (MgmtEmitter + pestaña Deauth)

**Fase 1 (parcial, completada en lo que corresponde a esta sección)**

| Fichero | Rol |
|---|---|
| `core/src/security/wifi/MgmtEmitter.hpp/.cpp` | Frame builder de gestión: Deauthentication `0x0C` y Disassociation `0x0A`, 26 bytes, agnóstico de plataforma |
| `core/include/cbdos/wifi_strike.hpp` | Contratos `MgmtSubtype`, `MgmtDirection`, `MgmtTarget`, `MgmtReason`, `MgmtStrikeConfig`, `StrikeKind` y log acotado de sesión |
| `core/src/security/WifiStrikeService.cpp` | `startMgmt()`, log de sesión y tarea de emisión generalizado por familia de sesión |

Decisiones técnicas que quedan fijadas:

1. **Dos emisores, una sola máquina de estados.** `WifiStrikeService` mantiene una única tarea (`wifi_strike_tx` / `wifi_strike_mgmt`) que resuelve el ciclo según `StrikeKind`. El arbitraje de radio es el mismo para beacons y para gestión: la Claim `WifiStrike` es única en el dispositivo.
2. **Sentido de la suplantación explícito.** En `AP -> STA` la trama aparenta salir de la propia AP (Addr2 = BSSID) y va a broadcast, porque las MAC de los clientes reales son desconocidas para el Air Recon; en `STA -> AP` (Addr2 = MAC localmente administrada rotativa, Addr1 = BSSID) es la dirección inversa. Ninguna de las dos inventa clientes que no se han visto.
3. **Sin rate-limiting artificial**, tal y como pide la sección 1.2: la cadencia la fija el usuario (`burstSize` por salto de canal, `intervalMs` entre iteraciones), con 10 ms de suelo porque por debajo `esp_wifi_80211_tx()` devuelve `NoMemory` en lugar de emitir más.
4. **Razón por defecto `0x0007`** (*Class 3 frame received from nonassociated STA*), la misma de GhostESP y Bruce, con el resto del subconjunto de la Tabla 10-18 del 802.11 seleccionable desde la UI.
5. **Bit Protected Frame implementado** (`0x04` en el byte 1 del Frame Control). Es el campo que permite distinguir WPA2 sin 802.11w de WPA3 con Management Frame Protection sin inventar resultados.
6. **Objetivos desde Air Recon, congelados al arrancar.** La emisión pausa el escaneo, así que la lista de BSSID se toma en el instante del `startMgmt()`; en modo `AccessPoints` sin lista, `validateMgmt()` rechaza la sesión en lugar de emitir contra un objetivo inventado.
7. **Log de sesión acotado a 8 líneas** (`StrikeStatus::log`, deduplicado) para que la UI muestre decisiones y errores reales sin que el servicio acumule memoria.

**Fase 3 (pestaña Deauth completada)**

- Selectores de subtipo (`0x0C` / `0x0A` / alternos), sentido (AP→STA / STA→AP / ambos) y código de razón.
- Selector de objetivo: `Broadcast (todo el espectro)` o una AP concreta del Air Recon, ordenada por RSSI. La selección se conserva **por BSSID**, no por índice, porque el Air Recon reordena la lista en cuanto cambia el RSSI.
- Métricas reactivas (tramas, dirigidas al objetivo, canal, fallos), LED de estado y log de sesión.
- La radio es un recurso único: ambas pestañas se bloquean entre sí mientras haya una sesión viva.

**Corrección de compilación dual-target**

`WifiStrikeView.cpp` usaba `lv_obj_set_hidden()` / `lv_obj_is_hidden()`, que solo existen en LVGL ≥ 9.6. La P4 fija el componente `lvgl__lvgl` 9.5.0 mientras que la S3 resuelve `lvgl/lvgl@^9.5.0` contra la 9.6.x, de modo que la vista **no compilaba en ESP32-P4**. Se han encapsulado ambos estados en los helpers `setHidden()` / `isHidden()`, que eligen el setter dedicado en 9.6+ y el flag `LV_OBJ_FLAG_HIDDEN` en 9.5, evitando además los avisos de obsolescencia en el build de S3.

**Pendiente**

- Slider de intervalo/ráfaga en la pestaña Beacon (previsto en la Fase 3, no implementado).
- Pestañas Evil Portal y PMKID: siguen en estado placeholder.
- Fase 4: flasheo y verificación en aire con analizador, pendiente de ejecutar sobre hardware.
