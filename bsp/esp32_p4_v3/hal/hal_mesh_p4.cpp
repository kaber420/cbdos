#include "cbdos/mesh/mesh_transport.hpp"
#include "cbdos/mesh/mesh_engine.hpp"
#include "cbdos/network_interface.hpp"
#include "usb_cdc_loader_port.hpp"
#include <esp_loader_io.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <cstring>
#include <cstdio>

static const char* TAG = "CBDOS_RADIO_USB_P4";

namespace {

static uint8_t calc_crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        uint8_t extract = data[i];
        for (uint8_t t = 8; t; t--) {
            uint8_t sum = (crc ^ extract) & 0x01;
            crc >>= 1;
            if (sum) crc ^= 0x8C;
            extract >>= 1;
        }
    }
    return crc;
}

class UsbCdcRadioTransport : public cbdos::mesh::IMeshTransport, public cbdos::network::INetworkInterface {
public:
    UsbCdcRadioTransport() = default;
    virtual ~UsbCdcRadioTransport() {
        stop();
    }

    bool init(uint8_t channel = 1) override {
        m_channel = channel;

        // Inicializar puerto USB CDC-ACM Host
        esp_loader_error_t err = loader_port_usb_cdc_init(1500);
        if (err != ESP_LOADER_SUCCESS) {
            ESP_LOGW(TAG, "Módem USB no detectado al iniciar (se reintentará bajo demanda)");
            return false;
        }

        // Consultar estado inicial del módem
        queryModemStatus();

        // Iniciar hilo receptor de tramas L2 en segundo plano
        if (!m_rxTaskHandle) {
            m_running = true;
            xTaskCreatePinnedToCore(rxTaskTrampoline, "usb_radio_rx", 4096, this, 5, &m_rxTaskHandle, 0);
        }

        m_ready = true;
        ESP_LOGI(TAG, "Módem de Radio USB (ESP32-C3) inicializado en Canal %u", m_channel);
        return true;
    }

    void stop() override {
        m_running = false;
        m_ready = false;
        if (m_rxTaskHandle) {
            vTaskDelay(pdMS_TO_TICKS(50));
            m_rxTaskHandle = nullptr;
        }
    }

    bool isReady() const override {
        return m_ready;
    }

    bool sendRaw(const uint8_t* dest_mac, const uint8_t* data, size_t len) override {
        return sendFrame(dest_mac, data, len);
    }

    int sendPacket(const uint8_t* buffer, size_t len) override {
        return sendFrame(nullptr, buffer, len) ? static_cast<int>(len) : -1;
    }

    bool sendFrame(const uint8_t* dest_mac, const uint8_t* payload, size_t len) {
        if (!m_ready || !payload || len == 0 || len > 250) {
            return false;
        }

        // Trama DIR_PC_TO_DONGLE (0x01): [0xAA, 0x55, 0x01, LEN_H, LEN_L] + PAYLOAD + CRC8
        uint8_t tx_buf[265];
        tx_buf[0] = 0xAA;
        tx_buf[1] = 0x55;
        tx_buf[2] = 0x01; // DIR_PC_TO_DONGLE
        tx_buf[3] = (uint8_t)((len >> 8) & 0xFF);
        tx_buf[4] = (uint8_t)(len & 0xFF);
        memcpy(tx_buf + 5, payload, len);
        uint8_t crc = calc_crc8(payload, len);
        tx_buf[5 + len] = crc;

        esp_loader_error_t err = loader_port_write(tx_buf, 6 + len, 500);
        return (err == ESP_LOADER_SUCCESS);
    }

    void setRecvCallback(cbdos::mesh::MeshRawRecvCallback cb) override {
        m_recvCb = cb;
    }

    bool getMacAddress(uint8_t out_mac[6]) override {
        if (!out_mac) return false;
        if (m_macValid) {
            memcpy(out_mac, m_mac, 6);
            return true;
        }
        memset(out_mac, 0, 6);
        return false;
    }

    uint8_t getChannel() const override {
        return m_channel;
    }

    uint8_t getChannel() override {
        return m_channel;
    }

    bool setChannel(uint8_t channel) override {
        if (channel < 1 || channel > 13) return false;
        m_channel = channel;
        // Enviar comando binario RADIO_CMD_SET_CHAN (0x03)
        uint8_t payload[2] = { 0x03, channel };
        uint8_t cmd_buf[8] = {
            0xAA, 0x55, 0x03, 0x00, 0x02,
            0x03, channel,
            calc_crc8(payload, 2)
        };
        loader_port_write(cmd_buf, sizeof(cmd_buf), 300);
        return true;
    }

    bool setRadioMode(cbdos::mesh::RadioMode mode) override {
        m_mode = mode;
        uint8_t rawMode = (mode == cbdos::mesh::RadioMode::EspNowLR) ? 0x02 : 0x01;
        uint8_t payload[2] = { 0x02, rawMode };
        uint8_t cmd_buf[8] = {
            0xAA, 0x55, 0x03, 0x00, 0x02,
            0x02, rawMode,
            calc_crc8(payload, 2)
        };
        loader_port_write(cmd_buf, sizeof(cmd_buf), 300);
        return true;
    }

    cbdos::mesh::RadioMode getRadioMode() const override {
        return m_mode;
    }

    cbdos::network::InterfaceType getType() const override {
        return cbdos::network::InterfaceType::SerialModem;
    }

    const char* getName() const override {
        return "Slot 2: Módem USB Radio";
    }

    const char* getAlias() const override {
        return m_alias[0] ? m_alias : "Módem USB";
    }

    int8_t getTxPower() const override {
        return static_cast<int8_t>(m_txPower * 0.25f);
    }

    cbdos::network::InterfaceMode getMode() const override {
        if (!m_ready) return cbdos::network::InterfaceMode::Off;
        return (m_mode == cbdos::mesh::RadioMode::EspNowLR) ? 
               cbdos::network::InterfaceMode::EspNowLR : 
               cbdos::network::InterfaceMode::EspNow;
    }

    bool setMode(cbdos::network::InterfaceMode mode) override {
        if (mode == cbdos::network::InterfaceMode::Off) {
            stop();
        } else {
            if (!m_ready) init(m_channel);
            if (mode == cbdos::network::InterfaceMode::EspNowLR) {
                setRadioMode(cbdos::mesh::RadioMode::EspNowLR);
            } else {
                setRadioMode(cbdos::mesh::RadioMode::EspNowNormal);
            }
        }
        return true;
    }

    void setPacketRecvCallback(cbdos::network::PacketRecvCallback cb, void* userCtx) override {
        m_packetRecvCb = cb;
        m_packetUserCtx = userCtx;
    }

private:
    static void rxTaskTrampoline(void* arg) {
        static_cast<UsbCdcRadioTransport*>(arg)->rxTaskLoop();
    }

    void rxTaskLoop() {
        ESP_LOGI(TAG, "Hilo de escucha de tramas USB de Radio iniciado");
        while (m_running) {
            uint8_t b = 0;
            if (loader_port_read(&b, 1, 100) == ESP_LOADER_SUCCESS && b == 0xAA) {
                if (loader_port_read(&b, 1, 100) == ESP_LOADER_SUCCESS && b == 0x55) {
                    uint8_t dir = 0, len_h = 0, len_l = 0;
                    if (loader_port_read(&dir, 1, 100) == ESP_LOADER_SUCCESS &&
                        loader_port_read(&len_h, 1, 100) == ESP_LOADER_SUCCESS &&
                        loader_port_read(&len_l, 1, 100) == ESP_LOADER_SUCCESS) {
                        
                        uint16_t plen = (len_h << 8) | len_l;
                        if (plen > 0 && plen <= 300) {
                            uint8_t payload[320] = {0};
                            if (loader_port_read(payload, plen, 200) == ESP_LOADER_SUCCESS) {
                                uint8_t crc = 0;
                                loader_port_read(&crc, 1, 50);

                                if (dir == 0x02 && plen >= 7) {
                                    // DIR_DONGLE_TO_PC: [SRC_MAC (6B)][RSSI (1B)][DATA (N B)]
                                    const uint8_t* src_mac = payload;
                                    int8_t rssi = static_cast<int8_t>(payload[6]);
                                    const uint8_t* data = payload + 7;
                                    size_t data_len = plen - 7;

                                    if (m_recvCb) {
                                        m_recvCb(src_mac, data, data_len, rssi);
                                    }
                                    if (m_packetRecvCb) {
                                        m_packetRecvCb(data, data_len, rssi, 0, m_packetUserCtx);
                                    }
                                } else if (dir == 0x04 && plen >= 10 && payload[0] == 0x01 && payload[1] == 0x00) {
                                    // Status response: [CMD 1B][STATUS 1B][MAC 6B][MODE 1B][CHAN 1B][PWR 1B][PEERS 1B][ALIAS...]
                                    memcpy(m_mac, payload + 2, 6);
                                    m_macValid = true;
                                    m_mode = (payload[8] == 0x02) ? cbdos::mesh::RadioMode::EspNowLR : cbdos::mesh::RadioMode::EspNowNormal;
                                    m_channel = payload[9];
                                    if (plen >= 11) m_txPower = payload[10];
                                    if (plen >= 12) m_peerCount = payload[11];
                                    if (plen > 12) {
                                        size_t alias_len = plen - 12;
                                        if (alias_len > 31) alias_len = 31;
                                        memcpy(m_alias, payload + 12, alias_len);
                                        m_alias[alias_len] = '\0';
                                    }
                                    ESP_LOGI(TAG, "Módem USB Detectado: Alias='%s', MAC=%02X:%02X:%02X:%02X:%02X:%02X, Ch=%u, Mode=%s",
                                             m_alias, m_mac[0], m_mac[1], m_mac[2], m_mac[3], m_mac[4], m_mac[5],
                                             m_channel, (m_mode == cbdos::mesh::RadioMode::EspNowLR) ? "LR" : "Normal");
                                }
                            }
                        }
                    }
                }
            }
            vTaskDelay(pdMS_TO_TICKS(5));
        }
        vTaskDelete(NULL);
    }

    void queryModemStatus() {
        uint8_t cmd = 0x01; // RADIO_CMD_GET_STATUS
        uint8_t crc = calc_crc8(&cmd, 1);
        uint8_t get_status_frame[] = { 0xAA, 0x55, 0x03, 0x00, 0x01, cmd, crc };
        loader_port_write(get_status_frame, sizeof(get_status_frame), 300);
    }

    TaskHandle_t m_rxTaskHandle = nullptr;
    cbdos::mesh::MeshRawRecvCallback m_recvCb = nullptr;
    cbdos::network::PacketRecvCallback m_packetRecvCb = nullptr;
    void* m_packetUserCtx = nullptr;

    cbdos::mesh::RadioMode m_mode = cbdos::mesh::RadioMode::EspNowNormal;
    uint8_t m_channel = 1;
    uint8_t m_txPower = 84;
    uint8_t m_peerCount = 0;
    char m_alias[32] = {0};
    uint8_t m_mac[6] = {0};
    bool m_macValid = false;
    volatile bool m_running = false;
    bool m_ready = false;
};

static UsbCdcRadioTransport s_usbRadioTransport;

} // anonymous namespace

namespace cbdos {
namespace bsp {

void initMeshTransportP4() {
    mesh::MeshEngine::getInstance().setTransport(&s_usbRadioTransport);
    network::NetworkInterfaceManager::getInstance().registerInterface(2, &s_usbRadioTransport);
}

} // namespace bsp
} // namespace cbdos
