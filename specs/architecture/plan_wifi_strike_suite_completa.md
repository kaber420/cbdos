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
