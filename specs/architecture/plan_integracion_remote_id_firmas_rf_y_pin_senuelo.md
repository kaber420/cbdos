# 🛡️ Especificación Técnica: Integración de Drones Remote ID, Firmas de Vigilancia y PIN de Coacción (Modo Señuelo)

**Fecha:** 28 de Octubre de 2026  
**Ubicación:** `specs/architecture/plan_integracion_remote_id_firmas_rf_y_pin_senuelo.md`  
**Estado:** Plan Formal Aprobado  
**Fuentes de Referencia:** `references/SquachWatch-CYD/` (`src/remote_id.cpp`, `src/signatures.cpp`, `src/security.cpp`), arquitectura CBDos (`BleShieldService.cpp`, `SecurityManager.hpp`, `VaultService.cpp`)

---

## 1. Visión y Objetivos

Esta especificación detalla la incorporación de tres capacidades defensivas de alto nivel a **CBDos**:

1. **Decodificación de Telemetría Real de Drones (ASTM F3411 Remote ID):** Pasar de una simple detección pasiva de baliza a la extracción en tiempo real de coordenadas del dron, altitud, número de serie y la **ubicación geográfica exacta del piloto/operador**.
2. **Taxonomía de Firmas de Vigilancia Auditadas con Confianza Escalonada:** Integración de identificadores de cámaras ALPR (Flock, Motorola, Genetec), cámaras corporales (Axon, Taser Signal), sensores acústicos de disparos (ShotSpotter) y skimmers de tarjetas (HC-05/RN42 SPP), auditados contra el registro IEEE para eliminar falsos positivos con módulos Espressif.
3. **Mecanismo de Desbloqueo Seguro con Modo Señuelo (Decoy Mode / Duress PIN):** Implementación del concepto diseñado para el PIN de seguridad de CBDos: si el usuario es forzado a desbloquear el dispositivo bajo coacción, un segundo PIN activa una sesión encubierta que oculta todas las herramientas de radio, auditoría y la bóveda, presentando el dispositivo como un inocuo reproductor de música y utilidades cotidianas.

---

## 2. Decodificador ASTM F3411 Remote ID (Drones Comerciales)

### 2.1. Diagnóstico Actual en CBDos
En [`core/src/security/BleShieldService.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/security/BleShieldService.cpp#L442-L446), CBDos ya clasifica el paquete como `BleDeviceCategory::DroneRemoteId` cuando detecta el UUID de servicio `0xFFFA`, pero descarta el payload, mostrando solo una alerta genérica sin datos de vuelo.

### 2.2. Arquitectura del Parser (`cbdos::radio::RemoteIdDecoder`)
El estándar **ASTM F3411** transmite paquetes Legacy BLE (AD Type `0x16` Service Data con UUID `0xFFFA`) o elementos de fabricante Wi-Fi Beacon.

Estructura de la trama de 25 bytes:
* **Byte 0 (Header):** Nibble superior = Tipo de Mensaje (`0x0` = Basic ID, `0x1` = Location, `0x4` = System/Operator).
* **Mensaje 0x0 (Basic ID):**
  * ID Type (4 bits) + UA Type (4 bits).
  * 20 bytes de identificación única: Número de serie del dron bajo estándar ANSI/CTA-2063-A (ej. DJI serial).
* **Mensaje 0x1 (Location / Vector):**
  * Estado operacional y altura de referencia.
  * Rumbo/Dirección (`Direction` * 1 grado).
  * Velocidad horizontal y vertical.
  * **Latitud y Longitud del Dron:** Enteros de 32 bits en complemento a dos con escala $10^7$ (`degrees = val / 10000000.0`).
  * **Altitud Barométrica y Geométrica:** Valor de 16 bits sin signo con offset de 1000 m y resolución de 0.5 m (`alt = val * 0.5f - 1000.0f`).
* **Mensaje 0x4 (System / Pilot Location):**
  * Localización de la estación de control terrestre (GCS) o del despegue.
  * **Latitud y Longitud del Piloto/Operador:** Misma escala $10^7$.
  * Altitud del piloto y marca de tiempo GNSS.

### 2.3. Estructuras de Datos en CBDos
En [`core/include/cbdos/ble_types.hpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/include/cbdos/ble_types.hpp), expandir `BleDeviceRecord`:

```cpp
struct DroneTelemetry {
    bool valid = false;
    char serialNumber[32] = {0};
    
    // Aeronave
    double droneLat = 0.0;
    double droneLon = 0.0;
    float droneAltM = 0.0f;
    float speedKmh = 0.0f;
    uint16_t headingDeg = 0;
    
    // Piloto / Operador
    bool hasPilotLocation = false;
    double pilotLat = 0.0;
    double pilotLon = 0.0;
    
    uint32_t lastUpdateMs = 0;
};
```

---

## 3. Base de Datos de Firmas Auditadas contra IEEE

### 3.1. Problema de Falsos Positivos Resuelto
Muchos firmwares de código abierto clasifican cualquier OUI de Espressif (`24:6F:28`, `30:AE:A4`, etc.) como "Flock Camera", generando alertas continuas sobre bombillas inteligentes, enchufes o el propio hardware del usuario.

Se adopta la matriz de 3 niveles de confianza:
1. **`HIGH_CONF`:** Coincidencia exacta de OUI institucional exclusiva, UUID propietario único o payload criptográfico verificado (ej. ShotSpotter MA-L, Flock MA-L `B4:1E:52`, Axon `00:25:DF`).
2. **`MED_CONF`:** OUI de subsidiarias o combinaciones de OUI secundario + patrón de nombre SSID/BLE.
3. **`LOW_CONF`:** Módulos de radio genéricos de terceros (LiteOn, Silicon Labs, Qualcomm) que se registran en el log de auditoría pero no disparan alertas invasivas en pantalla.

### 3.2. Tabla de Clasificación Táctica

| Categoría | Objetivo | Patrón de Detección | Confianza |
| :--- | :--- | :--- | :---: |
| **ALPR** | Flock Safety ALPR | OUI `B4:1E:52` + SSID `flock-` + UUID BLE Propietario | `HIGH_CONF` |
| **ALPR** | Motorola / Genetec | OUI Motorola (`00:01:21`, `00:1D:96`), Genetec OUI | `HIGH_CONF` |
| **BODYCAM** | Axon Body 2/3/4 | OUI `00:25:DF` + SSIDs `AB2-`, `AB3-`, `AB4-`, `AXON-` | `HIGH_CONF` |
| **BODYCAM** | Taser Signal BLE | OUI `28:24:FF` / `E4:05:40` | `HIGH_CONF` |
| **GUNSHOT** | ShotSpotter / Raven | OUI `D4:11:D6` + UUIDs de servicio BLE `0x3100` a `0x3500` | `HIGH_CONF` |
| **GLASSES** | Ray-Ban Meta / Snap | BLE Service UUID `0xFD5F` + Company ID Meta (`0x01AB`) / Luxottica | `HIGH_CONF` |
| **SKIMMER** | Skimmer Cajeros ATM | Módulos HC-05/06/RN42 + Perfil SPP UUID `0x1101` + OUI Microchip | `HIGH_CONF` |
| **TRACKER** | Apple AirTag | Apple Company ID `0x004C` + Payload Find My `0x12, 0x19` | `HIGH_CONF` |
| **TRACKER** | Samsung SmartTag | Service UUID `0xFD5A` (SmartThings Find) | `HIGH_CONF` |
| **TRACKER** | Google Find My Device | Service UUID `0xFEAA` (Chipolo, Moto Tag, Pebblebee) | `HIGH_CONF` |
| **PENTEST** | Flipper Zero | UUIDs `0x3081` a `0x3083` + Advertised Name `Flipper` | `HIGH_CONF` |

---

## 4. Arquitectura del PIN de Coacción y Modo Señuelo (Decoy Mode)

### 4.1. Concepto y Filosofía de Protección
A diferencia de un borrado destructivo (`Wipe`) que levanta sospechas inmediatas si alguien te exige ver el dispositivo, el **Modo Señuelo**:
* Permite el desbloqueo fluido e interactivo del sistema.
* Desactiva el montaje de la Bóveda Criptográfica (`VaultService`).
* Reconfigura el `AppRegistry` de CBDos para exponer únicamente una lista blanca de aplicaciones inofensivas.
* Enmascara el `HeaderBar` ocultando los estados del sniffer Wi-Fi, BLE Shield y monitor de red.

```
                    ┌────────────────────────────┐
                    │    Pantalla de Bloqueo     │
                    │      (SecurityModal)       │
                    └─────────────┬──────────────┘
                                  │
                  ┌───────────────┴───────────────┐
                  ▼                               ▼
       [ PIN Maestro Normal ]           [ PIN de Coacción ]
                  │                               │
                  ▼                               ▼
      ┌───────────────────────┐       ┌───────────────────────┐
      │     MODO COMPLETO     │       │     MODO SEÑUELO      │
      │   (Operación Real)    │       │     (Decoy Mode)      │
      ├───────────────────────┤       ├───────────────────────┤
      │ • Suite Radio / RF    │       │ • Reproductor Música  │
      │ • Wi-Fi Strike & IDS  │       │ • Galería de Fotos    │
      │ • BLE Shield & Hunter │       │ • Editor de Texto     │
      │ • Bóveda Vault Activa │       │ • Calculadora / Reloj │
      │ • SSH & Terminales    │       │ • Utilidades Básicas  │
      │ • Apps Lua++ Tácticas │       │ • Bóveda DESMONTADA   │
      └───────────────────────┘       └───────────────────────┘
```

### 4.2. Especificación del Gestor de Seguridad (`SecurityManager`)
En [`core/include/cbdos/security.hpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/include/cbdos/security.hpp):
* `setMasterPin(const std::string& pin)`
* `setDecoyPin(const std::string& pin)`
* `bool isDecoyModeActive() const`
* Al validar el PIN ingresado:
  * Si coincide con `MasterPin` ➔ `m_decoyActive = false; unlockSystem();`
  * Si coincide con `DecoyPin` ➔ `m_decoyActive = true; unlockSystem();`
  * Si es incorrecto ➔ Incrementa contador de intentos fallidos.

### 4.3. Filtrado de Aplicaciones en `AppRegistry`
El lanzador y el Dashboard consultan `SecurityManager::getInstance().isDecoyModeActive()`:
* En Modo Normal: Muestra todas las categorías (`System`, `Media`, `Network`, `Security`, `Utilities`).
* En Modo Señuelo: Filtra y muestra estrictamente categorías `Media` y `Utilities` básicas. Las aplicaciones tácticas (`BleShieldView`, `WifiStrikeView`, `LanScannerView`, `VaultView`, `SdrView`) quedan invisibles y bloqueadas en el despachador de navegación.

---

## 5. Plan de Implementación por Fases

| Fase | Tarea | Componentes Afectados | Criterio de Verificación |
| :-: | :--- | :--- | :--- |
| **1** | Parser ASTM F3411 Remote ID | `core/src/ble/RemoteIdDecoder.cpp`, `core/src/security/BleShieldService.cpp` | Decodificación de coordenadas y serial en tramas de prueba sintéticas y reales |
| **2** | Taxonomía de Firmas y Confianza | `core/src/security/BleShieldService.cpp`, `core/include/cbdos/ble_types.hpp` | Detección inequívoca de Flock, Axon y Skimmers SPP sin falsos positivos en ESP32 |
| **3** | Modo Señuelo en `SecurityManager` | `core/src/system/SecurityManager.cpp`, `core/src/ui/modals/SecurityModal.cpp` | Desbloqueo diferenciado con PIN maestro vs PIN de coacción |
| **4** | Filtrado Visual en Dashboard | `core/src/ui/views/DashboardView.cpp`, `core/src/system/AppRegistry.cpp` | Ocultamiento total de apps tácticas y bóveda en modo señuelo |

