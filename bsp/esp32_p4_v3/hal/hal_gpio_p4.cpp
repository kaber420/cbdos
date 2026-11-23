#include "cbdos/gpio.hpp"
#include "cbdos_device_tree.h"
#include <driver/gpio.h>
#include <esp_log.h>

static const char* TAG_GPIO = "HAL_GPIO_P4";

namespace cbdos {
namespace bsp {

class P4GpioBackend : public cbdos::gpio::IGpioBackend {
public:
    bool setPinMode(int pin, cbdos::gpio::PinMode mode) override {
        if (!isPinAvailable(pin)) {
            ESP_LOGW(TAG_GPIO, "GPIO %d protegido o reservado por el sistema", pin);
            return false;
        }

        gpio_config_t io_conf = {};
        io_conf.pin_bit_mask = (1ULL << pin);
        io_conf.intr_type = GPIO_INTR_DISABLE;

        switch (mode) {
            case cbdos::gpio::PinMode::Input:
                io_conf.mode = GPIO_MODE_INPUT;
                io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
                io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
                break;
            case cbdos::gpio::PinMode::Output:
                io_conf.mode = GPIO_MODE_OUTPUT;
                io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
                io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
                break;
            case cbdos::gpio::PinMode::InputPullUp:
                io_conf.mode = GPIO_MODE_INPUT;
                io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
                io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
                break;
            case cbdos::gpio::PinMode::InputPullDown:
                io_conf.mode = GPIO_MODE_INPUT;
                io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
                io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
                break;
        }

        return gpio_config(&io_conf) == ESP_OK;
    }

    bool digitalWrite(int pin, cbdos::gpio::PinLevel level) override {
        if (!isPinAvailable(pin)) return false;
        return gpio_set_level((gpio_num_t)pin, (uint32_t)level) == ESP_OK;
    }

    cbdos::gpio::PinLevel digitalRead(int pin) override {
        if (!isPinAvailable(pin)) return cbdos::gpio::PinLevel::Low;
        int val = gpio_get_level((gpio_num_t)pin);
        return (val > 0) ? cbdos::gpio::PinLevel::High : cbdos::gpio::PinLevel::Low;
    }

    bool isPinAvailable(int pin) const override {
        if (pin < 0 || pin > 54) return false;
        
        // Verificar si está en la lista de pines de expansión (Whitelist JP1)
        for (size_t i = 0; i < cbdos::board::NUM_EXPANSION_PINS; ++i) {
            if (pin == cbdos::board::EXPANSION_PINS[i]) {
                return true;
            }
        }
        return false; // Si no está en la lista blanca, acceso denegado
    }
};

static P4GpioBackend s_p4GpioBackend;

void initGpioBackendP4() {
    cbdos::gpio::setBackend(&s_p4GpioBackend);
    ESP_LOGI(TAG_GPIO, "Backend GPIO para ESP32-P4 registrado e inyectado (Firewall Activo)");
}

} // namespace bsp
} // namespace cbdos
