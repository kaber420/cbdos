# Plan de Arquitectura y Migración: Confinamiento Hardware de Storage a Device Tree (ESP32-P4)

> **Documento de Arquitectura y Especificación Técnica**  
> **Ubicación:** `specs/architecture/plan_arquitectura_storage_sdmmc_device_tree.md`  
> **Fecha:** 2026-09-24  
> **Fase / Paso:** Fase A (Nivel 1) — Paso 5 del Plan de Migración CDT  
> **Objetivo:** Confinar el conocimiento de hardware (pines, pull-ups y Device Tree) a la rutina de inicialización física (`mountSd()`), eliminando el conocimiento de hardware de las rutinas de alto nivel (`formatSd()`) y saneando el reporte de estado de FatFS.

---

## 1. Diagnóstico de Arquitectura

En [`bsp/esp32_p4_jc4880/hal/hal_storage_p4.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/hal_storage_p4.cpp), el diseño original presentaba una fuga de responsabilidad (*hardware leakage*):

### El Problema
* **`formatSd()` re-inicializaba el bus físico:** En lugar de operar como una función de sistema de archivos sobre una tarjeta conectada, desmontaba el dispositivo y volvía a ejecutar:
  - Encendido de LDO VO4 (`ensureLdoPower()`).
  - Activación de pull-ups internos en 5 GPIOs.
  - Creación de estructuras `sdmmc_host_t` y `sdmmc_slot_config_t`.
  - Asignación manual de los 6 pines (`clk`, `cmd`, `d0`..`d3`).
  - Llamada a `esp_vfs_fat_sdmmc_mount()` para recuperar el descriptor `s_cardHandle`.
* **Falso Éxito:** Si `esp_vfs_fat_sdcard_format()` fallaba, la función emitía un log de aviso pero retornaba `true` (líneas 271–274), engañando a la capa superior ([`StorageConfigView.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/StorageConfigView.cpp)).

---

## 2. Principio de Diseño: Separación de Responsabilidades

En un sistema embebido profesional:
1. **El hardware pertenece a la capa de enlace físico (`mountSd()`):**
   Solo la función responsable de inicializar el bus físico (`SDMMC Slot 0`) necesita consultar el Device Tree (`cbdos::board::sdcard`), energizar el LDO y configurar pull-ups.
2. **Las operaciones de archivos no tocan pines:**
   `formatSd()`, `readFile()`, `writeFile()`, `deleteFile()` operan sobre el sistema de archivos ya inicializado y montado en `SD_MOUNT_POINT` (`/sdcard`) utilizando el descriptor de la tarjeta `s_cardHandle`.

---

## 3. Especificación Técnica de la Solución

### 3.1. Enlace Físico (`mountSd()`) — Único consumidor de CDT
La función `mountSd()` es el único punto donde se configura el hardware:

```cpp
bool mountSd() override {
    if (s_sdMounted) {
        ESP_LOGI(TAG, "MicroSD ya se encuentra montada en %s", SD_MOUNT_POINT);
        return true;
    }

    ensureLdoPower();

    // Pull-ups en líneas de comando y datos
    gpio_pullup_en(static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_CMD));
    gpio_pullup_en(static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_D0));
    gpio_pullup_en(static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_D1));
    gpio_pullup_en(static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_D2));
    gpio_pullup_en(static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_D3));

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_0;
    host.max_freq_khz = 10000;

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 4;
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
    slot_config.clk = static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_CLK);
    slot_config.cmd = static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_CMD);
    slot_config.d0  = static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_D0);
    slot_config.d1  = static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_D1);
    slot_config.d2  = static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_D2);
    slot_config.d3  = static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_D3);

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 8,
        .allocation_unit_size = 16 * 1024,
        .disk_status_check_enable = false
    };

    ESP_LOGI(TAG, "Intentando montar MicroSD (Slot 0, 4-bit) en %s...", SD_MOUNT_POINT);
    esp_err_t ret = esp_vfs_fat_sdmmc_mount(SD_MOUNT_POINT, &host, &slot_config, &mount_config, &s_cardHandle);

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Fallo montaje 4-bit (0x%x), reintentando en modo 1-bit...", ret);
        slot_config.width = 1;
        ret = esp_vfs_fat_sdmmc_mount(SD_MOUNT_POINT, &host, &slot_config, &mount_config, &s_cardHandle);
    }

    if (ret == ESP_OK && s_cardHandle != nullptr) {
        s_sdMounted = true;
        ESP_LOGI(TAG, "MicroSD montada exitosamente en %s!", SD_MOUNT_POINT);
        sdmmc_card_print_info(stdout, s_cardHandle);
        return true;
    } else {
        s_sdMounted = false;
        s_cardHandle = nullptr;
        ESP_LOGW(TAG, "MicroSD no detectada o no montada: 0x%x (%s)", ret, esp_err_to_name(ret));
        return false;
    }
}
```

### 3.2. Operación de Formateo (`formatSd()`) — Cero conocimiento de pines
La función `formatSd()` se reduce a su verdadera responsabilidad:

```cpp
bool formatSd() override {
    ESP_LOGI(TAG, "Iniciando formateo de MicroSD a FAT32...");

    // Si la tarjeta no está montada, intentar montarla primero para validar conexión y handle
    if (!s_sdMounted) {
        if (!mountSd()) {
            ESP_LOGE(TAG, "No se puede formatear: la tarjeta MicroSD no esta conectada o no responde.");
            return false;
        }
    }

    if (!s_cardHandle) {
        ESP_LOGE(TAG, "Handle de tarjeta nulo, cancelando formateo.");
        return false;
    }

    ESP_LOGI(TAG, "Ejecutando formateo nativo FatFS a bajo nivel...");
    esp_err_t fmt_ret = esp_vfs_fat_sdcard_format(SD_MOUNT_POINT, s_cardHandle);
    if (fmt_ret == ESP_OK) {
        ESP_LOGI(TAG, "MicroSD formateada exitosamente a FAT32!");
        return true;
    } else {
        ESP_LOGE(TAG, "Fallo al formatear MicroSD: %s", esp_err_to_name(fmt_ret));
        return false;
    }
}
```

---

## 4. Beneficios Técnicos Inmediatos

| Aspecto | Antes | Con este Plan |
|:---|:---|:---|
| **Pines en el código** | 22 literales repartidos en 2 métodos | 11 constantes del CDT en un solo método (`mountSd`) |
| **Duplicación de código** | 60 líneas repetidas | 0 líneas duplicadas |
| **Responsabilidad** | `formatSd()` reconfiguraba buses y GPIOs | `formatSd()` solo gestiona formato FatFS |
| **Diagnóstico de errores** | `formatSd()` retornaba `true` al fallar | `formatSd()` reporta `false` ante errores reales |
| **LDO VO4 (3.3V)** | Encendido redundante | Encendido controlado por el ciclo de montaje |

---

## 5. Criterios de Aceptación y Validación

1. **Cero Literales Hardcodeados:**
   Ningún `GPIO_NUM_39`..`44` en `hal_storage_p4.cpp`.
2. **Cero Mención de Pines en `formatSd()`:**
   La función `formatSd()` no contiene referencias a pines, slots ni pull-ups.
3. **Compilación Limpia:**
   ```bash
   . /home/kaber420/esp/esp-idf/export.sh && idf.py -C bsp/esp32_p4_jc4880 build
   ```
   *Criterio:* `Project build complete.` código de salida 0.
4. **Actualización del Tracker:**
   Actualizar [`specs/architecture/PLAN_MIGRACION_CDT_Y_DRIVERS.md`](file:///home/kaber420/Documentos/proyectos/cbdos/specs/architecture/PLAN_MIGRACION_CDT_Y_DRIVERS.md) registrando el Paso 5 como `✅`.

---

## 6. Hoja de Ruta Posterior: Rediseño Arquitectónico del Core

Este plan resuelve la migración de hardware de la placa (Fase A). Para el desacoplamiento definitivo del sistema de archivos y la eliminación del formateo dentro de los BSPs (P4 y S3), consultar la especificación:

👉 **[`specs/architecture/plan_rediseño_arquitectura_storage_core.md`](file:///home/kaber420/Documentos/proyectos/cbdos/specs/architecture/plan_rediseño_arquitectura_storage_core.md)**

