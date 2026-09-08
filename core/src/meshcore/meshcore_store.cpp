#include "cbdos/meshcore/meshcore_store.hpp"
#include "cbdos/storage.hpp"
#include <cstring>

namespace cbdos {
namespace meshcore {

const char* MeshStore::kContactsPath = "/flash/data/meshcore/contacts.msgpack";
const char* MeshStore::kChannelsPath = "/flash/data/meshcore/channels.msgpack";
const char* MeshStore::kSelfPath = "/flash/data/meshcore/self.msgpack";
const char* MeshStore::kThreadsPath = "/flash/data/meshcore/threads.msgpack";
const char* MeshStore::kThreadsFullPath = "/sdcard/cbdos/data/meshcore/threads_full.msgpack";
const char* MeshStore::kDirFlash = "/flash/data/meshcore";
const char* MeshStore::kDirSd = "/sdcard/cbdos/data/meshcore";

namespace {

void putU16(std::string& o, uint16_t v) {
    o.push_back(static_cast<char>(v & 0xFF));
    o.push_back(static_cast<char>((v >> 8) & 0xFF));
}

void putU32(std::string& o, uint32_t v) {
    o.push_back(static_cast<char>(v & 0xFF));
    o.push_back(static_cast<char>((v >> 8) & 0xFF));
    o.push_back(static_cast<char>((v >> 16) & 0xFF));
    o.push_back(static_cast<char>((v >> 24) & 0xFF));
}

void putI32(std::string& o, int32_t v) { putU32(o, static_cast<uint32_t>(v)); }

bool getU16(const std::string& b, size_t& off, uint16_t& v) {
    if (off + 2 > b.size()) return false;
    v = static_cast<uint16_t>(static_cast<uint8_t>(b[off])) |
        (static_cast<uint16_t>(static_cast<uint8_t>(b[off + 1])) << 8);
    off += 2;
    return true;
}

bool getU32(const std::string& b, size_t& off, uint32_t& v) {
    if (off + 4 > b.size()) return false;
    v = static_cast<uint32_t>(static_cast<uint8_t>(b[off])) |
        (static_cast<uint32_t>(static_cast<uint8_t>(b[off + 1])) << 8) |
        (static_cast<uint32_t>(static_cast<uint8_t>(b[off + 2])) << 16) |
        (static_cast<uint32_t>(static_cast<uint8_t>(b[off + 3])) << 24);
    off += 4;
    return true;
}

bool getBytes(const std::string& b, size_t& off, uint8_t* dst, size_t n) {
    if (off + n > b.size()) return false;
    memcpy(dst, b.data() + off, n);
    off += n;
    return true;
}

}  // namespace

bool MeshStore::shouldSave(bool dirty, uint32_t nowMs, uint32_t lastSaveMs) {
    if (!dirty) return false;
    return (nowMs - lastSaveMs) >= kSaveCoalesceMs;
}

void MeshStore::ensureDirs() {
    // En SPIFFS flat makeDir es no-op; en SD/FAT es necesario.
    cbdos::storage::makeDir(kDirFlash);
    if (cbdos::storage::isSdMounted()) cbdos::storage::makeDir(kDirSd);
}

bool MeshStore::saveAtomic(const char* path, const std::string& content) {
    ensureDirs();
    std::string tmp = std::string(path) + ".tmp";
    if (!cbdos::storage::writeFile(tmp.c_str(), content)) return false;
    if (!cbdos::storage::copyFile(tmp.c_str(), path)) return false;
    cbdos::storage::deleteFile(tmp.c_str());
    return true;
}

std::string MeshStore::encodeContacts(const std::vector<MeshContact>& contacts) {
    std::string o;
    o += 'M';
    o += 'C';
    o += 'C';
    o.push_back(0x01);
    putU32(o, static_cast<uint32_t>(contacts.size()));
    for (const auto& c : contacts) {
        o.append(reinterpret_cast<const char*>(c.pubkey), PUBKEY_LEN);
        o.push_back(static_cast<char>(c.type));
        o.push_back(static_cast<char>(c.flags));
        o.push_back(static_cast<char>(c.outPathLen));
        o.append(reinterpret_cast<const char*>(c.outPath), CONTACT_OUTPATH_LEN);
        uint8_t nl = static_cast<uint8_t>(c.name.size() > 32 ? 32 : c.name.size());
        o.push_back(static_cast<char>(nl));
        o.append(c.name.data(), nl);
        putU32(o, c.lastAdvert);
        putI32(o, static_cast<int32_t>(c.lat * 1e6));
        putI32(o, static_cast<int32_t>(c.lon * 1e6));
        putU32(o, c.lastmod);
        o.push_back(c.favourite ? 0x01 : 0x00);
        putU32(o, c.unread);
    }
    return o;
}

bool MeshStore::decodeContacts(const std::string& blob, std::vector<MeshContact>& out) {
    out.clear();
    if (blob.size() < 8) return false;
    if (blob[0] != 'M' || blob[1] != 'C' || blob[2] != 'C' || blob[3] != 0x01) return false;
    size_t off = 4;
    uint32_t count = 0;
    if (!getU32(blob, off, count)) return false;
    if (count > 500) return false;
    for (uint32_t i = 0; i < count; ++i) {
        MeshContact c;
        if (!getBytes(blob, off, c.pubkey, PUBKEY_LEN)) return false;
        c.hasPubkey = true;
        if (off + 3 > blob.size()) return false;
        c.type = static_cast<uint8_t>(blob[off++]);
        c.flags = static_cast<uint8_t>(blob[off++]);
        c.outPathLen = static_cast<int8_t>(blob[off++]);
        if (!getBytes(blob, off, c.outPath, CONTACT_OUTPATH_LEN)) return false;
        if (off + 1 > blob.size()) return false;
        uint8_t nl = static_cast<uint8_t>(blob[off++]);
        if (nl > 32 || off + nl > blob.size()) return false;
        c.name.assign(blob.data() + off, nl);
        off += nl;
        uint32_t la = 0, lo = 0;
        if (!getU32(blob, off, c.lastAdvert)) return false;
        if (!getU32(blob, off, la)) return false;
        if (!getU32(blob, off, lo)) return false;
        c.lat = static_cast<double>(static_cast<int32_t>(la)) / 1e6;
        c.lon = static_cast<double>(static_cast<int32_t>(lo)) / 1e6;
        if (!getU32(blob, off, c.lastmod)) return false;
        if (off + 1 > blob.size()) return false;
        c.favourite = blob[off++] != 0;
        if (!getU32(blob, off, c.unread)) return false;
        char hex[13];
        snprintf(hex, sizeof(hex), "%02x%02x%02x%02x%02x%02x", c.pubkey[0], c.pubkey[1],
                 c.pubkey[2], c.pubkey[3], c.pubkey[4], c.pubkey[5]);
        c.prefixHex12 = hex;
        c.hops = (c.outPathLen < 0) ? 0 : static_cast<uint8_t>(c.outPathLen);
        c.valid = true;
        out.push_back(c);
    }
    return true;
}

std::string MeshStore::encodeThreads(const std::map<std::string, DMThread>& threads,
                                     size_t capPerThread) {
    std::string o;
    o += 'M';
    o += 'C';
    o += 'T';
    o.push_back(0x01);
    putU32(o, static_cast<uint32_t>(threads.size()));
    for (const auto& kv : threads) {
        const DMThread& th = kv.second;
        uint8_t pl = static_cast<uint8_t>(th.prefixHex12.size() > 12 ? 12 : th.prefixHex12.size());
        o.push_back(static_cast<char>(pl));
        o.append(th.prefixHex12.data(), pl);
        size_t n = th.msgs.size() > capPerThread ? capPerThread : th.msgs.size();
        size_t start = th.msgs.size() > n ? th.msgs.size() - n : 0;
        putU32(o, static_cast<uint32_t>(n));
        for (size_t i = start; i < th.msgs.size(); ++i) {
            const auto& m = th.msgs[i];
            o.push_back(static_cast<char>(m.pathLength));
            o.push_back(static_cast<char>(m.txtType));
            putU32(o, m.timestamp);
            o.push_back(m.hasSnr ? 0x01 : 0x00);
            uint32_t snrBits = 0;
            memcpy(&snrBits, &m.snrDb, sizeof(snrBits));
            putU32(o, snrBits);
            uint16_t tl = static_cast<uint16_t>(m.text.size() > 512 ? 512 : m.text.size());
            putU16(o, tl);
            o.append(m.text.data(), tl);
            uint8_t xl = static_cast<uint8_t>(m.pubkeyPrefix.size() > 12 ? 12 : m.pubkeyPrefix.size());
            o.push_back(static_cast<char>(xl));
            o.append(m.pubkeyPrefix.data(), xl);
            o.push_back(m.outgoing ? 0x01 : 0x00);
        }
    }
    return o;
}

bool MeshStore::decodeThreads(const std::string& blob, std::map<std::string, DMThread>& out) {
    out.clear();
    if (blob.size() < 8) return false;
    if (blob[0] != 'M' || blob[1] != 'C' || blob[2] != 'T' || blob[3] != 0x01) return false;
    size_t off = 4;
    uint32_t tcount = 0;
    if (!getU32(blob, off, tcount)) return false;
    if (tcount > 200) return false;
    for (uint32_t t = 0; t < tcount; ++t) {
        if (off + 1 > blob.size()) return false;
        uint8_t pl = static_cast<uint8_t>(blob[off++]);
        if (pl > 12 || off + pl > blob.size()) return false;
        std::string prefix(blob.data() + off, pl);
        off += pl;
        uint32_t n = 0;
        if (!getU32(blob, off, n)) return false;
        if (n > 500) return false;
        DMThread th;
        th.prefixHex12 = prefix;
        for (uint32_t i = 0; i < n; ++i) {
            if (off + 2 > blob.size()) return false;
            ContactMessage m;
            m.pathLength = static_cast<uint8_t>(blob[off++]);
            m.txtType = static_cast<TxtType>(blob[off++]);
            if (!getU32(blob, off, m.timestamp)) return false;
            if (off + 1 > blob.size()) return false;
            m.hasSnr = blob[off++] != 0;
            uint32_t snrBits = 0;
            if (!getU32(blob, off, snrBits)) return false;
            memcpy(&m.snrDb, &snrBits, sizeof(m.snrDb));
            uint16_t tl = 0;
            if (!getU16(blob, off, tl)) return false;
            if (off + tl > blob.size()) return false;
            m.text.assign(blob.data() + off, tl);
            off += tl;
            if (off + 1 > blob.size()) return false;
            uint8_t xl = static_cast<uint8_t>(blob[off++]);
            if (off + xl > blob.size()) return false;
            m.pubkeyPrefix.assign(blob.data() + off, xl);
            off += xl;
            if (off + 1 > blob.size()) return false;
            m.outgoing = blob[off++] != 0;
            th.msgs.push_back(m);
        }
        out[prefix] = th;
    }
    return true;
}

bool MeshStore::saveContacts(const std::vector<MeshContact>& contacts) {
    return saveAtomic(kContactsPath, encodeContacts(contacts));
}

bool MeshStore::loadContacts(std::vector<MeshContact>& out) {
    if (!cbdos::storage::fileExists(kContactsPath)) {
        out.clear();
        return true;  // sin archivo = agenda vacía, no error
    }
    return decodeContacts(cbdos::storage::readFile(kContactsPath), out);
}

bool MeshStore::saveChannels(const MeshChannel channels[CHANNEL_COUNT]) {
    std::string o;
    o += 'M';
    o += 'C';
    o += 'H';
    o.push_back(0x01);
    o.push_back(static_cast<char>(CHANNEL_COUNT));
    for (size_t i = 0; i < CHANNEL_COUNT; ++i) {
        o.push_back(static_cast<char>(channels[i].index));
        o.push_back(channels[i].known ? 0x01 : 0x00);
        o.push_back(static_cast<char>(channels[i].kind));
        uint8_t nl = static_cast<uint8_t>(channels[i].name.size() > 32 ? 32 : channels[i].name.size());
        o.push_back(static_cast<char>(nl));
        o.append(channels[i].name.data(), nl);
        o.append(reinterpret_cast<const char*>(channels[i].secret), CHANNEL_SECRET_LEN);
    }
    return saveAtomic(kChannelsPath, o);
}

bool MeshStore::loadChannels(MeshChannel channels[CHANNEL_COUNT]) {
    for (size_t i = 0; i < CHANNEL_COUNT; ++i) {
        channels[i] = MeshChannel();
        channels[i].index = static_cast<uint8_t>(i);
    }
    if (!cbdos::storage::fileExists(kChannelsPath)) return true;
    std::string b = cbdos::storage::readFile(kChannelsPath);
    if (b.size() < 5 || b[0] != 'M' || b[1] != 'C' || b[2] != 'H' || b[3] != 0x01) return false;
    size_t off = 4;
    uint8_t count = static_cast<uint8_t>(b[off++]);
    if (count != CHANNEL_COUNT) return false;
    for (size_t i = 0; i < CHANNEL_COUNT; ++i) {
        if (off + 4 > b.size()) return false;
        uint8_t idx = static_cast<uint8_t>(b[off++]);
        bool known = b[off++] != 0;
        ChannelKind kind = static_cast<ChannelKind>(b[off++]);
        uint8_t nl = static_cast<uint8_t>(b[off++]);
        if (nl > 32 || off + nl + CHANNEL_SECRET_LEN > b.size()) return false;
        if (idx >= CHANNEL_COUNT) return false;
        channels[idx].index = idx;
        channels[idx].known = known;
        channels[idx].kind = kind;
        channels[idx].name.assign(b.data() + off, nl);
        off += nl;
        memcpy(channels[idx].secret, b.data() + off, CHANNEL_SECRET_LEN);
        off += CHANNEL_SECRET_LEN;
    }
    return true;
}

bool MeshStore::saveSelf(const MeshSelfSettings& self) {
    std::string o;
    o += 'M';
    o += 'C';
    o += 'S';
    o.push_back(0x01);
    uint8_t nl = static_cast<uint8_t>(self.alias.size() > 64 ? 64 : self.alias.size());
    o.push_back(static_cast<char>(nl));
    o.append(self.alias.data(), nl);
    putI32(o, static_cast<int32_t>(self.lat * 1e6));
    putI32(o, static_cast<int32_t>(self.lon * 1e6));
    o.push_back(self.sharePosition ? 0x01 : 0x00);
    o.push_back(self.autoRetry ? 0x01 : 0x00);
    o.push_back(self.autoResetPath ? 0x01 : 0x00);
    o.push_back(static_cast<char>(self.nAcks));
    o.push_back(self.saveDrafts ? 0x01 : 0x00);
    o.push_back(self.showHops ? 0x01 : 0x00);
    return saveAtomic(kSelfPath, o);
}

bool MeshStore::loadSelf(MeshSelfSettings& out) {
    out = MeshSelfSettings();
    if (!cbdos::storage::fileExists(kSelfPath)) return true;
    std::string b = cbdos::storage::readFile(kSelfPath);
    if (b.size() < 6 || b[0] != 'M' || b[1] != 'C' || b[2] != 'S' || b[3] != 0x01) return false;
    size_t off = 4;
    uint8_t nl = static_cast<uint8_t>(b[off++]);
    if (nl > 64 || off + nl + 13 > b.size()) return false;
    out.alias.assign(b.data() + off, nl);
    off += nl;
    uint32_t la = 0, lo = 0;
    if (!getU32(b, off, la) || !getU32(b, off, lo)) return false;
    out.lat = static_cast<double>(static_cast<int32_t>(la)) / 1e6;
    out.lon = static_cast<double>(static_cast<int32_t>(lo)) / 1e6;
    out.sharePosition = b[off++] != 0;
    out.autoRetry = b[off++] != 0;
    out.autoResetPath = b[off++] != 0;
    out.nAcks = static_cast<uint8_t>(b[off++]);
    out.saveDrafts = b[off++] != 0;
    out.showHops = b[off++] != 0;
    return true;
}

bool MeshStore::saveThreads(const std::map<std::string, DMThread>& threads) {
    if (!saveAtomic(kThreadsPath, encodeThreads(threads, kFlashMsgsPerThread))) return false;
    // Tier 2 bulk solo si hay SD (historial largo).
    if (cbdos::storage::isSdMounted()) {
        std::string full = encodeThreads(threads, kSdMsgsPerThread);
        std::string tmp = std::string(kThreadsFullPath) + ".tmp";
        cbdos::storage::makeDir(kDirSd);
        if (cbdos::storage::writeFile(tmp.c_str(), full)) {
            cbdos::storage::copyFile(tmp.c_str(), kThreadsFullPath);
            cbdos::storage::deleteFile(tmp.c_str());
        }
    }
    return true;
}

bool MeshStore::loadThreads(std::map<std::string, DMThread>& out) {
    out.clear();
    // Tier 2 primero (más historial), fallback a Tier 1.
    if (cbdos::storage::fileExists(kThreadsFullPath)) {
        if (decodeThreads(cbdos::storage::readFile(kThreadsFullPath), out)) return true;
    }
    if (!cbdos::storage::fileExists(kThreadsPath)) return true;
    return decodeThreads(cbdos::storage::readFile(kThreadsPath), out);
}

}  // namespace meshcore
}  // namespace cbdos
