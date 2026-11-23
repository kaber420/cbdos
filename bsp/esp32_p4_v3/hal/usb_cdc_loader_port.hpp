#pragma once

#include "esp_loader.h"
#include <esp_loader_io.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicializa el puerto USB Host CDC-ACM y toma control del loader_port para esp-serial-flasher.
 * 
 * @param timeout_ms Tiempo máximo de espera para detección del dispositivo en milisegundos.
 * @return esp_loader_error_t ESP_LOADER_SUCCESS si se conectó el dispositivo USB.
 */
esp_loader_error_t loader_port_usb_cdc_init(uint32_t timeout_ms);

/**
 * @brief Libera el puerto USB CDC y el cliente host.
 */
void loader_port_usb_cdc_deinit(void);

/**
 * @brief Reinicia el microcontrolador target conectado por USB en modo de ejecución normal.
 */
esp_loader_error_t loader_port_usb_cdc_reset_target(void);

#ifdef __cplusplus
}
#endif
