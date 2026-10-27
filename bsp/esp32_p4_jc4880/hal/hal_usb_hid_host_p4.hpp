#pragma once

#include <cstdint>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace cbdos {
namespace bsp {

struct KeyboardEvent {
    uint32_t key;
    bool pressed;
};

/**
 * @brief Inicializa el driver HID Host y lo asocia al subsistema USB Host existente.
 * No reinstala usb_host_install, se conecta a la librería central ya activa.
 */
bool initUsbHidHostP4();

/**
 * @brief Desinicializa el driver HID Host y libera recursos.
 */
void deinitUsbHidHostP4();

/**
 * @brief Retorna la cola FreeRTOS que recibe los eventos de pulsación y liberación.
 */
QueueHandle_t getUsbKeyboardQueue();

/**
 * @brief Consulta rápida si hay un teclado USB físico actualmente reconocido y activo.
 */
bool isUsbKeyboardConnected();

} // namespace bsp
} // namespace cbdos
