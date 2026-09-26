#include "cbdos/uart.hpp"
#include "cbdos/serial.hpp"
#include "cbdos/gpio.hpp"
#include "hal_usb_cdc_p4.hpp"
#include <driver/uart.h>
#include <driver/gpio.h>
#include <driver/usb_serial_jtag.h>
#include <usb/usb_host.h>
#include <usb/cdc_acm_host.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/ringbuf.h>
#include <esp_log.h>
#include <cstring>
#include <vector>
#include "cbdos_device_tree.h"

static const char* TAG_UART = "HAL_UART_P4";
static const char* TAG_SERIAL = "HAL_SERIAL_P4";
static const char* TAG_GPIO = "HAL_GPIO_P4";

#define UART_HAL_PORT UART_NUM_1
#define UART_RX_BUF_SIZE 2048
#define UART_TX_BUF_SIZE 1024

namespace cbdos {
namespace bsp {

// ────────────────────────────────────────────────────────────────
// Implementación ISerialPort para USB 1 Nativo (Consola PC / JTAG)
// ────────────────────────────────────────────────────────────────

class P4UsbNativePort : public cbdos::serial::ISerialPort {
public:
    P4UsbNativePort() : m_isOpen(false) {}

    bool open(const cbdos::serial::SerialConfig& config) override {
        (void)config;
        if (!usb_serial_jtag_is_driver_installed()) {
            usb_serial_jtag_driver_config_t usb_s_cfg = {
                .tx_buffer_size = 1024,
                .rx_buffer_size = 2048,
            };
            esp_err_t err = usb_serial_jtag_driver_install(&usb_s_cfg);
            if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
                ESP_LOGE(TAG_SERIAL, "Fallo al instalar driver usb_serial_jtag: %s", esp_err_to_name(err));
                return false;
            }
        }
        m_isOpen = true;
        ESP_LOGI(TAG_SERIAL, "Puerto USB Nativo (Consola PC) abierto.");
        return true;
    }

    void close() override {
        m_isOpen = false;
        ESP_LOGI(TAG_SERIAL, "Puerto USB Nativo cerrado.");
    }

    bool isOpen() const override {
        return m_isOpen;
    }

    size_t available() override {
        return m_isOpen ? 1 : 0;
    }

    size_t read(uint8_t* buffer, size_t maxLen) override {
        if (!m_isOpen || !buffer || maxLen == 0) return 0;
        int len = usb_serial_jtag_read_bytes(buffer, maxLen, 0);
        return (len > 0) ? (size_t)len : 0;
    }

    std::string readString(size_t maxLen) override {
        std::string res;
        if (!m_isOpen || maxLen == 0) return res;
        uint8_t buf[256];
        size_t toRead = (maxLen < sizeof(buf)) ? maxLen : sizeof(buf);
        size_t r = read(buf, toRead);
        if (r > 0) res.assign((char*)buf, r);
        return res;
    }

    size_t write(const uint8_t* data, size_t len) override {
        if (!m_isOpen || !data || len == 0) return 0;
        int written = usb_serial_jtag_write_bytes(data, len, pdMS_TO_TICKS(50));
        return (written > 0) ? (size_t)written : 0;
    }

    size_t writeString(const std::string& str) override {
        return write((const uint8_t*)str.data(), str.size());
    }

    void flush() override {}

    bool setBaudrate(uint32_t) override {
        return true;
    }

    bool setControlPin(bool) override {
        return false;
    }

    bool pulseControlPin(uint32_t, bool = false) override {
        return false;
    }

private:
    bool m_isOpen;
};

// ────────────────────────────────────────────────────────────────
// Implementación ISerialPort para USB 2 OTG Host (CDC-ACM)
// ────────────────────────────────────────────────────────────────

class P4UsbOtgPort : public cbdos::serial::ISerialPort {
public:
    P4UsbOtgPort() = default;
    ~P4UsbOtgPort() override { close(); }

    bool open(const cbdos::serial::SerialConfig& config) override {
        auto* cdc = ::cbdos::bsp::getP4UsbCdcChannel();
        return cdc ? cdc->open(config) : false;
    }

    void close() override {
        auto* cdc = ::cbdos::bsp::getP4UsbCdcChannel();
        if (cdc) cdc->close();
    }

    bool isOpen() const override {
        auto* cdc = ::cbdos::bsp::getP4UsbCdcChannel();
        return cdc ? cdc->isOpen() : false;
    }

    size_t available() override {
        auto* cdc = ::cbdos::bsp::getP4UsbCdcChannel();
        return cdc ? cdc->available() : 0;
    }

    size_t read(uint8_t* buffer, size_t maxLen) override {
        auto* cdc = ::cbdos::bsp::getP4UsbCdcChannel();
        return cdc ? cdc->read(buffer, maxLen) : 0;
    }

    std::string readString(size_t maxLen) override {
        auto* cdc = ::cbdos::bsp::getP4UsbCdcChannel();
        return cdc ? cdc->readString(maxLen) : "";
    }

    size_t write(const uint8_t* data, size_t len) override {
        auto* cdc = ::cbdos::bsp::getP4UsbCdcChannel();
        return cdc ? cdc->write(data, len) : 0;
    }

    size_t writeString(const std::string& str) override {
        auto* cdc = ::cbdos::bsp::getP4UsbCdcChannel();
        return cdc ? cdc->writeString(str) : 0;
    }

    void flush() override {
        auto* cdc = ::cbdos::bsp::getP4UsbCdcChannel();
        if (cdc) cdc->flush();
    }

    bool setBaudrate(uint32_t baudrate) override {
        auto* cdc = ::cbdos::bsp::getP4UsbCdcChannel();
        return cdc ? cdc->setBaudrate(baudrate) : false;
    }

    bool setControlPin(bool level) override {
        auto* cdc = ::cbdos::bsp::getP4UsbCdcChannel();
        return cdc ? cdc->setControlPin(level) : false;
    }

    bool pulseControlPin(uint32_t durationMs, bool enterBootloader = false) override {
        auto* cdc = ::cbdos::bsp::getP4UsbCdcChannel();
        return cdc ? cdc->pulseControlPin(durationMs, enterBootloader) : false;
    }

    void onDisconnected() {
        close();
    }
};

// ────────────────────────────────────────────────────────────────
// Implementación ISerialPort para UART (JP1 y Manual) en ESP32-P4
// ────────────────────────────────────────────────────────────────

class P4SerialPort : public cbdos::serial::ISerialPort {
public:
    P4SerialPort(const std::string& portId, int defaultTx, int defaultRx, int ctrlPin)
        : m_portId(portId), m_defaultTx(defaultTx), m_defaultRx(defaultRx), m_controlPin(ctrlPin) {}

    bool open(const cbdos::serial::SerialConfig& config) override {
        if (m_isOpen) {
            close();
        }

        m_txPin = (config.txPin >= 0) ? config.txPin : m_defaultTx;
        m_rxPin = (config.rxPin >= 0) ? config.rxPin : m_defaultRx;
        m_baudrate = config.baudrate;

        if (m_txPin < 0 || m_rxPin < 0) {
            ESP_LOGE(TAG_SERIAL, "Pines invalidos para puerto %s: TX:%d RX:%d", m_portId.c_str(), m_txPin, m_rxPin);
            return false;
        }

        // 1. Instalar el driver de UART primero (asigna buffers e interrupciones en ESP-IDF)
        esp_err_t err = uart_driver_install(UART_HAL_PORT, UART_RX_BUF_SIZE, UART_TX_BUF_SIZE, 0, NULL, 0);
        if (err != ESP_OK) {
            ESP_LOGE(TAG_SERIAL, "uart_driver_install fallo: %s", esp_err_to_name(err));
            return false;
        }

        // 2. Configurar los parámetros de comunicación serie
        uart_config_t uart_config = {};
        uart_config.baud_rate = (int)m_baudrate;
        uart_config.data_bits = UART_DATA_8_BITS;
        uart_config.parity    = UART_PARITY_DISABLE;
        uart_config.stop_bits = UART_STOP_BITS_1;
        uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
        uart_config.rx_flow_ctrl_thresh = 122;
        uart_config.source_clk = UART_SCLK_DEFAULT;

        err = uart_param_config(UART_HAL_PORT, &uart_config);
        if (err != ESP_OK) {
            ESP_LOGE(TAG_SERIAL, "uart_param_config fallo: %s", esp_err_to_name(err));
            uart_driver_delete(UART_HAL_PORT);
            return false;
        }

        // 3. Asignar los pines físicos de TX y RX
        err = uart_set_pin(UART_HAL_PORT, m_txPin, m_rxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
        if (err != ESP_OK) {
            ESP_LOGE(TAG_SERIAL, "uart_set_pin fallo: %s", esp_err_to_name(err));
            uart_driver_delete(UART_HAL_PORT);
            return false;
        }

        // 4. Activar pull-up interno en RX para evitar líneas flotantes
        gpio_pullup_en((gpio_num_t)m_rxPin);

        // 5. Configurar pin de control o reset auxiliar si está definido
        if (config.controlPin >= 0) {
            m_controlPin = config.controlPin;
        }
        if (m_controlPin >= 0) {
            gpio_config_t io_conf = {};
            io_conf.pin_bit_mask = (1ULL << m_controlPin);
            io_conf.mode = GPIO_MODE_OUTPUT;
            io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
            io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
            io_conf.intr_type = GPIO_INTR_DISABLE;
            gpio_config(&io_conf);
            gpio_set_level((gpio_num_t)m_controlPin, 1);
        }

        m_isOpen = true;
        ESP_LOGI(TAG_SERIAL, "Puerto %s abierto en TX:%d RX:%d (Ctrl:%d) @ %u bps",
                 m_portId.c_str(), m_txPin, m_rxPin, m_controlPin, (unsigned)m_baudrate);
        return true;
    }

    void close() override {
        if (!m_isOpen) return;
        uart_driver_delete(UART_HAL_PORT);
        m_isOpen = false;
        ESP_LOGI(TAG_SERIAL, "Puerto %s cerrado y pines liberados", m_portId.c_str());
    }

    bool isOpen() const override {
        return m_isOpen;
    }

    size_t available() override {
        if (!m_isOpen) return 0;
        size_t len = 0;
        esp_err_t err = uart_get_buffered_data_len(UART_HAL_PORT, &len);
        return (err == ESP_OK) ? len : 0;
    }

    size_t read(uint8_t* buffer, size_t maxLen) override {
        if (!m_isOpen || !buffer || maxLen == 0) return 0;
        int len = uart_read_bytes(UART_HAL_PORT, buffer, maxLen, 0);
        return (len > 0) ? (size_t)len : 0;
    }

    std::string readString(size_t maxLen) override {
        std::string res;
        if (!m_isOpen || maxLen == 0) return res;
        size_t avail = available();
        if (avail == 0) return res;
        size_t toRead = (avail < maxLen) ? avail : maxLen;
        res.resize(toRead);
        size_t actual = read((uint8_t*)&res[0], toRead);
        res.resize(actual);
        return res;
    }

    size_t write(const uint8_t* data, size_t len) override {
        if (!m_isOpen || !data || len == 0) return 0;
        int written = uart_write_bytes(UART_HAL_PORT, (const char*)data, len);
        return (written > 0) ? (size_t)written : 0;
    }

    size_t writeString(const std::string& str) override {
        return write((const uint8_t*)str.data(), str.size());
    }

    void flush() override {
        if (!m_isOpen) return;
        uart_flush(UART_HAL_PORT);
    }

    bool setBaudrate(uint32_t baudrate) override {
        if (!m_isOpen) return false;
        m_baudrate = baudrate;
        return uart_set_baudrate(UART_HAL_PORT, baudrate) == ESP_OK;
    }

    bool setControlPin(bool level) override {
        if (m_controlPin < 0) return false;
        return gpio_set_level((gpio_num_t)m_controlPin, level ? 1 : 0) == ESP_OK;
    }

    bool pulseControlPin(uint32_t durationMs, bool enterBootloader = false) override {
        if (m_portId == "jp1") {
            // JP1: Reset físico en GPIO 54 (Target EN / C6_CHIP_PU), Boot en GPIO 34 (Target IO0 / C6_IO9)
            gpio_config_t io_conf = {};
            io_conf.pin_bit_mask = (1ULL << 54) | (1ULL << 34);
            io_conf.mode = GPIO_MODE_OUTPUT;
            io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
            io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
            io_conf.intr_type = GPIO_INTR_DISABLE;
            gpio_config(&io_conf);

            if (enterBootloader) {
                // Modo DFU: Bajar Boot (GPIO 34 = 0), pulsar Reset (GPIO 54 = 0 -> 1)
                gpio_set_level((gpio_num_t)34, 0);
                vTaskDelay(pdMS_TO_TICKS(10));
                gpio_set_level((gpio_num_t)54, 0);
                vTaskDelay(pdMS_TO_TICKS(durationMs));
                gpio_set_level((gpio_num_t)54, 1);
                vTaskDelay(pdMS_TO_TICKS(50));
                gpio_set_level((gpio_num_t)34, 1);
                ESP_LOGI(TAG_SERIAL, "JP1 puesto en Modo Bootloader / DFU (IO34=0, IO54 pulso)");
            } else {
                // Modo Normal: Mantener Boot en HIGH (GPIO 34 = 1), pulsar Reset (GPIO 54 = 0 -> 1)
                gpio_set_level((gpio_num_t)34, 1);
                vTaskDelay(pdMS_TO_TICKS(10));
                gpio_set_level((gpio_num_t)54, 0);
                vTaskDelay(pdMS_TO_TICKS(durationMs));
                gpio_set_level((gpio_num_t)54, 1);
                ESP_LOGI(TAG_SERIAL, "JP1 reinicio normal enviado (Run Mode - IO34=1, IO54 pulso)");
            }
            return true;
        }

        if (m_controlPin < 0) return false;
        gpio_set_level((gpio_num_t)m_controlPin, 0);
        vTaskDelay(pdMS_TO_TICKS(durationMs));
        gpio_set_level((gpio_num_t)m_controlPin, 1);
        ESP_LOGI(TAG_SERIAL, "Pulso de %u ms enviado a GPIO %d", (unsigned)durationMs, m_controlPin);
        return true;
    }

private:
    std::string m_portId;
    int m_defaultTx;
    int m_defaultRx;
    int m_controlPin;
    int m_txPin = -1;
    int m_rxPin = -1;
    uint32_t m_baudrate = 115200;
    bool m_isOpen = false;
};

// ────────────────────────────────────────────────────────────────
// Implementación ISerialBackend para ESP32-P4
// ────────────────────────────────────────────────────────────────

class P4SerialBackend : public cbdos::serial::ISerialBackend {
public:
    P4SerialBackend()
        : m_portJp1("jp1", 32, 28, 34),
          m_portUart0("uart0", 38, 37, -1),
          m_portManual("manual", 32, 28, 34) {
        if (auto* host = ::cbdos::usb::getUsbHostBackend()) {
            host->registerEventCallback([this](const ::cbdos::usb::UsbDeviceInfo& dev, bool connected, ::cbdos::usb::UsbEventCause cause) {
                (void)dev;
                (void)cause;
                if (!connected) {
                    m_portUsbOtg.onDisconnected();
                }
                if (m_hotplugCb) {
                    m_hotplugCb(connected, "usb_otg");
                }
            });
        }
    }

    std::vector<cbdos::serial::SerialPortDescriptor> getAvailablePorts() override {
        std::vector<cbdos::serial::SerialPortDescriptor> ports;

        // 1. Puerto GPIO configurable libremente desde UI
        ports.push_back({
            "manual",
            "📌 UART GPIO (Pines)",
            cbdos::serial::PortType::ManualUart,
            32,
            28,
            34,
            true
        });

        // 2. Preset JP1 (Mochila / C6)
        ports.push_back({
            "jp1",
            "📌 JP1 (TX:32 RX:28)",
            cbdos::serial::PortType::HardwareUart,
            32,
            28,
            34,
            true
        });

        // 3. Preset UART0 Conector MX
        ports.push_back({
            "uart0",
            "📌 UART0 MX (TX:38 RX:37)",
            cbdos::serial::PortType::HardwareUart,
            38,
            37,
            -1,
            true
        });

        // 4. Puerto USB OTG Host (Aparece dinámicamente al conectar dispositivo serie)
        if (auto* host = ::cbdos::usb::getUsbHostBackend()) {
            ::cbdos::usb::UsbDeviceInfo dev;
            if (host->getActiveDevice(dev) && dev.isConnected) {
                std::string name = "🔌 USB: " + std::string(dev.product);
                ports.push_back({
                    "usb_otg",
                    name,
                    cbdos::serial::PortType::UsbCdcAcm,
                    -1,
                    -1,
                    -1,
                    true
                });
            }
        }

        return ports;
    }

    cbdos::serial::ISerialPort* getPort(const std::string& portId) override {
        if (portId == "usb_otg") {
            return &m_portUsbOtg;
        } else if (portId == "usb_native") {
            return &m_portUsbNative;
        } else if (portId == "jp1") {
            return &m_portJp1;
        } else if (portId == "uart0") {
            return &m_portUart0;
        } else if (portId == "manual" || portId == "gpio") {
            return &m_portManual;
        }
        return &m_portManual;
    }

    void setHotplugCallback(std::function<void(bool connected, const std::string& portId)> cb) override {
        m_hotplugCb = cb;
    }

private:

    P4UsbNativePort m_portUsbNative;
    P4UsbOtgPort m_portUsbOtg;
    P4SerialPort m_portJp1;
    P4SerialPort m_portUart0;
    P4SerialPort m_portManual;
    std::function<void(bool, const std::string&)> m_hotplugCb;
};

// ────────────────────────────────────────────────────────────────
// Implementación Legacy IUartBackend para ESP32-P4
// ────────────────────────────────────────────────────────────────

class P4UartBackend : public cbdos::uart::IUartBackend {
public:
    P4UartBackend() {
        m_presets = {
            {"JP1 (TX:32 RX:28)", 32, 28},
            {"MX 1.25 UART0 (TX:38 RX:37)", 38, 37}
        };
    }

    bool init(int txPin, int rxPin, uint32_t baudrate) override {
        if (m_isInitialized) {
            deinit();
        }

        m_currentTxPin = txPin;
        m_currentRxPin = rxPin;
        m_currentBaud = baudrate;

        uart_config_t uart_config = {};
        uart_config.baud_rate = (int)baudrate;
        uart_config.data_bits = UART_DATA_8_BITS;
        uart_config.parity    = UART_PARITY_DISABLE;
        uart_config.stop_bits = UART_STOP_BITS_1;
        uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
        uart_config.rx_flow_ctrl_thresh = 122;
        uart_config.source_clk = UART_SCLK_DEFAULT;

        esp_err_t err = uart_param_config(UART_HAL_PORT, &uart_config);
        if (err != ESP_OK) {
            ESP_LOGE(TAG_UART, "uart_param_config fallo: %s", esp_err_to_name(err));
            return false;
        }

        err = uart_set_pin(UART_HAL_PORT, txPin, rxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
        if (err != ESP_OK) {
            ESP_LOGE(TAG_UART, "uart_set_pin fallo: %s", esp_err_to_name(err));
            return false;
        }

        gpio_pullup_en((gpio_num_t)rxPin);

        err = uart_driver_install(UART_HAL_PORT, UART_RX_BUF_SIZE, UART_TX_BUF_SIZE, 0, NULL, 0);
        if (err != ESP_OK) {
            ESP_LOGE(TAG_UART, "uart_driver_install fallo: %s", esp_err_to_name(err));
            return false;
        }

        m_isInitialized = true;
        ESP_LOGI(TAG_UART, "UART1 inicializada en TX:%d, RX:%d a %u bps", txPin, rxPin, (unsigned)baudrate);
        return true;
    }

    void deinit() override {
        if (!m_isInitialized) return;
        uart_driver_delete(UART_HAL_PORT);
        m_isInitialized = false;
        ESP_LOGI(TAG_UART, "UART1 liberada");
    }

    bool isInitialized() const override {
        return m_isInitialized;
    }

    size_t available() override {
        if (!m_isInitialized) return 0;
        size_t length = 0;
        esp_err_t err = uart_get_buffered_data_len(UART_HAL_PORT, &length);
        return (err == ESP_OK) ? length : 0;
    }

    size_t read(uint8_t* buffer, size_t maxLen) override {
        if (!m_isInitialized || !buffer || maxLen == 0) return 0;
        int len = uart_read_bytes(UART_HAL_PORT, buffer, maxLen, 0);
        return (len > 0) ? (size_t)len : 0;
    }

    std::string readString(size_t maxLen) override {
        std::string res;
        if (!m_isInitialized || maxLen == 0) return res;
        size_t avail = available();
        if (avail == 0) return res;

        size_t toRead = (avail < maxLen) ? avail : maxLen;
        res.resize(toRead);
        size_t actual = read((uint8_t*)&res[0], toRead);
        res.resize(actual);
        return res;
    }

    size_t write(const uint8_t* data, size_t len) override {
        if (!m_isInitialized || !data || len == 0) return 0;
        int written = uart_write_bytes(UART_HAL_PORT, (const char*)data, len);
        return (written > 0) ? (size_t)written : 0;
    }

    size_t writeString(const std::string& str) override {
        return write((const uint8_t*)str.data(), str.size());
    }

    void flush() override {
        if (!m_isInitialized) return;
        uart_flush(UART_HAL_PORT);
    }

    bool setBaudrate(uint32_t baudrate) override {
        if (!m_isInitialized) return false;
        m_currentBaud = baudrate;
        esp_err_t err = uart_set_baudrate(UART_HAL_PORT, baudrate);
        return (err == ESP_OK);
    }

    int getDefaultTxPin() const override { return 32; }
    int getDefaultRxPin() const override { return 28; }
    uint32_t getDefaultBaudrate() const override { return 115200; }
    const std::vector<cbdos::uart::UartPinPreset>& getPinPresets() const override { return m_presets; }

private:
    bool m_isInitialized = false;
    int m_currentTxPin = 32;
    int m_currentRxPin = 28;
    uint32_t m_currentBaud = 115200;
    std::vector<cbdos::uart::UartPinPreset> m_presets;
};

static P4UartBackend s_p4UartBackend;
static P4SerialBackend s_p4SerialBackend;

void initUartBackendP4() {
    cbdos::uart::setBackend(&s_p4UartBackend);
    cbdos::serial::setBackend(&s_p4SerialBackend);
    ESP_LOGI(TAG_UART, "Backend UART y Serial para ESP32-P4 registrados e inyectados");
}

} // namespace bsp
} // namespace cbdos
