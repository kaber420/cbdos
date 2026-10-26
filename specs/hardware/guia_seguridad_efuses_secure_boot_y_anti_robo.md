# Guía Técnica: Ciclo de Vida de Seguridad, Aprovisionamiento de eFuses y Blindaje Anti-Robo

**Documento:** `specs/hardware/guia_seguridad_efuses_secure_boot_y_anti_robo.md`  
**Categoría:** Hardware Security / Hardening / Anti-Tamper  
**Estado:** 📋 Especificado / Documento Canónico  
**Fecha:** 24 de Octubre de 2026  
**Autor:** kaber420 & Asistente Senior de Sistemas Embebidos  
**Ámbito:** Dispositivos CBDos (CyberDecks de campo), Nodos TableHub / Torres WISP, y Hardware Comercial Dedicado.

---

## 1. Visión Estratégica: La Amenaza del "Robo para Reutilización"

> [!CAUTION]
> **El Vector de Riesgo Real:**  
> Un ESP32-S3 o ESP32-P4 en una caja impresa en 3D en campo (torre, poste, mesa de control o cyberdeck) normalmente tiene bajo valor de reventa física por ser componentes baratos.  
> **Sin embargo, si cualquier ladrón o técnico deshonesto puede conectar un cable USB, borrar el firmware y cargar GhostESP, Bruce, Marauder o cualquier firmware ofensivo, el dispositivo se convierte de inmediato en un objetivo de alto valor para robo.**

Para anular este incentivo de robo y proteger las credenciales críticas de la red (routers MikroTik, APs Ubiquiti, claves privadas WPA3/SSH), el hardware debe implementar un **ciclo de vida de seguridad en dos fases**:

---

## 2. Las Dos Fases del Ciclo de Vida del Hardware

```
[ FASE 1: DESARROLLO Y LABORATORIO ]            [ FASE 2: HARDWARE EN PRODUCCIÓN / TABLEHUB ]
  (Dispositivos de Pruebas y Taller)              (Equipos en Torres, Clientes o Despliegues)
---------------------------------------         ---------------------------------------------
* eFuses OTP: INTACTOS (Cero quema).            * eFuses OTP: QUEMADOS y BLOQUEADOS (RD_DIS).
* USB/JTAG: Abiertos para depuración.           * USB Download Mode: RESTRINGIDO / DESHABILITADO.
* Secure Boot: DESACTIVADO.                     * Secure Boot v2: ACTIVADO (Firma RSA-3072 / ECDSA).
* Flash Encryption: DESACTIVADO.                * Flash Encryption: ACTIVADO (XTS-AES-256 por hardware).
* Bóveda de Claves: Derivación matemática       * Bóveda de Claves: HMAC Peripheral en Silicio.
  (KDF + DeviceSalt en NVS + PIN Usuario).
* RIESGO DE BRICKEAR: CERO.                     * ANTI-ROBO: TOTAL (Inutilizable para terceros).
```

---

## 3. Fase 1: Desarrollo Seguro (Lo que hacemos HOY)

Durante la etapa actual de desarrollo de CBDos:
- **No se quema ningún eFuse de forma permanente.** El chip puede borrarse por completo con `esptool.py erase_flash` cuantas veces sea necesario.
- La protección de credenciales reside en la capa **matemática y criptográfica**:
  1. `VaultService` utiliza `AES-256-GCM` acelerado por el hardware mbedTLS del ESP32.
  2. La clave se deriva mediante `PBKDF2-HMAC-SHA256` con el PIN del usuario y el ID de fábrica del chip.
  3. Los archivos de exportación `.cbdvault` viajan cifrados independientemente para copias de seguridad.

---

## 4. Fase 2: Blindaje de Producción (Cuándo y Cómo Asegurar)

### ¿Cuándo dar el paso a la Fase 2?
Se debe aplicar el blindaje irreversible **únicamente cuando se cumplan las siguientes condiciones**:
1. El firmware de CBDos o TableHub esté en una versión estable de producción (Release Gold).
2. El dispositivo vaya a ser instalado de forma permanente en campo (ej. nodo TableHub en un poste/torre o equipo entregado a un usuario final).
3. Tengas respaldada tu **clave privada de firma de firmware (`secure_boot_signing_key.pem`)** en al menos dos medios offline seguros (USB cifrada, caja fuerte).

---

### Los 4 Candados del Blindaje de Producción:

#### Candado 1: Secure Boot v2 (Protección contra GhostESP / Marauder)
- **Objetivo:** Impedir que el dispositivo ejecute cualquier binario no firmado por ti.
- **Mecanismo:**
  1. En tu PC se genera una clave RSA-3072 o ECDSA.
  2. El hash público (`digest`) de esa clave se quema en el eFuse `BLOCK_KEY0` o `BLOCK_KEY1` con propósito `SECURE_BOOT_DIGEST`.
  3. Se quema el bit de eFuse `SECURE_BOOT_EN`.
- **Efecto Anti-Robo Inmediato:**
  - Si alguien roba el dispositivo, lo conecta a su PC e intenta flashear `GhostESP.bin` o cualquier otro firmware: **la ROM del ESP32 rechaza el arranque**.
  - El chip entra en pánico de hardware (`Invalid image digest`) y se bloquea.
  - Para el ladrón, el hardware es un "ladrillo inservible", eliminando todo incentivo de robo.

#### Candado 2: Flash Encryption por Hardware (Anti-Extracción de Datos)
- **Objetivo:** Impedir que desuelden la memoria Flash o la lean físicamente para robar configuraciones, contraseñas de APs o topología de red.
- **Mecanismo:**
  1. El ESP32 genera internamente una clave `XTS-AES-256` mediante su generador cuántico/térmico (`TRNG`).
  2. La clave se quema en el bloque eFuse designado.
  3. Se quema el bit de lectura deshabilitada (`RD_DIS`) para ese bloque.
  4. La memoria Flash SPI queda cifrada en reposo. Toda lectura/escritura pasa por el coprocesador AES transparente del bus de memoria.
- **Efecto:**
  - Si extraen el chip Flash o clonan la partición, solo obtienen ruido criptográfico indistinguible de datos aleatorios.

#### Candado 3: Restricción del Modo Descarga USB (`DIS_DOWNLOAD_MODE`)
- **Objetivo:** Cerrar la puerta trasera de lectura y escritura por consola serie ROM.
- **Mecanismo:**
  - Se quema el eFuse `UART_ROM_DL_MODE`:
    - Valor `1` (`UART_ROM_DL_MODE_DISABLED`): Bloqueo total de la ROM de arranque por USB. Solo se actualiza por OTA firmado.
    - O Valor `2` (`UART_ROM_DL_MODE_SECURE`): Solo permite comandos mínimos indispensables para flashear binarios que ya vengan previamente firmados con tu clave privada.
  - Se deshabilita JTAG físico quemando `JTAG_DISABLE`.

#### Candado 4: Clave HMAC en Silicio para la Bóveda (`VaultService`)
- **Objetivo:** Atar las contraseñas de la infraestructura al silicio de forma irreversible.
- **Mecanismo:**
  1. Se quema una clave de 256 bits en un eFuse libre con propósito `HMAC_UP`.
  2. El `VaultService` en producción invoca el periférico de hardware `esp_hmac_calculate()`.
  3. Ni el propio firmware ni ningún software puede leer la clave del eFuse; solo el motor HMAC interno la usa para derivar la llave de la base de datos de APs y routers.

---

## 5. Procedimiento Práctico de Aprovisionamiento en Producción

Cuando un lote de hardware (por ejemplo 10 nodos TableHub) esté listo para montaje en campo, se ejecuta este flujo estandarizado desde la PC de desarrollo:

```bash
# 1. Generar la clave de firma maestra del proyecto (¡GUARDAR CON COPIA DE SEGURIDAD!)
espsecure.py generate_signing_key --version 2 secure_boot_signing_key.pem

# 2. Extraer el digest público de la clave
espsecure.py digest_secure_boot_signing_key --keyfile secure_boot_signing_key.pem --output digest.bin

# 3. Quemar el digest en el bloque de eFuses (ESP32-S3 / P4)
espefuse.py -p /dev/ttyACM0 burn_key BLOCK_KEY0 digest.bin SECURE_BOOT_DIGEST0

# 4. Activar Secure Boot v2 irreversible
espefuse.py -p /dev/ttyACM0 burn_efuse SECURE_BOOT_EN 1

# 5. Deshabilitar JTAG y restringir ROM Download
espefuse.py -p /dev/ttyACM0 burn_efuse JTAG_DISABLE 1
espefuse.py -p /dev/ttyACM0 burn_efuse DIS_DIRECT_USB 0  # Ajustado al diseño final

# 6. Para firmar cada actualización futura antes de enviarla al dispositivo:
espsecure.py sign_data --version 2 --keyfile secure_boot_signing_key.pem --output firmware_firmado.bin firmware.bin
```

---

## 6. Resumen de Reglas para el Equipo

1. **En tu banco de trabajo / taller:** Trabajar siempre en **Fase 1** (cero quema de eFuses). La seguridad descansa en `VaultService` + PIN + AES-256-GCM.
2. **Para hardware que se instala en postes, torres o clientes (TableHub):** Aplicar el checklist de **Fase 2** antes de salir del laboratorio. El hardware queda cerrado, anti-robo, inmune a GhostESP y con sus credenciales de infraestructura imposibles de extraer.
