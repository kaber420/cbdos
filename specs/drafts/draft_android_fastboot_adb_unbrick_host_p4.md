# Borrador de Arquitectura: Submódulo Android Fastboot, ADB y Unbrick Host por USB OTG
**Ecosistema:** CBDos (ESP32-P4 / USB Host Subsystem / MicroSD Storage)  
**Módulo Propuesto:** `cbdos::android` / `cbdos::fastboot` / `cbdos::adb`  
**Estado:** Borrador de Investigación y Viabilidad Técnica

---

## 1. Motivación y Casos de Uso Tácticos / Campo

Convertir a CBDos (ESP32-P4 con pantalla táctil, 32 MB PSRAM y slot MicroSD) en una estación autónoma de mantenimiento, auditoría y recuperación de dispositivos Android sin necesidad de una computadora portátil (Laptop / PC):

1. **Flashing y Desbloqueo en Campo (Fastboot Flasher):**
   - Desbloquear bootloaders (`fastboot flashing unlock` / `oem unlock`).
   - Flashear particiones críticas (`boot.img`, `recovery.img`, `init_boot.img`, `vbmeta.img`) directamente desde archivos almacenados en la tarjeta MicroSD.
   - Carga y arranque temporal de imágenes en RAM sin modificar la Flash del teléfono (`fastboot boot twrp.img` o recovery custom).
2. **Administración y Automatización ADB (Android Debug Bridge Client):**
   - Ejecución de comandos shell en el dispositivo conectado (`adb shell`).
   - Instalación de aplicaciones (`.apk`) de soporte o diagnóstico en el móvil desde `/sdcard/apps/`.
   - Extracción o inyección de archivos y respaldos rápidos a la tarjeta MicroSD de CBDos.
   - Envío de secuencias de toques y teclas (`adb shell input keyevent ...`, `adb shell input tap ...`).
3. **Recuperación de Emergencia / Unbrick a Bajo Nivel (EDL / BROM):**
   - Detección de dispositivos brickeados en modo **Qualcomm EDL (Emergency Download Mode 9008)** o **MediaTek BROM**.
   - Envío de loaders iniciales (Firehose MBN / DA) a través de canales seriales USB para revivir terminales sin hardware especializado.

---

## 2. Viabilidad Técnica en Hardware ESP32-P4

| Requisito Técnico | Capacidad del ESP32-P4 | Estado / Factibilidad |
| :--- | :--- | :--- |
| **Pila USB Host** | Controlador nativo USB 2.0 OTG High-Speed integrado en el SoC P4 (ESP-IDF USB Host / TinyUSB). | **Comprobado:** Soporta clases CDC-ACM y transferencias Bulk IN / Bulk OUT. |
| **Ancho de Banda** | **USB 2.0 High-Speed nativo a 480 Mbps** (hasta 60 MB/s teóricos). | **Ultra Rápido:** Permite transferir imágenes de boot/recovery (32–64 MB) en **1-2 segundos**, y particiones completas `system`/`super` en minutos. |
| **Memoria de Búfer** | 32 MB Hexal-PSRAM @ 200 MHz. | **Excelente:** Permite alojar imágenes completas o búferes de bloques de streaming (ej. 4 MB por chunk) en PSRAM. |
| **Almacenamiento** | MicroSD Slot 0 en modo SDMMC 4-bit (GPIO 39-44). | **Excelente:** Lectura continua a >15-20 MB/s. |

---

## 3. Arquitectura del Protocolo Fastboot sobre USB Bulk

El protocolo Fastboot es un protocolo cliente-servidor basado en texto y bloques binarios sobre dos endpoints USB Bulk (uno IN y uno OUT):

```
       ESP32-P4 (CBDos Host)                          Dispositivo Android (Fastboot Mode)
 ┌───────────────────────────────┐                  ┌─────────────────────────────────────┐
 │  fastboot::sendCommand()      │ ──[Bulk OUT]───► │ Recibe "getvar:version"             │
 │  fastboot::readResponse()     │ ◄──[Bulk IN]──── │ Responde "OKAY0.4"                  │
 │                               │                  │                                     │
 │  fastboot::download(data,sz)  │ ──[Bulk OUT]───► │ "download:02000000" (Tamaño hex)    │
 │                               │ ◄──[Bulk IN]──── │ Responde "DATA02000000"             │
 │  Envío de chunk de bytes      │ ──[Bulk OUT]───► │ Recibe payload binario en RAM       │
 │                               │ ◄──[Bulk IN]──── │ Responde "OKAY"                     │
 │                               │                  │                                     │
 │  fastboot::sendCommand()      │ ──[Bulk OUT]───► │ "flash:boot"                        │
 │                               │ ◄──[Bulk IN]──── │ Escribe en eMMC/UFS y responde "OKAY│
 └───────────────────────────────┘                  └─────────────────────────────────────┘
```

### 3.1. Estructura de Respuestas Fastboot (Prefijos de 4 bytes)
- **`OKAY`**: Comando completado con éxito (seguido de mensaje de texto opcional).
- **`FAIL`**: Error en la ejecución (seguido de la descripción del fallo).
- **`DATA`**: Listo para recibir datos binarios (seguido del tamaño hexadecimal de los datos).
- **`INFO`**: Mensajes informativos intermedios generados por el bootloader del teléfono.
- **`TEXT`**: Texto plano de depuración.

---

## 4. Arquitectura del Protocolo ADB Embebido

El protocolo ADB opera mediante paquetes estructurados con un encabezado de 24 bytes:

```cpp
struct AdbMessageHeader {
    uint32_t command;       // A_SYNC, A_CNXN, A_OPEN, A_OKAY, A_CLSE, A_WRTE
    uint32_t arg0;          // Primer argumento según comando
    uint32_t arg1;          // Segundo argumento según comando
    uint32_t data_length;   // Longitud del payload de datos que sigue
    uint32_t data_crc32;    // CRC32 del payload para validación
    uint32_t magic;         // command ^ 0xFFFFFFFF (comprobación de integridad)
};
```

### 4.1. Flujo de Autenticación RSA
1. Android requiere que el host se identifique con un par de claves RSA (2048 o 4096 bits).
2. Si el teléfono no tiene la clave autorizada, solicita confirmación en la pantalla del móvil ("¿Permitir depuración USB?").
3. En CBDos, las claves públicas/privadas RSA se pueden generar y guardar en `/sdcard/config/adb_keys.json` usando las librerías criptográficas de mbedTLS integradas en ESP-IDF.

---

## 5. Propuesta de Interfaz C++ e Integración en CBDos

```cpp
namespace cbdos {
namespace android {

enum class DeviceMode {
    Disconnected,
    Fastboot,
    Adb,
    QualcommEdl,
    MtkBrom,
    Unknown
};

struct DeviceInfo {
    DeviceMode mode;
    uint16_t vid;
    uint16_t pid;
    std::string serialNumber;
    std::string product;
    bool isUnlocked;
};

class AndroidHostManager {
public:
    static AndroidHostManager& getInstance();

    bool init();
    DeviceInfo scanDevice();

    // Fastboot APIs
    bool fastbootGetVar(const std::string& varName, std::string& outValue);
    bool fastbootFlash(const std::string& partition, const std::string& filePathOnSd);
    bool fastbootBoot(const std::string& kernelFilePathOnSd);
    bool fastbootUnlock();
    bool fastbootReboot(const std::string& target = ""); // "", "recovery", "bootloader"

    // ADB APIs
    bool adbConnect();
    bool adbShellCommand(const std::string& command, std::string& outResponse);
    bool adbInstallApk(const std::string& apkFilePathOnSd);
    bool adbPushFile(const std::string& localSdPath, const std::string& remoteAndroidPath);
    bool adbPullFile(const std::string& remoteAndroidPath, const std::string& localSdPath);
};

} // namespace android
} // namespace cbdos
```

---

## 6. Integración en UI (LVGL 9.5) y Lua Bindings

### 6.1. Vista Gráfica (`AndroidToolsView`)
- **Panel de Estado:** Muestra si el teléfono está conectado, modo (Fastboot / ADB), fabricante, partición activa (Slot A/B) y estado del Bootloader.
- **Selector de Archivos:** Integrado con el explorador de archivos para elegir imágenes `.img` o aplicaciones `.apk` de la MicroSD.
- **Barra de Progreso:** Visualización del progreso de subida de bloques en tiempo real (MB transferidos / velocidad KB/s).
- **Consola de Logs:** Salida de mensajes del bootloader y shell de Android.

### 6.2. Bindings para Scripts Lua (`cbdos.android.*`)
```lua
-- Script de automatización de recuperación
local dev = android.scan()
if dev.mode == android.MODE_FASTBOOT then
    print("Dispositivo Fastboot detectado: " .. dev.product)
    if not dev.isUnlocked then
        print("Desbloqueando bootloader...")
        android.fastbootUnlock()
    end
    print("Flasheando recovery custom...")
    android.fastbootFlash("recovery", "/sdcard/images/twrp_recovery.img")
    android.fastbootReboot("recovery")
end
```

---

## 7. Próximos Pasos para Fase de Experimentación
1. **Fase 1:** Probar la enumeración USB Host del ESP32-P4 conectando un teléfono en modo Fastboot y capturar los descriptores USB (VID: 0x18D1 Google / Xiaomi / Samsung / etc., Class: 0xFF Vendor Specific, Subclass: 0x42 Fastboot).
2. **Fase 2:** Implementar la capa mínima de transporte Bulk IN/OUT y probar el comando `getvar:version` y `getvar:all`.
3. **Fase 3:** Implementar el streaming de archivos desde MicroSD (`download:` + `flash:`).
4. **Fase 4:** Diseñar la app de LVGL 9.5 con UI intuitiva para el usuario.
