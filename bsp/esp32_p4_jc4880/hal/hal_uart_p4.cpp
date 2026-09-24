#include "cbdos/uart.hpp"
#include "cbdos/serial.hpp"
#include "cbdos/gpio.hpp"
#include "usb_device_manager.hpp"
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
    P4UsbOtgPort() : m_isOpen(false), m_cdcDev(nullptr), m_rxRingBuf(nullptr) {}

    ~P4UsbOtgPort() override {
        m_isOpen = false;
        if (m_cdcDev) {
            cdc_acm_host_close(m_cdcDev);
            m_cdcDev = nullptr;
        }
        if (m_rxRingBuf) {
            vRingbufferDelete(m_rxRingBuf);
            m_rxRingBuf = nullptr;
        }
    }

    bool open(const cbdos::serial::SerialConfig& config) override {
        const auto* devInfo = ::cbdos::usb::UsbDeviceManager::getInstance().getActiveDevice();
        if (!devInfo || !devInfo->isConnected) {
            ESP_LOGW(TAG_SERIAL, "No hay dispositivo USB conectado en puerto OTG");
            return false;
        }

        if (!m_rxRingBuf) {
            m_rxRingBuf = xRingbufferCreate(2048, RINGBUF_TYPE_BYTEBUF);
        }

        // Si el handle CDC ya existe y sigue activo por hardware, restauramos la sesión instantáneamente
        if (m_cdcDev) {
            setBaudrate(config.baudrate);
            cdc_acm_host_set_control_line_state(m_cdcDev, true, false);
            m_isOpen = true;
            ESP_LOGI(TAG_SERIAL, "Sesión USB OTG restaurada instantáneamente (DTR=1, RTS=0)");
            return true;
        }

        const cdc_acm_host_device_config_t dev_config = {
            .connection_timeout_ms = 1000,
            .out_buffer_size = 2048,
            .in_buffer_size = 2048,
            .event_cb = cdcEventCb,
            .data_cb = cdcRxCb,
            .user_arg = this,
        };

        esp_err_t err = cdc_acm_host_open(devInfo->vid, devInfo->pid, 0, &dev_config, &m_cdcDev);
        if (err != ESP_OK) {
            err = cdc_acm_host_open_vendor_specific(devInfo->vid, devInfo->pid, 0, &dev_config, &m_cdcDev);
        }

        if (err != ESP_OK || !m_cdcDev) {
            ESP_LOGE(TAG_SERIAL, "Fallo al abrir CDC-ACM Host en VID:0x%04X PID:0x%04X (err=%s)",
                     devInfo->vid, devInfo->pid, esp_err_to_name(err));
            return false;
        }

        setBaudrate(config.baudrate);
        cdc_acm_host_set_control_line_state(m_cdcDev, true, false);
        m_isOpen = true;
        ESP_LOGI(TAG_SERIAL, "Puerto USB OTG abierto con VID:0x%04X PID:0x%04X (DTR=1, RTS=0)", devInfo->vid, devInfo->pid);
        return true;
    }

    void close() override {
        if (!m_isOpen) return;
        // Retener m_cdcDev vivo en el hardware; solo desactivamos bandera y bajamos DTR
        if (m_cdcDev) {
            cdc_acm_host_set_control_line_state(m_cdcDev, false, false);
        }
        m_isOpen = false;
        ESP_LOGI(TAG_SERIAL, "Puerto USB OTG en reposo (sesión pausada, hardware retenido).");
    }

    bool isOpen() const override {
        return m_isOpen;
    }

    size_t available() override {
        if (!m_isOpen || !m_rxRingBuf) return 0;
        UBaseType_t items = 0;
        vRingbufferGetInfo(m_rxRingBuf, nullptr, nullptr, nullptr, nullptr, &items);
        return (size_t)items;
    }

    size_t read(uint8_t* buffer, size_t maxLen) override {
        if (!m_isOpen || !m_rxRingBuf || !buffer || maxLen == 0) return 0;
        size_t item_size = 0;
        uint8_t* item = (uint8_t*)xRingbufferReceiveUpTo(m_rxRingBuf, &item_size, 0, maxLen);
        if (item && item_size > 0) {
            memcpy(buffer, item, item_size);
            vRingbufferReturnItem(m_rxRingBuf, (void*)item);
            return item_size;
        }
        return 0;
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
        if (!m_isOpen || !m_cdcDev || !data || len == 0) return 0;
        esp_err_t err = cdc_acm_host_data_tx_blocking(m_cdcDev, data, len, pdMS_TO_TICKS(100));
        return (err == ESP_OK) ? len : 0;
    }

    size_t writeString(const std::string& str) override {
        return write((const uint8_t*)str.data(), str.size());
    }

    void flush() override {}

    bool setBaudrate(uint32_t baudrate) override {
        if (!m_cdcDev) return false;
        cdc_acm_line_coding_t line_coding = {
            .dwDTERate = baudrate,
            .bCharFormat = 0,
            .bParityType = 0,
            .bDataBits = 8,
        };
        return cdc_acm_host_line_coding_set(m_cdcDev, &line_coding) == ESP_OK;
    }

    bool setControlPin(bool level) override {
        if (!m_cdcDev) return false;
        return cdc_acm_host_set_control_line_state(m_cdcDev, level, false) == ESP_OK;
    }

    bool pulseControlPin(uint32_t durationMs, bool enterBootloader = false) override {
        if (!m_cdcDev) return false;
        if (enterBootloader) {
            // Modo DFU: DTR=false (GPIO0=0), RTS=true (Reset activo)
            cdc_acm_host_set_control_line_state(m_cdcDev, false, true);
            vTaskDelay(pdMS_TO_TICKS(durationMs));
            // Reset liberado manteniendo GPIO0 en LOW momentáneamente
            cdc_acm_host_set_control_line_state(m_cdcDev, false, false);
            vTaskDelay(pdMS_TO_TICKS(20));
            // Restaurar DTR=true para abrir canal de comunicación con el bootloader ROM
            cdc_acm_host_set_control_line_state(m_cdcDev, true, false);
            ESP_LOGI(TAG_SERIAL, "Dispositivo USB OTG puesto en Modo Bootloader / DFU");
        } else {
            // Modo Normal: DTR=true (GPIO0=1), RTS=true (Reset activo)
            cdc_acm_host_set_control_line_state(m_cdcDev, true, true);
            vTaskDelay(pdMS_TO_TICKS(durationMs));
            // Reset liberado con GPIO0 en HIGH -> Ejecuta firmware de usuario
            cdc_acm_host_set_control_line_state(m_cdcDev, true, false);
            ESP_LOGI(TAG_SERIAL, "Reinicio normal enviado por USB OTG (Run Mode)");
        }
        return true;
    }

    void onDisconnected() {
        m_isOpen = false;
        if (m_cdcDev) {
            cdc_acm_host_close(m_cdcDev);
            m_cdcDev = nullptr;
        }
        ESP_LOGI(TAG_SERIAL, "Dispositivo USB OTG liberado por desconexión física.");
    }

private:
    static bool cdcRxCb(const uint8_t *data, size_t data_len, void *user_arg) {
        auto* self = static_cast<P4UsbOtgPort*>(user_arg);
        if (self && self->m_rxRingBuf && data && data_len > 0) {
            xRingbufferSend(self->m_rxRingBuf, data, data_len, 0);
        }
        return true;
    }

    static void cdcEventCb(const cdc_acm_host_dev_event_data_t *event, void *user_arg) {
        auto* self = static_cast<P4UsbOtgPort*>(user_arg);
        if (!self || !event) return;
        if (event->type == CDC_ACM_HOST_DEVICE_DISCONNECTED) {
            ESP_LOGW(TAG_SERIAL, "Dispositivo CDC-ACM Host desconectado físicamente");
            self->onDisconnected();
        }
    }

    bool m_isOpen;
    cdc_acm_dev_hdl_t m_cdcDev;
    RingbufHandle_t m_rxRingBuf;
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
        ::cbdos::usb::UsbDeviceManager::getInstance().registerEventCallback(onUsbManagerEvent, this);
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
        const auto* dev = ::cbdos::usb::UsbDeviceManager::getInstance().getActiveDevice();
        if (dev && dev->isConnected) {
            std::string name = "🔌 USB: " + std::string(dev->product);
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
    static void onUsbManagerEvent(const ::cbdos::usb::UsbDeviceInfo& dev, bool connected, void* user_ctx) {
        (void)dev;
        auto* self = static_cast<P4SerialBackend*>(user_ctx);
        if (!self) return;
        if (!connected) {
            self->m_portUsbOtg.onDisconnected();
        }
        if (self->m_hotplugCb) {
            self->m_hotplugCb(connected, "usb_otg");
        }
    }

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

// ────────────────────────────────────────────────────────────────
// Implementación IGpioBackend para ESP32-P4
// ────────────────────────────────────────────────────────────────

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

static P4UartBackend s_p4UartBackend;
static P4SerialBackend s_p4SerialBackend;
static P4GpioBackend s_p4GpioBackend;

void initUartBackendP4() {
    cbdos::uart::setBackend(&s_p4UartBackend);
    cbdos::serial::setBackend(&s_p4SerialBackend);
    ESP_LOGI(TAG_UART, "Backend UART y Serial para ESP32-P4 registrados e inyectados");
}

void initGpioBackendP4() {
    cbdos::gpio::setBackend(&s_p4GpioBackend);
    ESP_LOGI(TAG_GPIO, "Backend GPIO para ESP32-P4 registrado e inyectado");
}

} // namespace bsp
} // namespace cbdos
