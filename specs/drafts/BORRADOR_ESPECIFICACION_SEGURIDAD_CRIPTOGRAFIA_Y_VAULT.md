# 🔒 Especificación Técnica Formal: Subsistema de Seguridad, Criptografía y Almacenamiento Seguro (Vault)

> **Documento:** `specs/drafts/BORRADOR_ESPECIFICACION_SEGURIDAD_CRIPTOGRAFIA_Y_VAULT.md`  
> **Estado:** Borrador de Ingeniería para Evaluación  
> **Versión:** CBDos v0.2.3-dev (Arquitectura Multi-Target: ESP32-P4 / ESP32-S3)  
> **Fecha:** Septiembre 2026  
> **Autor:** Equipo de Arquitectura CBDos  

---

## 1. Justificación Técnica y Análisis de Brechas

### 1.1. Diagnóstico de las Recomendaciones Previas
Un análisis preliminar de calidad de código (`SUGERENCIAS_FIRMWARE.md`) identificó vulnerabilidades legítimas en el manejo de credenciales en texto plano (`ConfigManager.cpp`) y bloqueos activos de CPU en FIDO2 (`KerberosManager.cpp`). Sin embargo, las soluciones propuestas en dicho documento adolecen de fallos graves:

1. **Recomendación Obsoleta e Insegura (XOR / AES-128-ECB):**
   * *Propuesta previa:* Usar cifrado XOR o AES-128-ECB con `esp_aes_crypt_ecb`.
   * *Fallo crítico:* XOR no es cifrado (es ofuscación trivial). El modo **Electronic Codebook (ECB)** no utiliza vector de inicialización (IV) ni código de autenticación de mensaje (MAC/Tag). Bloques idénticos de texto plano producen bloques idénticos de texto cifrado, lo que revela patrones estructurales y permite ataques de inyección, repetición y manipulación de bits (*bit-flipping*).
2. **Violación de la Regla de Oro #8 (Pureza de `core/`):**
   * *Propuesta previa:* Inyectar llamadas a `esp_aes_*` o `esp_get_free_heap_size()` dentro del núcleo agnóstico.
   * *Fallo crítico:* Destruye la compilación multi-target para ESP32-S3 (PlatformIO/Arduino) y el simulador de desarrollo en PC.
3. **Firma Rígida Incompatible con la Usabilidad de un Cyberdeck:**
   * *Propuesta previa:* Exigir firmas ECDSA/Ed25519 obligatorias en todos los scripts `.dd` de BadUSB.
   * *Fallo crítico:* Invalida la naturaleza de herramienta de campo de CBDos, impidiendo al usuario crear o modificar scripts Ducky al vuelo desde la tarjeta MicroSD o la terminal local.

### 1.2. Capacidades Reales Preexistentes en CBDos
CBDos no arranca de cero en materia criptográfica. El firmware ya cuenta con cimientos sólidos acelerados por hardware en ambos microcontroladores:
* **Motor Criptográfico Kerberos/FIDO2 (`kerberos_core`):** Implementación de curvas elípticas P-256 (ECDSA y ECDH), HMAC-SHA256 y envoltura de claves con **AES-256-GCM** autenticado (`kw_wrap` / `kw_unwrap`).
* **Aceleración Criptográfica por Hardware:** Motores AES, SHA y RSA acelerados por silicio tanto en ESP32-P4 como en ESP32-S3 mediante el backend mbedTLS.
* **Cifrado de Bus MSPI en Silicio (ESP32-P4):** El ESP32-P4 cifra de forma transparente toda la Hexal-PSRAM (32 MB) en tiempo de ejecución mediante hardware **XTS-AES-256**.

---

## 2. Arquitectura Global de Seguridad en CBDos

El modelo de seguridad se estructura en 6 capas ortogonales y desacopladas:

```text
┌─────────────────────────────────────────────────────────────────────────────┐
│                       CAPA 6: APLICACIONES Y UX                             │
│   • BadUSB Gatekeeper (Políticas)   • Permisos y Sandbox Lua (I/O Jail)     │
├─────────────────────────────────────────────────────────────────────────────┤
│                       CAPA 5: GESTIÓN DE IDENTIDAD                          │
│   • FIDO2 / CTAP2 / WebAuthn        • Kerberos Manager (Asíncrono)          │
├─────────────────────────────────────────────────────────────────────────────┤
│                       CAPA 4: ALMACENAMIENTO SEGURO                         │
│   • cbdos::security::Vault          • SecretStorage (AES-256-GCM)           │
├─────────────────────────────────────────────────────────────────────────────┤
│                       CAPA 3: PROTECCIÓN DE MEMORIA                         │
│   • cbdos::SecureBuffer             • Zeroization inmune a optimizador      │
├─────────────────────────────────────────────────────────────────────────────┤
│                       CAPA 2: ABSTRACCIÓN HAL (Agnóstica)                   │
│   • ICryptoProvider                 • Keywrap / KDF / Digest Interfaces     │
├─────────────────────────────────────────────────────────────────────────────┤
│                       CAPA 1: SILICIO / HARDWARE (BSP)                      │
│   • mbedTLS Hardware Accel (AES/SHA)• eFuse Master Keys / XTS-AES PSRAM     │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Especificación Detallada por Componente

### 3.1. Abstracción HAL de Criptografía (`ICryptoProvider`)
Para cumplir estrictamente la **Regla de Oro #8**, el código dentro de `core/` interactúa únicamente con una interfaz abstracta pura C++17.

```cpp
// core/include/cbdos/crypto.hpp
#pragma once
#include <cstdint>
#include <cstddef>

namespace cbdos {
namespace crypto {

struct AesGcmTag {
    uint8_t data[16];
};

class ICryptoProvider {
public:
    virtual ~ICryptoProvider() = default;

    // Generación de números pseudoaleatorios criptográficamente seguros (TRNG)
    virtual bool getRandom(uint8_t* dst, size_t len) = 0;

    // Resúmenes criptográficos (SHA-256)
    virtual bool sha256(const uint8_t* data, size_t len, uint8_t out[32]) = 0;

    // Autenticación de mensajes HMAC-SHA256
    virtual bool hmacSha256(const uint8_t* key, size_t keyLen,
                            const uint8_t* msg, size_t msgLen,
                            uint8_t out[32]) = 0;

    // Cifrado Autenticado AEAD (AES-256-GCM)
    // iv: 12 bytes recomendados por NIST SP 800-38D
    virtual bool aes256GcmEncrypt(const uint8_t key[32], const uint8_t* iv, size_t ivLen,
                                  const uint8_t* aad, size_t aadLen,
                                  const uint8_t* in, size_t inLen,
                                  uint8_t* out, AesGcmTag& outTag) = 0;

    // Descifrado Autenticado AEAD (AES-256-GCM)
    // Retorna false si el Tag falla o los datos fueron corrompidos/adulterados
    virtual bool aes256GcmDecrypt(const uint8_t key[32], const uint8_t* iv, size_t ivLen,
                                  const uint8_t* aad, size_t aadLen,
                                  const uint8_t* in, size_t inLen,
                                  const AesGcmTag& expectedTag,
                                  uint8_t* out) = 0;

    // Derivación de claves (HKDF-SHA256 según RFC 5869)
    virtual bool hkdf(const uint8_t* ikm, size_t ikmLen,
                      const uint8_t* salt, size_t saltLen,
                      const uint8_t* info, size_t infoLen,
                      uint8_t* okm, size_t okmLen) = 0;
};

// Inyección e inspección de proveedor activo
ICryptoProvider* getCrypto();
void setCryptoProvider(ICryptoProvider* provider);

}} // namespace cbdos::crypto
```

---

### 3.2. Módulo de Almacenamiento Seguro: `cbdos::security::Vault`
El `Vault` reemplaza el almacenamiento plano en NVS de `ConfigManager`.

#### A. Esquema Criptográfico del Contenedor
Cada entrada sensible persistida en NVS o almacenamiento de archivos almacena la siguiente estructura binaria:

```text
┌─────────────────────────────────────────────────────────────────────────────┐
│                          REGISTRO CIFRADO DEL VAULT                         │
├──────────────┬──────────────┬───────────────┬────────────────┬──────────────┤
│ Versión (1B) │  Nonce (12B) │ Tag AEAD(16B) │ Longitud (2B)  │ Ciphertext   │
│    0x01      │  CSPRNG TRNG │  AES-256-GCM  │ Longitud útil  │ Datos reales │
└──────────────┴──────────────┴───────────────┴────────────────┴──────────────┘
```

1. **Confidencialidad:** Cifrado simétrico AES-256 en modo GCM.
2. **Integridad y Autenticidad:** La etiqueta de autenticación (Tag) de 128 bits garantiza que cualquier intento de corrupción física de la memoria flash o inyección de bits sea detectado antes de entregar los datos a la aplicación.
3. **Datos Asociados Autenticados (AAD):** El nombre de la clave o namespace (ej. `"wifi.pass"`, `"mesh.psk"`) se utiliza como AAD en el cálculo GCM. Esto previene **ataques de intercambio de claves** (en los que un atacante copia el registro cifrado de la clave WiFi para sobrescribir la clave maestra de la malla).
4. **Protección contra Ataques de Repetición:** Cada escritura genera un Nonce único de 96 bits vía generador aleatorio de hardware (TRNG).

#### B. Gestión de la Clave Maestra del Vault
La clave maestra de 256 bits se deriva mediante una jerarquía de dos niveles:
* **Nivel 1 (Dispositivo - eFuse HMAC / MAC Salt):** Se genera un secreto único por silicio combinado con el identificador único del chip (`esp_efuse_mac_get_default()`), garantizando que un volcado binario de flash extraído de un dispositivo sea completamente ilegible en otro CyberDeck idéntico.
* **Nivel 2 (User Passphrase / Master PIN - Opcional):** Si el usuario define un PIN de arranque en el Cyberdeck, la clave maestra final se deriva combinando el secreto del hardware con el PIN mediante HKDF-SHA256 (10,000 rondas).

---

### 3.3. Manejo de Memoria Segura y Destrucción de Secretos (*Zeroization*)

#### Problema en Firmware Tradicional:
Cuando una contraseña o clave privada sale del almacenamiento y se procesa en variables locales (`std::string pass`), los optimizadores de compilación (`-O2`, `-Os`) frecuentemente eliminan llamadas estándar a `memset()` por considerarlas "escrituras muertas" (*dead store elimination*). Esto deja credenciales en el heap de la PSRAM accesibles a lecturas posteriores.

#### Solución en CBDos:
Se define `cbdos::crypto::cleanse()` utilizando barreras explícitas de memoria y un wrapper seguro `SecureBuffer`:

```cpp
// core/include/cbdos/secure_mem.hpp
#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

namespace cbdos {
namespace crypto {

// Inmune a Dead Store Elimination del optimizador
inline void cleanse(void* ptr, size_t len) {
    volatile uint8_t* p = static_cast<volatile uint8_t*>(ptr);
    while (len--) {
        *p++ = 0;
    }
}

template <typename T>
class SecureAllocator {
public:
    using value_type = T;
    SecureAllocator() noexcept = default;
    template <class U> SecureAllocator(const SecureAllocator<U>&) noexcept {}

    T* allocate(std::size_t n) {
        return static_cast<T*>(::malloc(n * sizeof(T)));
    }

    void deallocate(T* p, std::size_t n) noexcept {
        if (p) {
            cleanse(p, n * sizeof(T));
            ::free(p);
        }
    }
};

// Contenedor que garantiza auto-destrucción en RAII al salir de ámbito
using SecureByteVector = std::vector<uint8_t, SecureAllocator<uint8_t>>;
using SecureString = std::basic_string<char, std::char_traits<char>, SecureAllocator<char>>;

}} // namespace cbdos::crypto
```

---

### 3.4. Eliminación de Busy-Wait en `KerberosManager` (FIDO2 / CTAP2)

#### Problema Detectado:
En `core/src/security/kerberos/KerberosManager.cpp:169-222`:
```cpp
// CÓDIGO ACTUAL DEFICIENTE:
while (self->m_pendingPresence) {
    delay(10);
    lv_timer_handler();
    cbdos::ui::update();
}
```
Esto genera un bloqueo activo consumiendo el 100% de un core RISC-V durante los 30 segundos de espera de presencia de usuario.

#### Solución Arquitectónica:
Desacoplamiento asíncrono basado en un semáforo de sincronización RTOS y eventos reactivos en LVGL 9.5:

```text
  [ CTAP2 Client Thread ]                [ UI / LVGL Thread ]
            │                                      │
            ├─── Genera Solicitud Presencia ──────►│ (Muestra Modal LVGL 9.5)
            │    (Inyecta evento en cola UI)       │
            │                                      │ [ Usuario toca "Aprobar"
            ├─── Espera Semáforo Bloqueante        │   o pulsa botón físico]
            │    (Consumo CPU: 0%)                 │
            │                                      ├─── Libera Semáforo ───┐
            │◄─────────────────────────────────────┴───────────────────────┘
            │
      [ Continúa CTAP2 ]
```

1. **Primitiva de Sincronización:** El `KerberosManager` aloja un manejador de sincronización (`cbdos::os::BinarySemaphore`).
2. **Suspensión Limpia de Tarea:** La tarea FIDO2 entra en estado *Blocked* mediante `sem.take(timeoutMs = 30000)`, liberando los ciclos de CPU por completo.
3. **Manejador de UI Reactivo:** La interfaz LVGL muestra el diálogo modal de autorización sin interrumpir la tasa de refresco a 60 FPS ni interferir con otras tareas. Si el usuario presiona "Autorizar" o expira el tiempo límite, se desbloquea el semáforo.

---

### 3.5. Modelo de Seguridad y Sandbox para Aplicaciones Lua

CBDos ejecuta aplicaciones de terceros mediante el motor Lua integrado (`core/src/lua/`). Para evitar que un script malicioso extraiga secretos del Cyberdeck o sobreescriba firmware:

#### A. Aislamiento de Sistema de Archivos (VFS Jail)
* Los scripts Lua solo tienen visibilidad sobre su propio directorio:
  `/sdcard/apps/data/<app_id>/`
* Todo intento de abrir rutas como `/sdcard/system/`, `/nvs/`, `/spiffs/` o directorios de otras aplicaciones es interceptado por el runtime en C++ retornando `nil, "Permission Denied (Jail Sandbox)"`.

#### B. Modelo de Permisos Declarativos (Manifest)
Cada aplicación debe incluir un archivo `manifest.json` que declare sus capacidades requeridas:
```json
{
  "id": "tactical_tracker",
  "name": "Tactical GPS Tracker",
  "version": "1.0.0",
  "permissions": [
    "network.lora",
    "hardware.gps"
  ]
}
```
Si una app intenta invocar `cbdos.hid.send()` sin haber declarado `"hardware.badusb"`, la API de enlace C++ bloquea la ejecución de inmediato y genera una advertencia de seguridad en el registro del sistema.

#### C. Límite de Recursos (Quota & Watchdog)
* **Memoria RAM:** Asignación máxima de 512 KB de PSRAM por instancia de VM Lua. Si un script intenta crear tablas infinitas, el custom allocator de Lua aborta con error de memoria sin tumbar el sistema operativo.
* **Tiempo de CPU:** Hook de conteo de instrucciones (`lua_sethook`) cada 10,000 instrucciones virtuales para detectar y abortar bucles infinitos no cedentes (`while true do end`).

---

### 3.6. Gestión de Ataques BadUSB y Políticas de Inyección Táctil

Para evitar que scripts maliciosos o accidentes operacionales disparen cargas dañinas hacia un equipo anfitrión conectado por USB:

```text
┌─────────────────────────────────────────────────────────────────────────────┐
│                       POLÍTICAS DE EJECUCIÓN BADUSB                         │
├─────────────────────────────────────────────────────────────────────────────┤
│ 1. PERMISSIVE (Desarrollo / Auditorías Autorizadas):                        │
│    • Ejecuta cualquier script .dd desde SD o terminal.                      │
│                                                                             │
│ 2. PROMPT-ON-RUN (Nivel por Defecto de CBDos):                             │
│    • Analiza el script antes de enviar pulsaciones HID.                     │
│    • Muestra ventana de inspección táctil en pantalla:                      │
│      - Número total de teclas.                                              │
│      - Comandos sensibles detectados (ej. powershell, cmd, bash, curl, nc). │
│      - Retardo total estimado.                                              │
│    • Requiere confirmación física manual del operador en pantalla táctil.   │
│                                                                             │
│ 3. ENFORCE-HASH / AUDITED:                                                  │
│    • Solo ejecuta scripts cuyos hashes SHA-256 hayan sido pre-aprobados en  │
│      la configuración local cifrada (`vault:badusb.whitelist`).             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 4. Matriz de Riesgos y Mitigaciones Actualizada

| Vector de Ataque | Estado Actual (v0.2.1) | Mitigación Propuesta en este Borrador | Nivel de Seguridad Resultante |
| :--- | :--- | :--- | :---: |
| **Extracción Física de NVS (Flash Dump)** | Claves WiFi y parámetros guardados en texto claro. | `cbdos::security::Vault` con **AES-256-GCM** + Salt por silicio eFuse. | 🟢 Alta (Criptográficamente Fuerte) |
| **Agotamiento de CPU en FIDO2** | Bucle ocupado `while` con `delay(10)` en `KerberosManager`. | Sincronización asíncrona mediante semáforo FreeRTOS + Modal no bloqueante LVGL. | 🟢 Alta (0% de CPU en espera) |
| **Inspección de Claves en RAM (Forense)** | Strings y vectores C++ estándar en PSRAM sin borrado seguro. | Tipos `SecureString` y `cleanse()` inmunes a optimizaciones de compilador. | 🟢 Alta (Zeroization estricto) |
| **Falsificación de Paquetes en Malla LoRa** | No autenticado en implementaciones preliminares. | Filtro Early Drop HMAC-SHA256 (<2 µs) + Nonce anti-replay de 32 bits. | 🟢 Alta (Anti-Spoofing & Anti-Replay) |
| **Inyección Maliciosa desde Scripts Lua** | Acceso irrestricto al sistema de archivos del sistema. | VFS Jail por directorio de aplicación + Declaración estricta de permisos. | 🟢 Alta (Sandbox de Aplicaciones) |
| **Disparo Accidental de Cargas BadUSB** | Ejecución directa de scripts `.dd` sin validación. | Política *Prompt-on-Run* con desglose de comandos y confirmación manual táctil. | 🟢 Alta (Control Físico del Operador) |

---

## 5. Plan de Implementación por Fases (Paso a Paso)

### Fase 1: Abstracción Criptográfica y Tipos Seguros
* [ ] Crear `core/include/cbdos/crypto.hpp` con la interfaz abstracta `ICryptoProvider`.
* [ ] Implementar el backend de `ICryptoProvider` en `bsp/esp32_p4_jc4880` reutilizando el silicio mbedTLS existente.
* [ ] Implementar el backend de `ICryptoProvider` en `bsp/esp32_s3_jc3248` asegurando compatibilidad con el entorno PlatformIO/Arduino.
* [ ] Implementar `core/include/cbdos/secure_mem.hpp` (`cleanse` y `SecureBuffer`).

### Fase 2: Almacenamiento Seguro (`Vault`) y Migración de `ConfigManager`
* [ ] Implementar la clase `cbdos::security::Vault` en `core/src/security/Vault.cpp`.
* [ ] Refactorizar `ConfigManager::loadWiFi` y `saveWiFi` para usar `Vault::getSecret` y `Vault::setSecret`.
* [ ] Migrar las claves de red LoRa/Mesh a registros cifrados del Vault.
* [ ] Implementar rutina de migración transparente: si existen credenciales en texto plano en el NVS viejo, se cifran de inmediato y se borra el registro plano.

### Fase 3: Desacoplamiento Asíncrono de Kerberos / FIDO2
* [ ] Reemplazar el bucle ocupado en `core/src/security/kerberos/KerberosManager.cpp` por un semáforo binario asíncrono.
* [ ] Crear el componente visual LVGL 9.5 `KerberosPresenceDialog` para confirmar la autorización de forma limpia y reactiva.

### Fase 4: Seguridad en Runtimes (Lua & BadUSB)
* [ ] Implementar el VFS Jail en las funciones de apertura de archivos del puente Lua.
* [ ] Integrar el analizador estático de DuckyScript para la pantalla de confirmación previa (*Prompt-on-Run*).
* [ ] Validar compilación dual limpia en ESP-IDF 5.5 y PlatformIO.

---

*Borrador de especificación técnica formulado y preparado para revisión y aprobación.*
