# Especificación y Plan: Bóveda de Credenciales Segura y Portabilidad (VaultService)

**Documento:** `specs/architecture/especificacion_boveda_segura_y_portabilidad_vault.md`  
**Módulo:** `cbdos::security::VaultService`  
**Estado:** 📋 Especificado / Listo para Aprobación  
**Fecha:** 24 de Octubre de 2026  
**Autor:** kaber420 & Asistente Senior de Sistemas Embebidos  
**Precedencia:** Base de seguridad para almacenamiento de credenciales de red (APs Ubiquiti, routers MikroTik, llaves SSH y contraseñas Wi-Fi).

---

## 1. Principio Fundamental: Cero Quema Irreversible de eFuses

> [!IMPORTANT]
> **REGLA DE ORO DE SEGURIDAD FÍSICA:**  
> **Está estrictamente prohibido quemar eFuses de forma irreversible** durante el desarrollo de CBDos.  
> Los eFuses OTP (One-Time Programmable) son permanentes; si se comete un error, la placa queda inutilizada o bloqueada para depuración (JTAG/Serial).

### ¿Cómo logramos seguridad de grado hardware SIN quemar eFuses?
En lugar de quemar claves permanentes en los bloques OTP del chip, aprovechamos los **eFuses de solo lectura de fábrica** que ya vienen grabados por Espressif en el silicio:
1. **Identificador Único de Silicio (Unique Chip ID / MAC Factory)**:
   - Grabado en la oblea durante la manufactura de Espressif en los registros `EFUSE_BLOCK0` / `EFUSE_BLOCK1`.
   - No se puede falsificar por software en hardware físico real.
2. **Semilla de Entropía Verdadera (True Hardware RNG)**:
   - El ESP32 cuenta con un generador de números aleatorios por ruido térmico de RF (`esp_random()`).
3. **Secreto del Dispositivo en NVS Protegida (`Device Salt`)**:
   - Durante el primer inicio ("Provisioning inicial"), CBDos genera un `Device Salt` criptográfico aleatorio de 32 bytes y lo almacena en la partición NVS del sistema.

Con esto, la seguridad del dispositivo físico queda garantizada **sin riesgo de brickeo ni modificaciones irreversibles de hardware**.

---

## 2. Arquitectura de Dos Capas (Local vs Portable)

El sistema resuelve el dilema entre **seguridad atada al dispositivo** y **capacidad de respaldo / migración a otros dispositivos**:

```
                              +----------------------------+
                              |   Credenciales en Claro    |
                              |   (RAM Temporal Volátil)   |
                              +----------------------------+
                                             |
                     +-----------------------+-----------------------+
                     |                                               |
         [ BÓVEDA LOCAL DEL DISPOSITIVO ]                [ ARCHIVO DE EXPORTACIÓN / BACKUP ]
          Archivo: /cbdos/vault/ubnt.db                   Archivo: /sdcard/backup.cbdvault
                     |                                               |
          * Atada al chip físico actual.                  * Totalmente portátil (otro ESP32 o PC).
          * Clave en RAM derivada de:                     * Clave derivada exclusivamente de:
            K_local = PBKDF2(PIN, ChipID + Salt)           K_export = PBKDF2(Passphrase_Backup, Salt)
          * Cifrado: AES-256-GCM (Hardware)               * Cifrado: AES-256-GCM (Hardware/Software)
          * No se puede abrir en PC ni en otro ESP32.     * Se abre con la contraseña elegida al exportar.
```

---

## 3. Especificación Criptográfica Detallada

### 3.1. Bóveda Local (`*.db`)
- **Algoritmo**: `AES-256-GCM` (Autenticado: garantiza confidencialidad e integridad anti-tamper).
- **Aceleración**: Motor DMA AES por hardware de ESP32-S3 y ESP32-P4 vía mbedTLS.
- **Derivación de Clave Local ($K_{local}$)**:
  $$K_{local} = \text{PBKDF2-HMAC-SHA256}(\text{PIN\_Usuario},\ \text{ChipID} \parallel \text{DeviceSalt},\ \text{iter}=10000,\ \text{len}=32)$$
  - Si se extrae la memoria Flash o la MicroSD y se lee en una PC: el archivo es ruido aleatorio (falta el `ChipID` de silicio y el `PIN`).
  - Si un atacante roba el ESP32 encendido pero bloqueado: el PIN no está en memoria; el sistema requiere el desbloqueo por `LockService`.

### 3.2. Contenedor de Exportación Portable (`*.cbdvault`)
El archivo de exportación utiliza un formato binario estructurado y estándar que puede ser descifrado tanto por otro dispositivo CBDos como por un script Python en tu computadora:

#### Estructura del Archivo `.cbdvault`:
```
+--------------------------------------------------------------------------+
| CABECERA BINARIA (64 Bytes)                                              |
+-------------------+--------------------+---------------------------------+
| Magic (4B)        | Versión (2B)       | KDF Iterations (4B) [Ej. 50000] |
| "CBDV"            | 0x0001             | Big-Endian                      |
+-------------------+--------------------+---------------------------------+
| Salt KDF (16B)                         | IV / Nonce AES-GCM (12B)        |
| Criptográficamente aleatorio (TRNG)    | Único por archivo generado      |
+-------------------+--------------------+---------------------------------+
| Tag GCM (16B)                          | Reservado / Padding (10B)       |
| Etiqueta de integridad y autenticación | Para futuras extensiones        |
+----------------------------------------+---------------------------------+
| PAYLOAD CIFRADO (Longitud Variable)                                      |
| Bloque cifrado AES-256-GCM conteniendo el JSON comprimido de la base     |
| de datos de credenciales.                                                |
+--------------------------------------------------------------------------+
```

---

## 4. Flujo Operativo: Exportación e Importación

### 4.1. Proceso de Exportación (Backup a MicroSD o USB)
1. El usuario ejecuta en la consola de CBDos o en la UI:
   `vault export ubnt /sdcard/respaldo_torres.cbdvault`
2. Si el sistema está bloqueado, solicita el **PIN de Usuario** para autorizar el acceso a la memoria RAM.
3. El sistema solicita al usuario:
   - `Introduce contraseña para el archivo de exportación:`
   - `Confirma la contraseña:`
4. El generador TRNG obtiene 16 bytes de `Salt` y 12 bytes de `IV`.
5. Se deriva la clave de exportación:
   $$K_{export} = \text{PBKDF2-HMAC-SHA256}(\text{Passphrase},\ \text{Salt},\ \text{iter}=50000,\ \text{len}=32)$$
6. Se cifra el JSON de credenciales con `AES-256-GCM`.
7. Se escribe la cabecera `CBDV` junto con el payload cifrado en el destino.
8. La memoria RAM temporal se limpia inmediatamente con `memset_s()`.

### 4.2. Proceso de Importación (Restauración en un Nuevo ESP32)
1. El usuario inserta la MicroSD en el nuevo dispositivo y ejecuta:
   `vault import /sdcard/respaldo_torres.cbdvault`
2. El sistema lee los primeros 64 bytes (`Magic == "CBDV"`).
3. Solicita la **contraseña del archivo de respaldo**.
4. Deriva $K_{export}$ usando el `Salt` y número de iteraciones leídos de la cabecera.
5. Ejecuta `AES-256-GCM` decrypt con validación de etiqueta de autenticación (Tag):
   - **Si la contraseña es incorrecta o el archivo fue alterado**: La validación de Tag falla inmediatamente sin tocar la base local. Retorna `Error: Contraseña inválida o archivo corrupto`.
6. Si la autenticación es exitosa, los datos se descifran en RAM.
7. Se llama a `VaultService::saveLocal("ubnt", plaintext)`:
   - El nuevo dispositivo cifra los datos con **su propia clave de silicio local ($K_{local\_nuevo}$)** y los almacena en su partición interna `/cbdos/vault/ubnt.db`.
8. Se borra la memoria RAM temporal.

---

## 5. Script de Compatibilidad para PC (`tools/cbdvault_tool.py`)

Para que nunca quedes atrapado sin acceso a tus respaldos, se proporciona una herramienta Python de código abierto para tu computadora que puede empaquetar, desencriptar o inspeccionar archivos `.cbdvault`:

```bash
# Ver contenido de un respaldo desde tu PC
python3 tools/cbdvault_tool.py inspect /media/sdcard/respaldo_torres.cbdvault

# Desencriptar a JSON legible
python3 tools/cbdvault_tool.py decrypt /media/sdcard/respaldo_torres.cbdvault --out aps.json

# Encriptar un nuevo JSON desde PC para importarlo en CBDos
python3 tools/cbdvault_tool.py encrypt aps.json --out nuevo_respaldo.cbdvault
```

---

## 6. Fases de Implementación

- [ ] **Fase 1: Servicio Core de Criptografía (`VaultService`)**
  - Implementar `core/include/cbdos/vault.hpp` y `core/src/security/VaultService.cpp`.
  - Enlazar con las funciones aceleradas por hardware de mbedTLS (`mbedtls_gcm_*`, `mbedtls_pkcs5_pbkdf2_hmac`).
- [ ] **Fase 2: Almacenamiento Local de APs Ubiquiti**
  - Integrar `UbiquitiApManager` con `VaultService`.
  - Probar guardado y lectura cifrada local en Flash/NVS/SD.
- [ ] **Fase 3: Exportador e Importador de Contenedores `.cbdvault`**
  - Implementar la serialización binaria con cabecera de 64 bytes.
  - Implementar comandos de consola `vault export` y `vault import`.
- [ ] **Fase 4: Herramienta de Soporte en PC (`tools/cbdvault_tool.py`)**
  - Crear script estándar con la librería `cryptography` de Python para validación cruzada PC $\leftrightarrow$ ESP32.
