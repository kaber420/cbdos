# Especificación Detallada: OmniWISP - Suite de Gestión y Diagnóstico de Redes Inalámbricas de Campo (WISP / PoP / Torres)

**Documento:** `specs/network/plan_aplicacion_y_monitor_ubiquiti_airos.md`  
**Estado:** 📋 Especificación Arquitectónica Completa / En Revisión  
**Fecha:** 25 de Octubre de 2026  
**Módulo:** `cbdos::omniwisp` / `core/src/ui/views/omniwisp/`  
**Destinatario:** Técnico WISP de Terreno, Administrador de Redes Ruteadas, Soporte de Torres y Puntos de Presencia (PoP)

---

## 1. Visión y Necesidad Operativa en Campo

Un técnico WISP o administrador de torres no trabaja con un listado plano de IPs sueltas ni con una dirección fija de fábrica (`192.168.1.20`). En producción se opera con **redes ruteadas multi-subred** organizadas jerárquicamente:
- **Puntos de Presencia (PoP) / Torres / Sitios:** Cada sitio físico (Cerro, Edificio, Mástil, Nodo de Repetición) tiene un router de borde o switch y múltiples sectores/antenas.
- **Sectores / APs:** Antenas sectoriales o cornetas que cubren un azimut específico (ej: "Norte 90° - Rocket 5AC", "Sur 45° - Prism").
- **Estaciones Clientes (CPEs):** Decenas de antenas receptoras conectadas a cada sector, cada una con su propia IP en subredes ruteadas (ej: `10.120.4.x`, `172.24.x.x`).

La aplicación **OmniWISP** en CBDos debe estructurar la información con esta misma jerarquía topológica para permitir:
1. Navegar por **Torres / PoPs**.
2. Filtrar o agrupar los **Sectores (APs)** de cada torre.
3. Monitorear la **salud del sector** (ruido de piso, ancho de canal, airtime, saturación).
4. Diagnosticar con precisión quirúrgica el **CPE del cliente** (desalineación de antenas con Chain 0/1, calidad del cable Ethernet PoE, caídas de negociación a 10Mbps, saturación de tráfico).

---

## 2. Jerarquía de Datos y Topología de Red

```
[ BÓVEDA SEGURA / VAULT (Namespace: "ubnt") ]
  │
  ├── PoP / Torre: "Torre Cerro Colorado" (VLAN Gestión: 10.200.10.0/24)
  │     ├── AP Sectorial: "Norte 90° - Rocket Prism 5AC"  [10.200.10.21]
  │     │     ├── CPE 1: "CPE-Farmacia-San-Juan" [10.200.10.101] (Chain 0: -58, Chain 1: -59 | 1000M Full)
  │     │     ├── CPE 2: "CPE-Talleres-Rios"     [10.200.10.102] (Chain 0: -61, Chain 1: -78 | ALERTA DESALINEADO!)
  │     │     └── CPE 3: "CPE-Escuela-Rural"     [10.200.10.103] (Cable: 10M Half | ALERTA CABLE DAÑADO!)
  │     ├── AP Sectorial: "Sur 120° - LiteAP AC"         [10.200.10.22]
  │     └── Backhaul PTP: "Troncal PoP-Norte -> Central"  [10.200.10.2]
  │
  └── PoP / Torre: "Torre Repetidor Valle Alto" (VLAN Gestión: 10.200.20.0/24)
        ├── AP Sectorial: "Este 60° - Rocket 5AC"        [10.200.20.11]
        └── AP Sectorial: "Oeste 60° - PowerBeam 5AC"     [10.200.20.12]
```

---

## 3. Arquitectura Multiventana de la Aplicación

La navegación dentro de la app es una pila fluida (`pushView`/`popView` de LVGL v9.5):

```
┌────────────────────────────────────────────────────────────────────────┐
│ PANTALLA 1: Gestor de Puntos de Presencia (PoP / Torres)               │
│ - Filtro / Selector de Torre: [ Todas ] [ Cerro Colorado ] [ Valle ]   │
│ - Botón "+ Nuevo PoP / Torre" (Crea una agrupación de antenas)         │
│ - Botón "+ Agregar AP" (Asigna un nuevo radio a un PoP seleccionado)   │
│ - Resumen global: Total Torres, Total APs activos, CPEs Online/Offline │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ (Tocar una Torre o seleccionar AP)
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│ PANTALLA 2: Catálogo de Sectores (APs) de la Torre                     │
│ - Tarjetas táctiles por Sector con métricas rápidas:                   │
│   • Nombre del Sector y Modelo (ej. "Norte 90° - Rocket Prism 5AC")    │
│   • IP de gestión y Estado (Online / Offline / Auth Error)             │
│   • Clientes conectados (ej. "18 CPEs")                                │
│   • Airtime usage global (ej. "48% Sat")                               │
│   • Ruido de piso (ej. "-92 dBm")                                      │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ (Tocar un Sector)
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│ PANTALLA 3: Monitor de Sector en Vivo & Lista de Clientes (CPEs)       │
│ ┌─ HEADER DEL AP ────────────────────────────────────────────────────┐ │
│ │ Frec: 5745 MHz | ChBW: 40 MHz | SSID: WISP-COLORADO-N90            │ │
│ │ Airtime: 42% (Tx: 28% / Rx: 14%) | Noise: -94 dBm | GPS: 12 Sats   │ │
│ └────────────────────────────────────────────────────────────────────┘ │
│ ┌─ LISTA DE ESTACIONES CPE (Scrollable con filtrado por señal/IP) ───┐ │
│ │ [!] CPE-Escuela  | 10.200.10.103 | -71 dBm | LAN: 10M-Half (CABLE!)│ │
│ │ [!] CPE-Talleres | 10.200.10.102 | Δ17 dBm | Ch0: -61 / Ch1: -78  │ │
│ │ [✓] CPE-Farmacia | 10.200.10.101 | -58 dBm | 240/180 Mbps | 1000M │ │
│ └────────────────────────────────────────────────────────────────────┘ │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ (Tocar un CPE problemático)
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│ PANTALLA 4: Diagnóstico Físico Quirúrgico del CPE                      │
│ - Hostname, MAC, IP (`lastip`), Firmware del CPE                       │
│ - Barra de Alineación MIMO:                                            │
│   • Cadena 0: [====== -59 dBm ======]                                  │
│   • Cadena 1: [== -78 dBm ==========] -> Alerta: Delta > 5 dBm!       │
│ - Estado del Puerto Ethernet del Cliente (PoE):                        │
│   • Link: Conectado (Plugged)                                          │
│   • Negociación: 10 Mbps Half-Duplex -> Alerta: Conector RJ45 oxidado  │
│ - Capacidad y Throughput AirMAX:                                       │
│   • DL: 120 Mbps | UL: 80 Mbps                                         │
│   • Tráfico actual: Tx: 4500 kbps / Rx: 820 kbps                       │
│ - Distancia física estimada (m) y Tiempo de actividad (Uptime)         │
│ - Botón "Ping Rápido" y Botón "Volver al Sector"                       │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 4. Modales de Entrada de Datos Táctiles

### 4.1. Modal "Crear PoP / Torre" (`OmniwispAddPopModal`)
- **Nombre del PoP / Torre:** (ej. `Torre Cerro Colorado`, `PoP Central`).
- **Ubicación / Notas:** (ej. `Km 14 Ruta 5`, `Acceso con llave maestra`).
- **Prefijo de Red / Subred sugerida:** (ej. `10.200.10.0/24`).

### 4.2. Modal "Agregar / Editar AP" (`OmniwispAddApModal`)
- **Torre / PoP Asignado:** Dropdown con los PoPs existentes.
- **Nombre del Sector:** (ej. `Sector Norte 90°`).
- **Dirección IP del Radio:** Textarea numérico con teclado (soporta cualquier IP ruteada).
- **Puerto HTTPS / HTTP:** Por defecto 443 (o puertos alternativos tipo 8443, 80).
- **Usuario de Gestión:** (ej. `ubnt`, `admin`).
- **Contraseña:** Input protegido (con icono de ver/ocultar).
- **Guardar en Vault:** Se persiste atómicamente en `VaultService`.

---

## 5. Capa de Red y Deserialización Exhaustiva (`AirOSClient`)

Se extraen todos los datos telemétricos reales que AirOS expone en `/status.cgi` según el backend de referencia:

```cpp
struct AirOSCpeDiagnostics {
    std::string mac;
    std::string ip;
    std::string hostname;
    int signalDbm = 0;
    int signalChain0 = 0;       // chainrssi[0]
    int signalChain1 = 0;       // chainrssi[1]
    int chainDelta = 0;         // |Chain0 - Chain1| (Alerta desalineación si > 4 dB)
    int noiseFloor = -95;
    
    // Capacidad y Throughput
    int txRateKbps = 0;
    int rxRateKbps = 0;
    int dlCapacityMbps = 0;     // airmax.dl_capacity / 1000
    int ulCapacityMbps = 0;     // airmax.ul_capacity / 1000
    int txThroughputKbps = 0;   // remote.tx_throughput
    int rxThroughputKbps = 0;   // remote.rx_throughput
    
    // Cable y Puerto Ethernet Local del Cliente
    bool ethPlugged = false;    // remote.ethlist[0].plugged
    int ethSpeedMbps = 0;       // remote.ethlist[0].speed (10, 100, 1000)
    bool cableFaultAlert = false; // true si negocia a 10M o unplugged
    
    int distanceM = 0;
    int uptimeSec = 0;
    int ccq = 0;
};

struct AirOSSectorTelemetry {
    bool success = false;
    std::string errorMessage;
    
    // Dispositivo AP
    std::string hostname;
    std::string model;
    std::string firmware;
    int uptimeSec = 0;
    
    // Radiofrecuencia
    std::string essid;
    int frequency = 0;          // MHz
    int channelWidth = 0;       // MHz (chanbw)
    int noiseFloor = -95;       // dBm
    
    // Saturación de Espectro (Airtime)
    int airtimeTotal = 0;       // %
    int airtimeTx = 0;          // %
    int airtimeRx = 0;          // %
    
    int gpsSats = 0;            // Satélites GPS sincronizados
    std::vector<AirOSCpeDiagnostics> stations;
};
```

---

## 6. Modularización de Archivos C++ (Directorio `core/src/ui/views/omniwisp/`)

Para garantizar código ultra ordenado, mantenible y sin archivos gigantes:

```
core/
├── include/cbdos/
│   ├── airos_client.hpp                -> Cabecera del cliente desacoplado AirOS
│   └── omniwisp_models.hpp             -> Modelos de datos (PoP, Sector, CPE, Diagnóstico)
│
└── src/
    ├── network/
    │   └── AirOSClient.cpp             -> Autenticación, cookies, cJSON exhaustivo
    │
    └── ui/views/omniwisp/
        ├── OmniwispCommon.hpp          -> Tipos comunes, estilos visuales y navegación
        ├── OmniwispView.hpp / .cpp     -> Vista Principal y orquestador de pila (BaseView)
        ├── OmniwispPopListView.cpp     -> Pantalla 1: Listado de Torres / PoPs
        ├── OmniwispSectorListView.cpp  -> Pantalla 2: Catálogo de Sectores del PoP
        ├── OmniwispSectorDetailView.cpp-> Pantalla 3: Monitor de Sector y tabla de CPEs
        ├── OmniwispCpeDetailView.cpp   -> Pantalla 4: Diagnóstico físico de alineación y cable
        ├── OmniwispAddPopModal.hpp/.cpp-> Modal táctil para crear Torre / PoP
        └── OmniwispAddApModal.hpp/.cpp -> Modal táctil con teclado para agregar Sector/IP
```

---

## 7. Fases de Implementación Quirúrgica

- [ ] **Fase 1: Capa de Datos y Modelos (`omniwisp_models.hpp` y `AirOSClient.cpp`):**
  - Implementar estructuras `PoPInfo`, `SectorConfig`, `AirOSCpeDiagnostics` y `AirOSSectorTelemetry`.
  - Extender el parseo de `AirOSClient.cpp` con cJSON para `chainrssi`, `ethlist`, `airtime`, `chanbw`, `gps`.
- [ ] **Fase 2: Persistencia de Topología en Vault (`VaultService`):**
  - Guardar PoPs y Sectores organizados por namespace `"ubnt_pops"` y `"ubnt_sectors"`.
  - Permitir listar, añadir y eliminar sectores e IPs de cualquier subred.
- [ ] **Fase 3: Modales Táctiles de Alta Usabilidad:**
  - `OmniwispAddPopModal`: Creación de torres.
  - `OmniwispAddApModal`: Formulario completo con teclado virtual de CBDos (`UIManager::attachKeyboard`).
- [ ] **Fase 4: Pantallas Jerárquicas de Monitoreo:**
  - `OmniwispPopListView`: Vista de torres con contadores de salud.
  - `OmniwispSectorListView`: Vista de sectores con métricas de saturación.
  - `OmniwispSectorDetailView`: Monitor del sector con lista scrollable de CPEs.
  - `OmniwispCpeDetailView`: Diagnóstico de desalineación MIMO y cable PoE.
- [ ] **Fase 5: Integración en CMake y Registro en AppRegistry:**
  - Actualizar `core/CMakeLists.txt` con los nuevos archivos modulares.
  - Conectar `"omniwisp"` a `OmniwispView` en `AppRegistry.cpp`.
  - Limpiar el viejo archivo monolítico provisional.
- [ ] **Fase 6: Verificación de Compilación Multi-Target:**
  - ESP32-S3 PlatformIO: `pio run -d bsp/esp32_s3_jc3248`
  - ESP32-P4 ESP-IDF: `idf.py -C bsp/esp32_p4_jc4880 build`
