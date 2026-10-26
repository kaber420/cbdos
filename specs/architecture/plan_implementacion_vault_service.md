# Plan Técnico: Sistema de Bóveda Segura y Portabilidad (VaultService)

**Documento:** `specs/architecture/plan_implementacion_vault_service.md`  
**Módulo:** `cbdos::security::VaultService`  
**Estado:** 📋 Especificado / Listo para Aprobación  
**Fecha:** 24 de Octubre de 2026  
**Autor:** kaber420 & Asistente Senior de Sistemas Embebidos  
**Restricción Crítica:** ⚠️ **Cero quema de eFuses / Cero Secure Boot permanente.** Todo opera mediante criptografía matemática pura (AES-256-GCM + PBKDF2).

---

## 1. Propósito y Responsabilidades

`VaultService` es el servicio central y universal de custodia criptográfica en CBDos. Su propósito es actuar como el **Guardián Universal de Secretos y Claves** para todo el sistema operativo:
- **Espacios de Red e Infraestructura (`wisp`, `ssh`, `mikrotik`):** Credenciales de Access Points, routers y llaves SSH.
- **Espacio de Redes (`wifi`):** Claves WPA2/WPA3 guardadas para auto-conexión segura.
- **Espacio de Radio y Mensajería (`mesh`, `cbdbbs`):** Claves maestras AES de salas de chat y llaves de identidad de radio (LoRa/FLRC).
- **Espacio de Autenticación (`fido2`, `tokens`):** Semillas de tokens y claves de APIs externas.

### Arquitectura Híbrida: MessagePack Document-Store + Secretos Cifrados
Para garantizar velocidad instantánea, cero overhead y compatibilidad binaria estándar:
1. **Base de Datos Binaria MessagePack (`/cbdos/vault/<namespace>.db`):** 
   - Cada namespace se serializa en un Document-Store binario usando [`cbdos::msgpack::MsgPackWriter`](../../core/include/cbdos/msgpack_util.hpp) y se parsea con `MsgPackReader`.
   - Soporta documentos con campos dinámicos y opcionales sin tamaño fijo ni desperdicio de memoria.
2. **Índice Público de Metadatos:** ID, Etiqueta descriptiva, Host/IP, Puerto, Usuario y Tipo se indexan en campos abiertos de MessagePack, permitiendo a la UI/LVGL y a Lua buscar y listar dispositivos instantáneamente sin invocar criptografía.
3. **Secreto Cifrado Atómicamente con AES-256-GCM:** Únicamente el secreto sensible (`secret`) se almacena cifrado en binario dentro del documento, con su Vector de Inicialización (IV) de 12 bytes y Authentication Tag (16 bytes), derivado de `(PIN + ChipID + Salt_NVS)`.
4. **Cero-Trace en RAM:** Al bloquear el sistema (`lock()`), se purga la clave maestra volátil mediante `memset_s()`.

---

## 2. API Pública en C++ (`core/include/cbdos/vault.hpp`)

```cpp
#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace cbdos::security {

struct VaultItem {
    std::string id;         // Identificador único ej: "ap-torre-norte"
    std::string label;      // Nombre amigable ej: "Sectorial Norte 5G (Rocket AC)"
    std::string host;       // IP, FQDN o contexto ej: "192.168.1.20"
    uint16_t    port = 0;   // Puerto opcional (ej: 443, 22, o 0 si no aplica)
    std::string user;       // Usuario opcional (ej: "ubnt", "admin")
    std::string secret;     // Contraseña descifrada (solo presente si la bóveda está desbloqueada)
    std::string type;       // Tipo/Namespace: "ubiquiti", "mikrotik", "ssh", "wifi", "mesh", "token"
    std::string notes;      // Metadatos o comentarios opcionales
};

class VaultService {
public:
    static VaultService& getInstance();

    // Inicialización y Estado
    bool init();
    bool isUnlocked() const;
    bool unlock(const std::string& userPin);
    void lock();

    // Gestión de Credenciales Estructuradas
    bool saveItem(const std::string& ns, const VaultItem& item);
    bool getItem(const std::string& ns, const std::string& id, VaultItem& outItem);
    bool findByHost(const std::string& ns, const std::string& host, VaultItem& outItem);
    bool deleteItem(const std::string& ns, const std::string& id);
    bool listItems(const std::string& ns, std::vector<VaultItem>& outItems, bool includeSecrets = false);

    // Primitivas Criptográficas On-Demand (Para Chats Mesh, Notas o Cifrado de Archivos)
    bool encryptData(const uint8_t* plaintext, size_t plainLen, std::vector<uint8_t>& outCiphertext);
    bool decryptData(const uint8_t* ciphertext, size_t cipherLen, std::vector<uint8_t>& outPlaintext);

    // Exportación e Importación Portable (.cbdvault)
    bool exportNamespace(const std::string& ns, const std::string& filePath, const std::string& exportPassphrase);
    bool importNamespace(const std::string& ns, const std::string& filePath, const std::string& importPassphrase);

private:
    VaultService() = default;
    bool deriveLocalKey(const std::string& pin, uint8_t outKey[32]);
    
    bool m_unlocked = false;
    uint8_t m_activeKey[32] = {0};
};

} // namespace cbdos::security
```

---

## 3. Comandos de Consola Shell (`vault`)

```text
CBDOS > vault status
Bóveda: DESBLOQUEADA | Espacios activos: ubnt, mikrotik, wifi

CBDOS > vault export ubnt /sdcard/respaldo_torres.cbdvault
[?] Introduce contraseña de exportación: **********
[OK] Bóveda 'ubnt' exportada exitosamente (AES-256-GCM / 50000 iters).

CBDOS > vault import ubnt /sdcard/respaldo_torres.cbdvault
[?] Introduce contraseña del respaldo: **********
[OK] 12 credenciales importadas y re-cifradas en el almacenamiento local.
```

---

## 4. Integración con `LockService` y Erradicación del PIN en Texto Plano

Actualmente, `LockService.cpp` almacena el PIN del usuario en **texto plano directo** dentro de la NVS (`be->setString("pin", newPin)`), lo cual representa una vulnerabilidad de seguridad.

### 4.1. El Modelo de Autenticación Unificada (Zero-Knowledge Hash)
Para evitar la paradoja de guardar el PIN dentro de la propia bóveda que el PIN abre, se implementa una arquitectura en dos ramas:

```
[ Usuario ingresa PIN en pantalla/terminal ] ─── (Ej. 1234)
              │
              ├───> Rama 1: Verificación de Identidad (LockService)
              │        Calcula: PIN_Digest = PBKDF2-HMAC-SHA256(PIN, Salt_NVS, iter=10000, len=32)
              │        Compara contra el Hash guardado en NVS (¡NUNCA el PIN en claro!).
              │        Si coincide: Desbloquea la interfaz de usuario (UI / LVGL).
              │
              └───> Rama 2: Desbloqueo de Bóveda (VaultService)
                       Inmediatamente se invoca: VaultService::getInstance().unlock(PIN);
                       Deriva la Clave Maestra AES-256 en memoria RAM volátil.
                       Las aplicaciones (Ubiquiti, MikroTik, Wi-Fi) pueden operar.
```

### 4.2. Bloqueo Automático Total (Auto-Purge en RAM)
- Cuando el sistema entra en reposo, timeout de inactividad o el usuario pulsa "Bloquear":
  1. `LockService::getInstance().lock()` bloquea la interfaz de usuario.
  2. `VaultService::getInstance().lock()` ejecuta `memset_s()` sobre `m_activeKey`, destruyendo la clave criptográfica de la memoria RAM.
  3. Ningún dato sensible queda expuesto en memoria volátil mientras el equipo esté bloqueado.

---

## 5. Fases de Implementación

- [ ] **Paso 0 (Modernización de `LockService`):**
  - Generar un `Salt` aleatorio de 16 bytes en la partición NVS.
  - Reemplazar el almacenamiento en texto plano del PIN por un hash derivado con `PBKDF2-HMAC-SHA256`.
  - Migración automática transparente del PIN por defecto `"1234"`.
  - Vincular `LockService::unlock()` para que invoque automáticamente `VaultService::unlock(pin)` y `LockService::lock()` para purgar `VaultService::lock()`.
- [ ] **Paso 1:** Implementar backend criptográfico con mbedTLS (`aes_gcm` y `pbkdf2`) en `core/src/security/VaultService.cpp`.
- [ ] **Paso 2:** Persistencia de archivos locales en `/cbdos/vault/` mediante la capa `IStorage`.
- [ ] **Paso 3:** Formato binario de exportador e importador `.cbdvault` (64 bytes de cabecera estándar).
- [ ] **Paso 4:** Comandos `vault` en la Shell del sistema (`status`, `export`, `import`).
- [ ] **Paso 5:** Script en PC `tools/cbdvault_tool.py` para gestión y verificación cruzada de respaldos en computadora.

