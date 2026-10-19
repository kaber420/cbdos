# Especificación y Plan Técnico: Perfil CBDos RED & Cuarteto Táctico RF

**Documento:** `specs/architecture/plan_perfil_red_wifi_ids_y_ble_hunter.md`  
**Estado:** Aprobado / En Ejecución  
**Fecha:** 18 de Octubre de 2026  
**Autores:** Kaber420 (Lead Architect & Project Director) & Antigravity (Senior Embedded Architecture)  
**Objetivo:** Especificación técnica completa del perfil **Cyberdeck RED** y su arquitectura táctica de 4 aplicaciones consolidadas con pestañas internas: **Wi-Fi Shield** (defensa), **BLE Shield** (defensa), **Wi-Fi Strike** (ataque) y **BLE Strike** (ataque), junto a la expansión masiva de bases de datos OUI y Bluetooth SIG.

---

## 1. Arquitectura de las 4 Aplicaciones Tácticas (Matriz 2x2)

```
┌────────────────────────────────────────────────────────┐
│               GRID TÁCTICO CYBERDECK RED               │
├───────────────────────────┬────────────────────────────┤
│ 🛡️📶 WI-FI SHIELD         │ 🛡️ᛒ BLE SHIELD             │
│    (Defensa / Azul)       │    (Defensa / Azul)        │
│    Icono: Escudo + Wi-Fi  │    Icono: Escudo + BLE     │
│    Pestañas internas:     │    Pestañas internas:      │
│    • 🔍 Recon (APs/OUI)   │    • 📡 Air Recon (Scan)   │
│    • 🚨 IDS (Deauth/Rogue)│    • 📍 Geiger (AirTags)   │
│    • 📊 Sniffer (PCAP SD) │    • 🚨 Spam Detector (IDS)│
├───────────────────────────┼────────────────────────────┤
│ ⚔️📶 WI-FI STRIKE         │ ⚔️ᛒ BLE STRIKE             │
│    (Ataque / Rojo)        │    (Ataque / Rojo)         │
│    Icono: Espada + Wi-Fi  │    Icono: Espada + BLE     │
│    Pestañas internas:     │    Pestañas internas:      │
│    • ⚡ Deauth / Disassoc │    • 🍏 AppleJuice         │
│    • 📡 Beacon Spam       │    • 🤖 FastPair Spam      │
│    • 🎣 Evil Portal       │    • 🪟 SwiftPair Spam     │
│    • 🔑 PMKID Capture     │    • 🎭 Device Spoofer     │
└───────────────────────────┴────────────────────────────┘
```

---

## 2. Detalle Funcional de Cada Aplicación

### 2.1 🛡️📶 `Wi-Fi Shield` (`WifiShieldView`)
* **Color de Acento:** Azul Cibernético (`#00B4D8`)
* **Icono:** Escudo de defensa con símbolo Wi-Fi
* **Naturaleza:** 100% Pasivo (Cero transmisiones de radio).
* **Pestañas Internas (LVGL 9.5):**
  1. **Recon:** Escaneo de redes Wi-Fi, clientes asociados, canales, RSSI y fabricante OUI.
  2. **IDS Sentinel:** Detección en tiempo real de Deauth Floods, Disassociation, Beacon Spam (1000 SSIDs) y Rogue AP / Evil Twin con alertas sonoras (DAC ES8311).
  3. **Sniffer:** Monitor visual de actividad por canal y volcado continuo a archivo `.pcap` en la MicroSD para análisis con Wireshark.

### 2.2 🛡️ᛒ `BLE Shield` (`BleShieldView`)
* **Color de Acento:** Azul Cobalto (`#0077B6`)
* **Icono:** Escudo de defensa con símbolo Bluetooth
* **Naturaleza:** 100% Pasivo (Escáner GAP continuo en Core 0).
* **Pestañas Internas (LVGL 9.5):**
  1. **Air Recon:** Escaneo de balizas BLE, identificación de fabricantes (Bluetooth SIG Company ID), cámaras/gafas inteligentes (Ray-Ban Meta) y drones (ASTM F3411 Remote ID).
  2. **Geiger Tracker:** Modo localizador de proximidad táctil con medidor VU y clicks audibles por el DAC ES8311 para rastrear físicamente balizas AirTags, SmartTags o Tiles.
  3. **Spam Detector:** Centinela pasivo que alerta de inmediato ante ráfagas de ataques de popups BLE.

### 2.3 ⚔️📶 `Wi-Fi Strike` (`WifiStrikeView`)
* **Color de Acento:** Rojo Táctico (`#E63946`)
* **Icono:** Espada con símbolo Wi-Fi
* **Naturaleza:** Activo / Emisión de Radio (Requiere confirmación explícita de seguridad).
* **Pestañas Internas (LVGL 9.5):**
  1. **Deauth:** Inyección controlada de tramas de desautenticación para auditoría de resiliencia.
  2. **Beacon Spam:** Generador masivo de balizas SSIDs y pruebas de estrés.
  3. **Evil Portal:** Servidor web cautivo autónomo para pruebas de concientización.
  4. **PMKID:** Captura activa de handshakes y hashes WPA2/WPA3 guardados en SD (`.22000`).

### 2.4 ⚔️ᛒ `BLE Strike` (`BleStrikeView`)
* **Color de Acento:** Rojo Carmesí (`#D90429`)
* **Icono:** Espada con símbolo Bluetooth
* **Naturaleza:** Activo / Emisión de Balizas.
* **Pestañas Internas (LVGL 9.5):**
  1. **AppleJuice:** Popups de emparejamiento iOS (AirPods, Beats, Apple TV, AirDrop).
  2. **FastPair:** Popups de accesorios Android.
  3. **SwiftPair:** Popups de vinculación rápida para Windows 10/11.
  4. **Device Spoofer:** Emisión de balizas personalizadas y spoofing de accesorios.

---

## 3. Bases de Datos Estáticas en Flash (.rodata, O(log N))

1. **`OuiDatabase.cpp`:**
   * Más de 400 OUIs ordenadas para `std::lower_bound` (0 bytes heap RAM).
   * Cobertura de routers (Ubiquiti, MikroTik, Cisco, TP-Link), dispositivos móviles (Apple, Samsung, Intel), drones (DJI, Autel) y cyberdecks (Espressif, Raspberry Pi, Flipper).
2. **`BleCompanyDatabase.cpp`:**
   * Tabla completa de Bluetooth SIG Company Identifiers de 16 bits.
