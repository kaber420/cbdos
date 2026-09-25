# Propuesta Técnica: Refactorización DRY y Corrección de Formateo en Storage (ESP32-P4)

> **Documento Complementario de Propuesta Técnica**  
> **Ubicación:** `specs/architecture/propuesta_refactor_storage_dry_y_formateo.md`  
> **Fecha:** 2026-09-24  
> **Relacionado con:** `specs/architecture/plan_migracion_storage_a_device_tree.md` (Paso 5 CDT)  
> **Objetivo:** Documentar formalmente la propuesta para eliminar la duplicación de hardware en `hal_storage_p4.cpp` y corregir el bug de reporte de estado en `formatSd()`.

---

## 1. Problemas Identificados en `hal_storage_p4.cpp`

### A. Duplicación de Hardware (Violación DRY)
En la implementación actual existen 11 sentencias de configuración de hardware repetidas en dos métodos independientes:
1. `mountSd()` (líneas 158–177)
2. `formatSd()` (líneas 228–246)

Ambas rutinas configuran manualmente los pull-ups internos de los 5 pines de datos/comando y asignan los 6 pines del bus SDMMC (`clk`, `cmd`, `d0`..`d3`) a la estructura `sdmmc_slot_config_t`. Mantener dos bloques idénticos genera riesgo de desincronización en futuras revisiones de la placa.

### B. Ocultamiento de Error en `formatSd()`
En las líneas 265–274:
```cpp
esp_err_t fmt_ret = esp_vfs_fat_sdcard_format(SD_MOUNT_POINT, s_cardHandle);
if (fmt_ret == ESP_OK) {
    s_sdMounted = true;
    ESP_LOGI(TAG, "MicroSD formateada exitosamente a FAT32!");
    return true;
} else {
    // ⚠️ Bug: Ante un fallo en el formateo a bajo nivel, se emite aviso pero se retorna true
    ESP_LOGW(TAG, "Aviso al formatear directamente: %s. Reintentando montaje limpio...", esp_err_to_name(fmt_ret));
    s_sdMounted = true;
    return true; 
}
```
Esto provoca que la vista de configuración (`StorageConfigView.cpp:70`) informe al usuario que la tarjeta fue formateada con éxito cuando en realidad la operación falló.

---

## 2. Solución Propuesta

### 2.1. Helper Interno `configureSdmmcSlot()`
Definir una función auxiliar estática de ámbito de archivo en `hal_storage_p4.cpp`:

```cpp
static void configureSdmmcSlot(sdmmc_slot_config_t& slot_config) {
    // 1. Pull-ups internos requeridos para SDMMC 4-bit
    gpio_pullup_en(static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_CMD));
    gpio_pullup_en(static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_D0));
    gpio_pullup_en(static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_D1));
    gpio_pullup_en(static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_D2));
    gpio_pullup_en(static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_D3));

    // 2. Mapeo de pines desde el Device Tree
    slot_config.width = 4;
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
    slot_config.clk = static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_CLK);
    slot_config.cmd = static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_CMD);
    slot_config.d0  = static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_D0);
    slot_config.d1  = static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_D1);
    slot_config.d2  = static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_D2);
    slot_config.d3  = static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_D3);
}
```

### 2.2. Uso en `mountSd()` y `formatSd()`
Tanto `mountSd()` como `formatSd()` reemplazan sus 11 líneas duplicadas por una sola invocación:
```cpp
sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
configureSdmmcSlot(slot_config);
```

### 2.3. Corrección del Retorno de `formatSd()`
Hacer que el retorno sea verídico y coherente con el resultado de la operación:
```cpp
if (fmt_ret == ESP_OK) {
    s_sdMounted = true;
    ESP_LOGI(TAG, "MicroSD formateada exitosamente a FAT32!");
    return true;
} else {
    ESP_LOGE(TAG, "Fallo al formatear MicroSD: %s", esp_err_to_name(fmt_ret));
    s_sdMounted = false;
    return false;
}
```

---

## 3. Estado de la Propuesta
- [ ] Pendiente de evaluación y autorización por parte del usuario.
- El plan base ([`plan_migracion_storage_a_device_tree.md`](file:///home/kaber420/Documentos/proyectos/cbdos/specs/architecture/plan_migracion_storage_a_device_tree.md)) permanece intacto con la migración directa 1 a 1 de pines.

---

## 4. Hoja de Ruta Posterior: Rediseño Arquitectónico del Core

El desacoplamiento estructural definitivo del sistema de archivos respecto a los BSPs (P4 y S3) se encuentra especificado en:

👉 **[`specs/architecture/plan_rediseño_arquitectura_storage_core.md`](file:///home/kaber420/Documentos/proyectos/cbdos/specs/architecture/plan_rediseño_arquitectura_storage_core.md)**

