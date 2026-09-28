#include "cbdos/uart.hpp"
#include "cbdos/serial.hpp"
#include "cbdos/gpio.hpp"
#include "cbdos_device_tree.h"
#include <Arduino.h>
#include <HardwareSerial.h>
#include <vector>

namespace cbdos {
namespace bsp {

// ────────────────────────────────────────────────────────────────
// Implementación ISerialPort para ESP32-S3
// ────────────────────────────────────────────────────────────────

class S3SerialPort : public cbdos::serial::ISerialPort {
public:
    S3SerialPort(const std::string& portId, int defaultTx, int defaultRx)
        : m_portId(portId), m_defaultTx(defaultTx), m_defaultRx(defaultRx), m_uart(1) {}

    bool open(const cbdos::serial::SerialConfig& config) override {
        if (m_isOpen) {
            close();
        }

        m_txPin = (config.txPin >= 0) ? config.txPin : m_defaultTx;
        m_rxPin = (config.rxPin >= 0) ? config.rxPin : m_defaultRx;
        m_baudrate = config.baudrate;

        if (m_txPin < 0 || m_rxPin < 0) {
            return false;
        }

        pinMode(m_rxPin, INPUT_PULLUP);
        m_uart.begin(m_baudrate, SERIAL_8N1, m_rxPin, m_txPin);
        m_isOpen = true;
        return true;
    }

    void close() override {
        if (!m_isOpen) return;
        m_uart.end();
        m_isOpen = false;
    }

    bool isOpen() const override {
        return m_isOpen;
    }

    size_t available() override {
        if (!m_isOpen) return 0;
        int avail = m_uart.available();
        return (avail > 0) ? (size_t)avail : 0;
    }

    size_t read(uint8_t* buffer, size_t maxLen) override {
        if (!m_isOpen || !buffer || maxLen == 0) return 0;
        return m_uart.readBytes((char*)buffer, maxLen);
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
        return m_uart.write(data, len);
    }

    size_t writeString(const std::string& str) override {
        if (!m_isOpen || str.empty()) return 0;
        return m_uart.print(str.c_str());
    }

    void flush() override {
        if (!m_isOpen) return;
        m_uart.flush();
    }

    bool setBaudrate(uint32_t baudrate) override {
        if (!m_isOpen) return false;
        m_baudrate = baudrate;
        cbdos::serial::SerialConfig cfg;
        cfg.portId = m_portId;
        cfg.baudrate = baudrate;
        cfg.txPin = m_txPin;
        cfg.rxPin = m_rxPin;
        return open(cfg);
    }

    bool setControlPin(bool) override {
        return false;
    }

    bool pulseControlPin(uint32_t, bool = false) override {
        return false;
    }

private:
    std::string m_portId;
    int m_defaultTx;
    int m_defaultRx;
    int m_txPin = -1;
    int m_rxPin = -1;
    uint32_t m_baudrate = 115200;
    bool m_isOpen = false;
    HardwareSerial m_uart;
};

// ────────────────────────────────────────────────────────────────
// Implementación ISerialPort para USB Consola en ESP32-S3
// ────────────────────────────────────────────────────────────────

class S3UsbSerialPort : public cbdos::serial::ISerialPort {
public:
    bool open(const cbdos::serial::SerialConfig& config) override {
        (void)config;
        m_isOpen = true;
        return true;
    }

    void close() override {
        m_isOpen = false;
    }

    bool isOpen() const override {
        return m_isOpen;
    }

    size_t available() override {
        return m_isOpen ? (size_t)Serial.available() : 0;
    }

    size_t read(uint8_t* buffer, size_t maxLen) override {
        if (!m_isOpen || !buffer || maxLen == 0) return 0;
        return Serial.readBytes((char*)buffer, maxLen);
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
        return Serial.write(data, len);
    }

    size_t writeString(const std::string& str) override {
        if (!m_isOpen || str.empty()) return 0;
        return Serial.print(str.c_str());
    }

    void flush() override {
        if (m_isOpen) Serial.flush();
    }

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
    bool m_isOpen = false;
};

// ────────────────────────────────────────────────────────────────
// Implementación ISerialBackend para ESP32-S3
// ────────────────────────────────────────────────────────────────

class S3SerialBackend : public cbdos::serial::ISerialBackend {
public:
    S3SerialBackend()
        : m_portExt("ext_s3", 15, 16),
          m_portManual("manual", 15, 16) {}

    std::vector<cbdos::serial::SerialPortDescriptor> getAvailablePorts() override {
        std::vector<cbdos::serial::SerialPortDescriptor> ports;

        // 1. Puerto USB Consola
        ports.push_back({
            "usb0",
            "🔌 USB Serial (Consola)",
            cbdos::serial::PortType::UsbCdcAcm,
            -1,
            -1,
            -1,
            true
        });

        // 2. Puerto físico probado en el hardware S3 JC3248
        ports.push_back({
            "ext_s3",
            "📌 UART Ext (TX:15 RX:16)",
            cbdos::serial::PortType::HardwareUart,
            15,
            16,
            -1, // Sin pin de control en S3
            true
        });

        // 3. Puerto Manual
        ports.push_back({
            "manual",
            "⚙ Manual (TX / RX)",
            cbdos::serial::PortType::ManualUart,
            -1,
            -1,
            -1,
            true
        });

        return ports;
    }

    cbdos::serial::ISerialPort* getPort(const std::string& portId) override {
        if (portId == "usb0") {
            return &m_portUsb;
        } else if (portId == "ext_s3") {
            return &m_portExt;
        } else if (portId == "manual") {
            return &m_portManual;
        }
        return nullptr;
    }

    void setHotplugCallback(std::function<void(bool, const std::string&)> cb) override {
        m_hotplugCb = cb;
    }

private:
    S3UsbSerialPort m_portUsb;
    S3SerialPort m_portExt;
    S3SerialPort m_portManual;
    std::function<void(bool, const std::string&)> m_hotplugCb;
};

class S3UartBackend : public cbdos::uart::IUartBackend {
public:
    S3UartBackend() : m_uart(1) {
        m_presets = {
            {"S3 Ext (TX:15 RX:16)", 15, 16},
            {"S3 Alt (TX:17 RX:18)", 17, 18}
        };
    }

    bool init(int txPin, int rxPin, uint32_t baudrate) override {
        if (m_isInitialized) {
            deinit();
        }

        m_currentTxPin = txPin;
        m_currentRxPin = rxPin;
        m_currentBaud = baudrate;

        pinMode(rxPin, INPUT_PULLUP);
        m_uart.begin(baudrate, SERIAL_8N1, rxPin, txPin);
        m_isInitialized = true;
        return true;
    }

    void deinit() override {
        if (!m_isInitialized) return;
        m_uart.end();
        m_isInitialized = false;
    }

    bool isInitialized() const override {
        return m_isInitialized;
    }

    size_t available() override {
        if (!m_isInitialized) return 0;
        int avail = m_uart.available();
        return (avail > 0) ? (size_t)avail : 0;
    }

    size_t read(uint8_t* buffer, size_t maxLen) override {
        if (!m_isInitialized || !buffer || maxLen == 0) return 0;
        return m_uart.readBytes((char*)buffer, maxLen);
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
        return m_uart.write(data, len);
    }

    size_t writeString(const std::string& str) override {
        if (!m_isInitialized || str.empty()) return 0;
        return m_uart.print(str.c_str());
    }

    void flush() override {
        if (!m_isInitialized) return;
        m_uart.flush();
    }

    bool setBaudrate(uint32_t baudrate) override {
        if (!m_isInitialized) return false;
        return init(m_currentTxPin, m_currentRxPin, baudrate);
    }

    int getDefaultTxPin() const override { return 15; }
    int getDefaultRxPin() const override { return 16; }
    uint32_t getDefaultBaudrate() const override { return 115200; }
    const std::vector<cbdos::uart::UartPinPreset>& getPinPresets() const override { return m_presets; }

private:
    HardwareSerial m_uart;
    bool m_isInitialized = false;
    int m_currentTxPin = 15;
    int m_currentRxPin = 16;
    uint32_t m_currentBaud = 115200;
    std::vector<cbdos::uart::UartPinPreset> m_presets;
};

class S3GpioBackend : public cbdos::gpio::IGpioBackend {
public:
    bool setPinMode(int pin, cbdos::gpio::PinMode mode) override {
        if (!isPinAvailable(pin)) return false;

        switch (mode) {
            case cbdos::gpio::PinMode::Input:
                pinMode(pin, INPUT);
                break;
            case cbdos::gpio::PinMode::Output:
                pinMode(pin, OUTPUT);
                break;
            case cbdos::gpio::PinMode::InputPullUp:
                pinMode(pin, INPUT_PULLUP);
                break;
            case cbdos::gpio::PinMode::InputPullDown:
                pinMode(pin, INPUT_PULLDOWN);
                break;
        }
        return true;
    }

    bool digitalWrite(int pin, cbdos::gpio::PinLevel level) override {
        if (!isPinAvailable(pin)) return false;
        ::digitalWrite(pin, (level == cbdos::gpio::PinLevel::High) ? HIGH : LOW);
        return true;
    }

    cbdos::gpio::PinLevel digitalRead(int pin) override {
        if (!isPinAvailable(pin)) return cbdos::gpio::PinLevel::Low;
        int val = ::digitalRead(pin);
        return (val == HIGH) ? cbdos::gpio::PinLevel::High : cbdos::gpio::PinLevel::Low;
    }

    bool isPinAvailable(int pin) const override {
        if (pin < 0) return false;
        // Solo permitir pines listados en la whitelist de expansion del Device Tree
        for (int expPin : cbdos::board::EXPANSION_PINS) {
            if (pin == expPin) return true;
        }
        return false;
    }
};

static S3UartBackend s_s3UartBackend;
static S3SerialBackend s_s3SerialBackend;
static S3GpioBackend s_s3GpioBackend;

void initUartBackendS3() {
    cbdos::uart::setBackend(&s_s3UartBackend);
    cbdos::serial::setBackend(&s_s3SerialBackend);
}

void initGpioBackendS3() {
    cbdos::gpio::setBackend(&s_s3GpioBackend);
}

} // namespace bsp
} // namespace cbdos
