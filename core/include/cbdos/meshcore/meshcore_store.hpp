#pragma once

// Persistencia MeshCore Fase 1 (RFC-CBDOS-MESHCORE-FULL).
// Solo usa cbdos::storage::IStorageBackend (core puro, Regla 8).
// Tier 1 crítico en /flash (arranca sin SD), Tier 2 bulk en SD si existe.
// Escritura coalescente: solo si dirty + cada 30 s; atómica vía tmp+copy.

#include "meshcore_types.hpp"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace cbdos {
namespace meshcore {

struct MeshSelfSettings {
    std::string alias;
    double lat = 0.0;
    double lon = 0.0;
    bool sharePosition = false;
    bool autoRetry = true;
    bool autoResetPath = true;
    uint8_t nAcks = 1;
    bool saveDrafts = true;
    bool showHops = true;
};

class MeshStore {
public:
    static const char* kContactsPath;
    static const char* kChannelsPath;
    static const char* kSelfPath;
    static const char* kThreadsPath;
    static const char* kThreadsFullPath;
    static const char* kDirFlash;
    static const char* kDirSd;

    static constexpr uint32_t kSaveCoalesceMs = 30000;
    static constexpr size_t kFlashMsgsPerThread = 50;
    static constexpr size_t kSdMsgsPerThread = 200;

    // true si toca guardar (dirty + ventana de 30 s cumplida).
    static bool shouldSave(bool dirty, uint32_t nowMs, uint32_t lastSaveMs);

    // Escritura atómica: tmp + copy + delete tmp.
    static bool saveAtomic(const char* path, const std::string& content);
    static void ensureDirs();

    static bool saveContacts(const std::vector<MeshContact>& contacts);
    static bool loadContacts(std::vector<MeshContact>& out);

    static bool saveChannels(const MeshChannel channels[CHANNEL_COUNT]);
    static bool loadChannels(MeshChannel channels[CHANNEL_COUNT]);

    static bool saveSelf(const MeshSelfSettings& self);
    static bool loadSelf(MeshSelfSettings& out);

    // threads: cap a 50/hilo en flash; si hay SD, espejo largo en Tier 2.
    static bool saveThreads(const std::map<std::string, DMThread>& threads);
    static bool loadThreads(std::map<std::string, DMThread>& out);

    // Serialización expuesta para tests (binario compacto versionado).
    static std::string encodeContacts(const std::vector<MeshContact>& contacts);
    static bool decodeContacts(const std::string& blob, std::vector<MeshContact>& out);
    static std::string encodeThreads(const std::map<std::string, DMThread>& threads,
                                     size_t capPerThread);
    static bool decodeThreads(const std::string& blob,
                              std::map<std::string, DMThread>& out);
};

}  // namespace meshcore
}  // namespace cbdos
