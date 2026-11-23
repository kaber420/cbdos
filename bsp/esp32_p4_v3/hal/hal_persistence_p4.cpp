#include "cbdos/persistence.hpp"
#include <nvs_flash.h>
#include <nvs.h>
#include <esp_log.h>
#include <cstring>

static const char* TAG_PERSISTENCE = "NVS_P4";

namespace cbdos {
namespace bsp {

class EspIdfPersistenceBackend : public persistence::IPersistenceBackend {
public:
    EspIdfPersistenceBackend() : m_handle(0), m_isOpen(false) {}

    ~EspIdfPersistenceBackend() override {
        end();
    }

    bool begin(const char* nameSpace, bool readOnly = false) override {
        if (m_isOpen) {
            end();
        }

        esp_err_t err = nvs_open(nameSpace, readOnly ? NVS_READONLY : NVS_READWRITE, &m_handle);
        if (err == ESP_ERR_NVS_NOT_INITIALIZED) {
            esp_err_t initErr = nvs_flash_init();
            if (initErr == ESP_ERR_NVS_NO_FREE_PAGES || initErr == ESP_ERR_NVS_NEW_VERSION_FOUND) {
                nvs_flash_erase();
                nvs_flash_init();
            }
            err = nvs_open(nameSpace, readOnly ? NVS_READONLY : NVS_READWRITE, &m_handle);
        }

        m_isOpen = (err == ESP_OK);
        return m_isOpen;
    }

    void end() override {
        if (m_isOpen) {
            nvs_commit(m_handle);
            nvs_close(m_handle);
            m_handle = 0;
            m_isOpen = false;
        }
    }

    bool setUChar(const char* key, uint8_t value) override {
        if (!m_isOpen) return false;
        return (nvs_set_u8(m_handle, key, value) == ESP_OK);
    }

    uint8_t getUChar(const char* key, uint8_t defaultVal) override {
        if (!m_isOpen) return defaultVal;
        uint8_t val = defaultVal;
        nvs_get_u8(m_handle, key, &val);
        return val;
    }

    bool setUShort(const char* key, uint16_t value) override {
        if (!m_isOpen) return false;
        return (nvs_set_u16(m_handle, key, value) == ESP_OK);
    }

    uint16_t getUShort(const char* key, uint16_t defaultVal) override {
        if (!m_isOpen) return defaultVal;
        uint16_t val = defaultVal;
        nvs_get_u16(m_handle, key, &val);
        return val;
    }

    bool setUInt(const char* key, uint32_t value) override {
        if (!m_isOpen) return false;
        return (nvs_set_u32(m_handle, key, value) == ESP_OK);
    }

    uint32_t getUInt(const char* key, uint32_t defaultVal) override {
        if (!m_isOpen) return defaultVal;
        uint32_t val = defaultVal;
        nvs_get_u32(m_handle, key, &val);
        return val;
    }

    bool setInt(const char* key, int32_t value) override {
        if (!m_isOpen) return false;
        return (nvs_set_i32(m_handle, key, value) == ESP_OK);
    }

    int32_t getInt(const char* key, int32_t defaultVal) override {
        if (!m_isOpen) return defaultVal;
        int32_t val = defaultVal;
        nvs_get_i32(m_handle, key, &val);
        return val;
    }

    bool setFloat(const char* key, float value) override {
        if (!m_isOpen) return false;
        return (nvs_set_blob(m_handle, key, &value, sizeof(float)) == ESP_OK);
    }

    float getFloat(const char* key, float defaultVal) override {
        if (!m_isOpen) return defaultVal;
        float val = defaultVal;
        size_t len = sizeof(float);
        if (nvs_get_blob(m_handle, key, &val, &len) == ESP_OK && len == sizeof(float)) {
            return val;
        }
        return defaultVal;
    }

    bool setBool(const char* key, bool value) override {
        if (!m_isOpen) return false;
        return (nvs_set_u8(m_handle, key, value ? 1 : 0) == ESP_OK);
    }

    bool getBool(const char* key, bool defaultVal) override {
        if (!m_isOpen) return defaultVal;
        uint8_t val = defaultVal ? 1 : 0;
        nvs_get_u8(m_handle, key, &val);
        return (val != 0);
    }

    bool setString(const char* key, const std::string& value) override {
        if (!m_isOpen) return false;
        return (nvs_set_str(m_handle, key, value.c_str()) == ESP_OK);
    }

    std::string getString(const char* key, const std::string& defaultVal) override {
        if (!m_isOpen) return defaultVal;
        char buf[256];
        size_t len = sizeof(buf);
        if (nvs_get_str(m_handle, key, buf, &len) == ESP_OK) {
            return std::string(buf);
        }
        return defaultVal;
    }

    bool remove(const char* key) override {
        if (!m_isOpen) return false;
        return (nvs_erase_key(m_handle, key) == ESP_OK);
    }

    bool clear() override {
        if (!m_isOpen) return false;
        return (nvs_erase_all(m_handle) == ESP_OK);
    }

private:
    nvs_handle_t m_handle;
    bool m_isOpen;
};

// Instancia estática singleton para registrar en cbdos
static EspIdfPersistenceBackend s_p4PersistenceBackend;

void initPersistenceBackend() {
    persistence::setBackend(&s_p4PersistenceBackend);
    ESP_LOGI(TAG_PERSISTENCE, "ESP-IDF NVS Persistence Backend inicializado e inyectado.");
}

} // namespace bsp
} // namespace cbdos
