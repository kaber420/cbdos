# Plan de Arquitectura y Portado: KERBEROS FIDO2 / U2F Hardware Passkeys para CBDos

Este documento define la arquitectura técnica, abstracciones HAL, flujo criptográfico y diseño de interfaz gráfica en **LVGL 9.5** para portar el motor **KERBEROS** (FIDO2 / WebAuthn y U2F) a **CBDos**.

---

## 🎯 1. Visión General del Módulo
**KERBEROS** convierte al Cyberdeck en una llave de seguridad física de hardware (equivalente a una *YubiKey 5 / Titan Key*) que se conecta por el puerto **USB-OTG** a una PC, tablet o teléfono para:
* **Autenticación sin contraseñas (Passkeys / WebAuthn):** Inicio de sesión biométrico/PIN en Google, GitHub, Microsoft, Apple, AWS, Bitwarden, etc.
* **Segundo Factor de Autenticación (2FA / U2F):** Reemplazo seguro e inmune al phishing frente a SMS o códigos TOTP.
* **Confirmación Visual Anti-Phishing:** En la pantalla del Cyberdeck se muestra el nombre real de la web solicitante (*Relying Party ID*, ej. `github.com`) y se exige pulsar físicamente **[Aprobar]** o ingresar PIN antes de firmar.

---

## 🧩 2. Estructura Arquitectónica y Desacoplamiento

Siguiendo las reglas estrictas de CBDos, el módulo se divide en 3 capas desacopladas:

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           CAPA DE APLICACIÓN                            │
│                  core/src/apps/kerberos/KerberosView.cpp                │
│    • Pantalla de Aprobación de Acceso con Relying Party (LVGL 9.5)      │
│    • Teclado numérico PIN táctil y gestión de Passkeys guardadas        │
└────────────────────────────────────┬────────────────────────────────────┘
                                     │ Eventos & Callbacks de Presencia
┌────────────────────────────────────▼────────────────────────────────────┐
│                             NÚCLEO FIDO2                                │
│                     core/src/security/kerberos/                         │
│    • ctap2.cpp / ctap2.h       : Máquina de estados FIDO2 / WebAuthn    │
│    • u2f.cpp / u2f.h           : Compatibilidad con U2F / CTAP1         │
│    • ctaphid.cpp / dispatch.h  : Enmarcado USB HID (64 bytes)           │
│    • authdata.cpp / cose.cpp   : Formateador de datos y llaves COSE     │
│    • kerb_crypto.cpp           : Criptografía NIST P-256 con mbedTLS    │
│    • cred_store.cpp            : Gestor de credenciales persistentes    │
└────────────────────────────────────┬────────────────────────────────────┘
                                     │ Interfaces HAL C++
┌────────────────────────────────────▼────────────────────────────────────┐
│                              CAPA BSP / HAL                             │
│                  bsp/.../hal_usb_fido / hal_storage_nvs                 │
│    • Transporte USB-OTG (Endpoints HID Interrupción 64B)                │
│    • Persistencia en Flash NVS (Clave maestra y credenciales)           │
│    • mbedTLS Acelerado por Hardware (ESP32-P4 / ESP32-S3)               │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 🔐 3. Especificación Criptográfica y Protocolos

### A. Algoritmos Criptográficos (Vía `mbedTLS` acelerado por hardware):
1. **Curva Elíptica:** ECDSA sobre curva **NIST P-256 (`secp256r1`)** para firmas de aserción y registro.
2. **Hashing:** **SHA-256** para el cálculo de `clientDataHash` y `rpIdHash`.
3. **Cifrado de Key Handles:** **AES-256-CBC / HMAC-SHA256** con la llave maestra del dispositivo para credenciales no residentes (*stateless key wrapping*).

### B. Identificador AAGUID de CBDos:
* **AAGUID (16 bytes):** `CBDOS-FIDO2-KEY-V1` (`43 42 44 4F 53 2D 46 49 44 4F 32 2D 4B 45 59 31`).

---

## 🖥️ 4. Diseño de la Interfaz de Usuario (LVGL 9.5)

La vista `KerberosView` proporciona una experiencia interactiva premium:

```
 ┌──────────────────────────────────────────────────────────────┐
 │  🔒 KERBEROS FIDO2                         🔋 85%  [ ✕ ]     │
 ├──────────────────────────────────────────────────────────────┤
 │                                                              │
 │                   ⚠️ SOLICITUD DE ACCESO                     │
 │                                                              │
 │   Sitio Web:  github.com                                     │
 │   Usuario:    kaber420                                       │
 │   Operación:  Inicio de sesión con Passkey                   │
 │                                                              │
 │   ┌────────────────────────┐    ┌────────────────────────┐   │
 │   │      [ RECHAZAR ]      │    │    [ APROBAR (✓) ]     │   │
 │   └────────────────────────┘    └────────────────────────┘   │
 │                                                              │
 ├──────────────────────────────────────────────────────────────┤
 │  🟢 USB Conectado y Listo | 4 Passkeys en Almacenamiento NVS │
 └──────────────────────────────────────────────────────────────┘
```

### Características de la UI:
1. **Modal de Aprobación de Presencia:** Aparece automáticamente cuando el navegador envía una solicitud `getAssertion` o `makeCredential`.
2. **Anti-Timeout KeepAlive:** Mientras el usuario lee la pantalla o introduce su PIN, la tarea de fondo envía paquetes `CTAPHID_KEEPALIVE` (status `STATUS_UPNEEDED`) para que el navegador nunca corte la conexión por tiempo de espera.
3. **Gestor de Passkeys Guardadas:** Lista táctil deslizable con todas las cuentas residentes en la NVS con opción de ver fecha de creación o borrarlas.
4. **Protección por PIN:** Opcionalmente requiere introducir un PIN numérico en pantalla antes de liberar la firma.

---

## 📂 5. Fases de Implementación

### Fase 1: Importación de Core Criptográfico y TinyCBOR
* Integrar `tinycbor` y los módulos independientes de `lib/kerberos_core/` en `core/src/security/kerberos/`.
* Vincular las funciones criptográficas con `mbedtls/ecdsa.h`, `mbedtls/sha256.h` y `mbedtls/aes.h`.

### Fase 2: Capa HAL de Transporte USB FIDO
* Añadir el descriptor de interfaz **FIDO CTAPHID** (Usage Page `0xF1D0`, Usage `0x01`, Report Size 64 bytes) a la pila USB-OTG de **ESP32-P4** y **ESP32-S3**.
* Implementar `poll()` no bloqueante con colas de mensajes hacia la UI.

### Fase 3: Persistencia en Flash NVS
* Implementar el almacenamiento de claves maestras y credenciales residentes en la partición NVS.

### Fase 4: Interfaz Gráfica LVGL 9.5
* Crear `core/src/apps/kerberos/KerberosView.cpp` y su registro en el lanzador de aplicaciones de CBDos.

### Fase 5: Validación y Pruebas
* Pruebas con la suite de verificación oficial de Yubico (`python-fido2`).
* Pruebas de registro y login real en **Google Accounts**, **GitHub Passkeys** y **WebAuthn.io**.
