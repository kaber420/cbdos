#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <cstddef>

namespace cbdos {
namespace security {

struct VaultItem {
    std::string id;         // Identificador único (ej: "ap-torre-norte", "wifi-casa")
    std::string label;      // Nombre descriptivo (ej: "Sectorial Norte 5G", "Mi Router")
    std::string host;       // IP o FQDN opcional (ej: "192.168.1.20")
    uint16_t    port = 0;   // Puerto opcional (ej: 443, 22, o 0 si no aplica)
    std::string user;       // Usuario opcional (ej: "ubnt", "admin")
    std::string secret;     // Contraseña o clave privada (descifrada solo si la bóveda está desbloqueada)
    std::string type;       // Tipo/Namespace (ej: "ubiquiti", "mikrotik", "ssh", "wifi", "mesh", "token")
    std::string notes;      // Metadatos o comentarios opcionales
};

class VaultService {
public:
    static VaultService& getInstance();

    // Inicialización y Estado de Bloqueo
    bool init();
    bool isUnlocked() const;
    bool unlock(const std::string& userPin);
    void lock();

    // Gestión de Credenciales MessagePack por Namespace
    bool saveItem(const std::string& ns, const VaultItem& item);
    bool getItem(const std::string& ns, const std::string& id, VaultItem& outItem);
    bool findByHost(const std::string& ns, const std::string& host, VaultItem& outItem);
    bool deleteItem(const std::string& ns, const std::string& id);
    bool listItems(const std::string& ns, std::vector<VaultItem>& outItems, bool includeSecrets = false);

    // Primitivas Criptográficas On-Demand (Para Chats Mesh, Notas o Cifrado de Archivos)
    bool encryptData(const uint8_t* plaintext, size_t plainLen, std::vector<uint8_t>& outCiphertext);
    bool decryptData(const uint8_t* ciphertext, size_t cipherLen, std::vector<uint8_t>& outPlaintext);

    // Exportación e Importación Portable (.cbdvault)
    bool exportNamespace(const std::string& ns, const std::string& filePath, const std::string& exportPassphrase);
    bool importNamespace(const std::string& ns, const std::string& filePath, const std::string& importPassphrase);

private:
    VaultService() = default;
    ~VaultService();

    // Derivación de clave local vinculada al hardware (ChipID + NVS Salt + PIN)
    bool deriveLocalKey(const std::string& pin, uint8_t outKey[32]);

    // Métodos internos de serialización MessagePack y cifrado atómico de secretos
    bool loadNamespaceDb(const std::string& ns);
    bool saveNamespaceDb(const std::string& ns);
    bool encryptSecret(const std::string& secretPlain, std::vector<uint8_t>& outIv, std::vector<uint8_t>& outTag, std::vector<uint8_t>& outEncrypted);
    bool decryptSecret(const std::vector<uint8_t>& iv, const std::vector<uint8_t>& tag, const std::vector<uint8_t>& encrypted, std::string& outPlain);

    bool m_initialized = false;
    bool m_unlocked = false;
    uint8_t m_activeKey[32] = {0};

    // Caché en RAM de ítems indexados por namespace
    struct CachedRecord {
        VaultItem item;
        std::vector<uint8_t> iv;
        std::vector<uint8_t> tag;
        std::vector<uint8_t> encryptedSecret;
    };
    std::string m_loadedNamespace;
    std::vector<CachedRecord> m_cachedRecords;
};

} // namespace security
} // namespace cbdos
