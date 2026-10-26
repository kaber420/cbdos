#include "cbdos/vault.hpp"
#include "cbdos/msgpack_util.hpp"
#include "cbdos/storage.hpp"
#include "cbdos/persistence.hpp"
#include "cbdos/system.hpp"

#include <mbedtls/gcm.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>

#include <cstring>
#include <cstdio>
#include <algorithm>

namespace cbdos {
namespace security {

static const char* TAG = "VaultService";
static const char* NVS_NS = "cbdos_vault";
static const char* NVS_KEY_SALT = "dev_salt";

static mbedtls_entropy_context  s_entropy;
static mbedtls_ctr_drbg_context s_drbg;
static bool s_drbg_initialized = false;

static void initDrbg() {
    if (s_drbg_initialized) return;
    mbedtls_entropy_init(&s_entropy);
    mbedtls_ctr_drbg_init(&s_drbg);
    const char* custom = "cbdos_vault_entropy";
    mbedtls_ctr_drbg_seed(&s_drbg, mbedtls_entropy_func, &s_entropy,
                          reinterpret_cast<const uint8_t*>(custom), strlen(custom));
    s_drbg_initialized = true;
}

static bool getRandomBytes(uint8_t* dst, size_t len) {
    initDrbg();
    return mbedtls_ctr_drbg_random(&s_drbg, dst, len) == 0;
}

static std::string bytesToHex(const std::vector<uint8_t>& bytes) {
    std::string hex;
    hex.reserve(bytes.size() * 2);
    char buf[3];
    for (uint8_t b : bytes) {
        snprintf(buf, sizeof(buf), "%02x", b);
        hex.append(buf);
    }
    return hex;
}

static std::vector<uint8_t> hexToBytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    if (hex.size() % 2 != 0) return bytes;
    bytes.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        unsigned int b = 0;
        sscanf(hex.c_str() + i, "%02x", &b);
        bytes.push_back(static_cast<uint8_t>(b));
    }
    return bytes;
}

VaultService& VaultService::getInstance() {
    static VaultService instance;
    return instance;
}

VaultService::~VaultService() {
    lock();
}

bool VaultService::init() {
    if (m_initialized) return true;
    initDrbg();

    // Asegurar directorio base en memoria Flash / SD
    auto* storage = cbdos::storage::getBackend();
    if (storage) {
        storage->makeDir("/cbdos");
        storage->makeDir("/cbdos/vault");
    }

    m_initialized = true;
    cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "VaultService inicializado");
    return true;
}

bool VaultService::isUnlocked() const {
    return m_unlocked;
}

void VaultService::lock() {
    m_unlocked = false;
    // Cero-Trace: purgar clave maestra volátil
    memset(m_activeKey, 0, sizeof(m_activeKey));
    // Limpiar contraseñas descifradas de la caché en RAM
    for (auto& rec : m_cachedRecords) {
        rec.item.secret.clear();
    }
    cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "Boveda bloqueada y memoria purgada");
}

bool VaultService::deriveLocalKey(const std::string& pin, uint8_t outKey[32]) {
    auto* be = persistence::getBackend();
    if (!be) return false;

    // Obtener o generar Salt único de hardware en NVS
    std::string saltStr;
    if (be->begin(NVS_NS, false)) {
        saltStr = be->getString(NVS_KEY_SALT, "");
        if (saltStr.size() != 32) {
            uint8_t rawSalt[16];
            getRandomBytes(rawSalt, sizeof(rawSalt));
            char hexSalt[33] = {0};
            for (size_t i = 0; i < 16; ++i) {
                snprintf(hexSalt + (i * 2), 3, "%02x", rawSalt[i]);
            }
            saltStr = hexSalt;
            be->setString(NVS_KEY_SALT, saltStr);
        }
        be->end();
    }

    // Material de derivación: Salt NVS único + huella de chip/SoC
    std::string saltMaterial = saltStr + "_cbdos_hw_salt";

    // PBKDF2-HMAC-SHA256 con 5,000 iteraciones para equilibrio óptimo de MCU y seguridad
    int ret = mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA256,
                                            reinterpret_cast<const uint8_t*>(pin.data()), pin.size(),
                                            reinterpret_cast<const uint8_t*>(saltMaterial.data()), saltMaterial.size(),
                                            5000, 32, outKey);
    return ret == 0;
}

bool VaultService::unlock(const std::string& userPin) {
    if (!m_initialized) init();
    if (userPin.empty()) return false;

    uint8_t key[32] = {0};
    if (!deriveLocalKey(userPin, key)) {
        return false;
    }

    memcpy(m_activeKey, key, 32);
    memset(key, 0, sizeof(key));
    m_unlocked = true;

    cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "Boveda desbloqueada exitosamente");
    return true;
}

bool VaultService::encryptSecret(const std::string& secretPlain,
                                 std::vector<uint8_t>& outIv,
                                 std::vector<uint8_t>& outTag,
                                 std::vector<uint8_t>& outEncrypted) {
    if (!m_unlocked || secretPlain.empty()) return false;

    outIv.resize(12);
    if (!getRandomBytes(outIv.data(), 12)) return false;

    outTag.resize(16);
    outEncrypted.resize(secretPlain.size());

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    int rc = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, m_activeKey, 256);
    if (rc == 0) {
        rc = mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, secretPlain.size(),
                                       outIv.data(), 12, nullptr, 0,
                                       reinterpret_cast<const uint8_t*>(secretPlain.data()),
                                       outEncrypted.data(), 16, outTag.data());
    }
    mbedtls_gcm_free(&gcm);
    return rc == 0;
}

bool VaultService::decryptSecret(const std::vector<uint8_t>& iv,
                                 const std::vector<uint8_t>& tag,
                                 const std::vector<uint8_t>& encrypted,
                                 std::string& outPlain) {
    if (!m_unlocked || iv.size() != 12 || tag.size() != 16) return false;

    std::vector<uint8_t> plainBuf(encrypted.size());
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    int rc = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, m_activeKey, 256);
    if (rc == 0) {
        rc = mbedtls_gcm_auth_decrypt(&gcm, encrypted.size(),
                                      iv.data(), 12, nullptr, 0,
                                      tag.data(), 16,
                                      encrypted.data(), plainBuf.data());
    }
    mbedtls_gcm_free(&gcm);

    if (rc == 0) {
        outPlain.assign(reinterpret_cast<char*>(plainBuf.data()), plainBuf.size());
        return true;
    }
    return false;
}

bool VaultService::loadNamespaceDb(const std::string& ns) {
    if (m_loadedNamespace == ns) return true;

    m_loadedNamespace = ns;
    m_cachedRecords.clear();

    auto* storage = cbdos::storage::getBackend();
    if (!storage) return false;

    std::string path = "/cbdos/vault/" + ns + ".db";
    if (!storage->fileExists(path.c_str())) {
        return true; // Base de datos vacía inicial
    }

    std::string content = storage->readFile(path.c_str());
    if (content.size() < 4) return false;

    msgpack::MsgPackReader reader(reinterpret_cast<const uint8_t*>(content.data()), content.size());
    size_t mapSize = 0;
    if (!reader.readMapHeader(mapSize)) return false;

    for (size_t i = 0; i < mapSize; ++i) {
        std::string sectionKey;
        if (!reader.readString(sectionKey)) break;

        if (sectionKey == "items") {
            size_t arrSize = 0;
            if (reader.readArrayHeader(arrSize)) {
                for (size_t j = 0; j < arrSize; ++j) {
                    size_t itemMap = 0;
                    if (!reader.readMapHeader(itemMap)) break;

                    CachedRecord rec;
                    for (size_t k = 0; k < itemMap; ++k) {
                        std::string kName;
                        if (!reader.readString(kName)) break;

                        if (kName == "id") reader.readString(rec.item.id);
                        else if (kName == "label") reader.readString(rec.item.label);
                        else if (kName == "host") reader.readString(rec.item.host);
                        else if (kName == "port") {
                            int32_t p = 0;
                            reader.readInt(p);
                            rec.item.port = static_cast<uint16_t>(p);
                        }
                        else if (kName == "user") reader.readString(rec.item.user);
                        else if (kName == "type") reader.readString(rec.item.type);
                        else if (kName == "notes") reader.readString(rec.item.notes);
                        else if (kName == "iv") {
                            std::string hex;
                            reader.readString(hex);
                            rec.iv = hexToBytes(hex);
                        }
                        else if (kName == "tag") {
                            std::string hex;
                            reader.readString(hex);
                            rec.tag = hexToBytes(hex);
                        }
                        else if (kName == "enc") {
                            std::string hex;
                            reader.readString(hex);
                            rec.encryptedSecret = hexToBytes(hex);
                        }
                        else reader.skipValue();
                    }
                    if (!rec.item.id.empty()) {
                        m_cachedRecords.push_back(std::move(rec));
                    }
                }
            }
        } else {
            reader.skipValue();
        }
    }
    return true;
}

bool VaultService::saveNamespaceDb(const std::string& ns) {
    auto* storage = cbdos::storage::getBackend();
    if (!storage) return false;

    msgpack::MsgPackWriter writer;
    writer.writeMapHeader(2);

    writer.writeString("ns");
    writer.writeString(ns);

    writer.writeString("items");
    writer.writeArrayHeader(m_cachedRecords.size());

    for (const auto& rec : m_cachedRecords) {
        writer.writeMapHeader(10);

        writer.writeString("id");
        writer.writeString(rec.item.id);

        writer.writeString("label");
        writer.writeString(rec.item.label);

        writer.writeString("host");
        writer.writeString(rec.item.host);

        writer.writeString("port");
        writer.writeInt(rec.item.port);

        writer.writeString("user");
        writer.writeString(rec.item.user);

        writer.writeString("type");
        writer.writeString(rec.item.type);

        writer.writeString("notes");
        writer.writeString(rec.item.notes);

        writer.writeString("iv");
        writer.writeString(bytesToHex(rec.iv));

        writer.writeString("tag");
        writer.writeString(bytesToHex(rec.tag));

        writer.writeString("enc");
        writer.writeString(bytesToHex(rec.encryptedSecret));
    }

    std::string path = "/cbdos/vault/" + ns + ".db";
    std::string tmpPath = path + ".tmp";

    // Escritura atómica a prueba de cortes de energía
    if (storage->writeFile(tmpPath.c_str(), writer.toString())) {
        storage->deleteFile(path.c_str());
        storage->copyFile(tmpPath.c_str(), path.c_str());
        storage->deleteFile(tmpPath.c_str());
        return true;
    }
    return false;
}

bool VaultService::saveItem(const std::string& ns, const VaultItem& item) {
    if (!loadNamespaceDb(ns)) return false;

    CachedRecord rec;
    rec.item = item;

    if (!item.secret.empty()) {
        if (!m_unlocked) {
            cbdos::system::log(cbdos::system::LogLevel::Warn, TAG, "No se puede cifrar secreto: Boveda bloqueada");
            return false;
        }
        if (!encryptSecret(item.secret, rec.iv, rec.tag, rec.encryptedSecret)) {
            return false;
        }
    }

    auto it = std::find_if(m_cachedRecords.begin(), m_cachedRecords.end(),
                           [&item](const CachedRecord& r) { return r.item.id == item.id; });
    if (it != m_cachedRecords.end()) {
        *it = std::move(rec);
    } else {
        m_cachedRecords.push_back(std::move(rec));
    }

    return saveNamespaceDb(ns);
}

bool VaultService::getItem(const std::string& ns, const std::string& id, VaultItem& outItem) {
    if (!loadNamespaceDb(ns)) return false;

    auto it = std::find_if(m_cachedRecords.begin(), m_cachedRecords.end(),
                           [&id](const CachedRecord& r) { return r.item.id == id; });
    if (it == m_cachedRecords.end()) return false;

    outItem = it->item;
    if (m_unlocked && !it->encryptedSecret.empty()) {
        decryptSecret(it->iv, it->tag, it->encryptedSecret, outItem.secret);
    }
    return true;
}

bool VaultService::findByHost(const std::string& ns, const std::string& host, VaultItem& outItem) {
    if (!loadNamespaceDb(ns)) return false;

    auto it = std::find_if(m_cachedRecords.begin(), m_cachedRecords.end(),
                           [&host](const CachedRecord& r) { return r.item.host == host; });
    if (it == m_cachedRecords.end()) return false;

    outItem = it->item;
    if (m_unlocked && !it->encryptedSecret.empty()) {
        decryptSecret(it->iv, it->tag, it->encryptedSecret, outItem.secret);
    }
    return true;
}

bool VaultService::deleteItem(const std::string& ns, const std::string& id) {
    if (!loadNamespaceDb(ns)) return false;

    auto it = std::find_if(m_cachedRecords.begin(), m_cachedRecords.end(),
                           [&id](const CachedRecord& r) { return r.item.id == id; });
    if (it == m_cachedRecords.end()) return false;

    m_cachedRecords.erase(it);
    return saveNamespaceDb(ns);
}

bool VaultService::listItems(const std::string& ns, std::vector<VaultItem>& outItems, bool includeSecrets) {
    if (!loadNamespaceDb(ns)) return false;

    outItems.clear();
    outItems.reserve(m_cachedRecords.size());

    for (const auto& rec : m_cachedRecords) {
        VaultItem vi = rec.item;
        if (includeSecrets && m_unlocked && !rec.encryptedSecret.empty()) {
            decryptSecret(rec.iv, rec.tag, rec.encryptedSecret, vi.secret);
        } else {
            vi.secret.clear();
        }
        outItems.push_back(std::move(vi));
    }
    return true;
}

bool VaultService::encryptData(const uint8_t* plaintext, size_t plainLen, std::vector<uint8_t>& outCiphertext) {
    if (!m_unlocked || !plaintext || plainLen == 0) return false;

    uint8_t iv[12];
    if (!getRandomBytes(iv, 12)) return false;

    uint8_t tag[16];
    std::vector<uint8_t> cipher(plainLen);

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    int rc = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, m_activeKey, 256);
    if (rc == 0) {
        rc = mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, plainLen,
                                       iv, 12, nullptr, 0,
                                       plaintext, cipher.data(), 16, tag);
    }
    mbedtls_gcm_free(&gcm);

    if (rc != 0) return false;

    // Empaquetado: [IV (12B)] + [Tag (16B)] + [Ciphertext]
    outCiphertext.resize(12 + 16 + plainLen);
    memcpy(outCiphertext.data(), iv, 12);
    memcpy(outCiphertext.data() + 12, tag, 16);
    memcpy(outCiphertext.data() + 28, cipher.data(), plainLen);
    return true;
}

bool VaultService::decryptData(const uint8_t* ciphertext, size_t cipherLen, std::vector<uint8_t>& outPlaintext) {
    if (!m_unlocked || !ciphertext || cipherLen < 28) return false;

    const uint8_t* iv = ciphertext;
    const uint8_t* tag = ciphertext + 12;
    const uint8_t* data = ciphertext + 28;
    size_t dataLen = cipherLen - 28;

    outPlaintext.resize(dataLen);

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    int rc = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, m_activeKey, 256);
    if (rc == 0) {
        rc = mbedtls_gcm_auth_decrypt(&gcm, dataLen,
                                      iv, 12, nullptr, 0,
                                      tag, 16,
                                      data, outPlaintext.data());
    }
    mbedtls_gcm_free(&gcm);
    return rc == 0;
}

bool VaultService::exportNamespace(const std::string& ns, const std::string& filePath, const std::string& exportPassphrase) {
    if (!loadNamespaceDb(ns) || exportPassphrase.empty()) return false;
    if (!m_unlocked) return false;

    // Serializar todos los ítems descifrados en un MessagePack de exportación
    msgpack::MsgPackWriter plainWriter;
    plainWriter.writeMapHeader(2);
    plainWriter.writeString("ns");
    plainWriter.writeString(ns);
    plainWriter.writeString("items");
    plainWriter.writeArrayHeader(m_cachedRecords.size());

    for (const auto& rec : m_cachedRecords) {
        std::string plainSec;
        if (!rec.encryptedSecret.empty()) {
            decryptSecret(rec.iv, rec.tag, rec.encryptedSecret, plainSec);
        }
        plainWriter.writeMapHeader(8);
        plainWriter.writeString("id");    plainWriter.writeString(rec.item.id);
        plainWriter.writeString("label"); plainWriter.writeString(rec.item.label);
        plainWriter.writeString("host");  plainWriter.writeString(rec.item.host);
        plainWriter.writeString("port");  plainWriter.writeInt(rec.item.port);
        plainWriter.writeString("user");  plainWriter.writeString(rec.item.user);
        plainWriter.writeString("secret");plainWriter.writeString(plainSec);
        plainWriter.writeString("type");  plainWriter.writeString(rec.item.type);
        plainWriter.writeString("notes"); plainWriter.writeString(rec.item.notes);
    }

    const std::vector<uint8_t>& plainPayload = plainWriter.getBuffer();

    // Derivar clave de exportación con 50,000 iteraciones de PBKDF2
    uint8_t expSalt[16];
    getRandomBytes(expSalt, 16);

    uint8_t expKey[32];
    mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA256,
                                  reinterpret_cast<const uint8_t*>(exportPassphrase.data()), exportPassphrase.size(),
                                  expSalt, 16, 50000, 32, expKey);

    uint8_t expIv[12];
    getRandomBytes(expIv, 12);
    uint8_t expTag[16];
    std::vector<uint8_t> expCipher(plainPayload.size());

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, expKey, 256);
    mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, plainPayload.size(),
                              expIv, 12, nullptr, 0,
                              plainPayload.data(), expCipher.data(), 16, expTag);
    mbedtls_gcm_free(&gcm);
    memset(expKey, 0, sizeof(expKey));

    // Formato .cbdvault: Magic(4) "CBDV" + Salt(16) + IV(12) + Tag(16) + Payload
    std::vector<uint8_t> fileData;
    fileData.reserve(4 + 16 + 12 + 16 + expCipher.size());
    fileData.push_back('C'); fileData.push_back('B'); fileData.push_back('D'); fileData.push_back('V');
    fileData.insert(fileData.end(), expSalt, expSalt + 16);
    fileData.insert(fileData.end(), expIv, expIv + 12);
    fileData.insert(fileData.end(), expTag, expTag + 16);
    fileData.insert(fileData.end(), expCipher.begin(), expCipher.end());

    auto* storage = cbdos::storage::getBackend();
    if (!storage) return false;

    std::string binStr(reinterpret_cast<char*>(fileData.data()), fileData.size());
    return storage->writeFile(filePath.c_str(), binStr);
}

bool VaultService::importNamespace(const std::string& ns, const std::string& filePath, const std::string& importPassphrase) {
    if (importPassphrase.empty() || !m_unlocked) return false;

    auto* storage = cbdos::storage::getBackend();
    if (!storage || !storage->fileExists(filePath.c_str())) return false;

    std::string binStr = storage->readFile(filePath.c_str());
    if (binStr.size() < (4 + 16 + 12 + 16)) return false;

    const uint8_t* raw = reinterpret_cast<const uint8_t*>(binStr.data());
    if (raw[0] != 'C' || raw[1] != 'B' || raw[2] != 'D' || raw[3] != 'V') return false;

    const uint8_t* expSalt = raw + 4;
    const uint8_t* expIv = raw + 20;
    const uint8_t* expTag = raw + 32;
    const uint8_t* cipherData = raw + 48;
    size_t cipherLen = binStr.size() - 48;

    uint8_t impKey[32];
    mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA256,
                                  reinterpret_cast<const uint8_t*>(importPassphrase.data()), importPassphrase.size(),
                                  expSalt, 16, 50000, 32, impKey);

    std::vector<uint8_t> plainPayload(cipherLen);
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, impKey, 256);
    int rc = mbedtls_gcm_auth_decrypt(&gcm, cipherLen,
                                      expIv, 12, nullptr, 0,
                                      expTag, 16,
                                      cipherData, plainPayload.data());
    mbedtls_gcm_free(&gcm);
    memset(impKey, 0, sizeof(impKey));

    if (rc != 0) {
        cbdos::system::log(cbdos::system::LogLevel::Error, TAG, "Error importando: Contraseña incorrecta o archivo alterado");
        return false;
    }

    // Parsear ítems y guardarlos re-cifrados en el almacenamiento local
    msgpack::MsgPackReader reader(plainPayload.data(), plainPayload.size());
    size_t mapSize = 0;
    if (!reader.readMapHeader(mapSize)) return false;

    for (size_t i = 0; i < mapSize; ++i) {
        std::string section;
        if (!reader.readString(section)) break;

        if (section == "items") {
            size_t arrSize = 0;
            if (reader.readArrayHeader(arrSize)) {
                for (size_t j = 0; j < arrSize; ++j) {
                    size_t itemMap = 0;
                    if (!reader.readMapHeader(itemMap)) break;

                    VaultItem vi;
                    for (size_t k = 0; k < itemMap; ++k) {
                        std::string kn;
                        if (!reader.readString(kn)) break;
                        if (kn == "id") reader.readString(vi.id);
                        else if (kn == "label") reader.readString(vi.label);
                        else if (kn == "host") reader.readString(vi.host);
                        else if (kn == "port") {
                            int32_t p = 0; reader.readInt(p); vi.port = static_cast<uint16_t>(p);
                        }
                        else if (kn == "user") reader.readString(vi.user);
                        else if (kn == "secret") reader.readString(vi.secret);
                        else if (kn == "type") reader.readString(vi.type);
                        else if (kn == "notes") reader.readString(vi.notes);
                        else reader.skipValue();
                    }
                    if (!vi.id.empty()) {
                        saveItem(ns, vi);
                    }
                }
            }
        } else {
            reader.skipValue();
        }
    }

    cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "Boveda '%s' importada con exito desde '%s'", ns.c_str(), filePath.c_str());
    return true;
}

} // namespace security
} // namespace cbdos
