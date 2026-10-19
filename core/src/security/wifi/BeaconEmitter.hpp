#pragma once

// Constructor de tramas beacon 802.11 de la suite Wi-Fi Strike.
//
// Codigo 100% agnostico: no conoce esp_wifi.h, ni el HAL de radio, ni
// FreeRTOS. Su unico trabajo es escribir los bytes de una trama de gestion
// subtipo 0x08 (Beacon) en un buffer del llamante, igual que
// BlePayloadRegistry hace con los payloads publicitarios BLE. Esa separacion
// permite validar los bytes en host y garantiza que el HAL solo tiene que
// llamar a esp_wifi_80211_tx() con lo que sale de aqui.
//
// Diseno de la trama (802.11-2016, clausula 9.4.1.10):
//
//   off  tam  campo
//   ---  ---  ----------------------------------------------------------
//    0    2   Frame Control (0x80 0x00 = Management/Beacon, version 0)
//    2    2   Duration / ID
//    4    6   Addr1 (destino: broadcast ff:ff:ff:ff:ff:ff)
//   10    6   Addr2 (origen: BSSID de la trama)
//   16    6   Addr3 (BSSID)
//   22    2   Sequence Control (12 bits seq + 4 bits fragmento)
//   24    8   Timestamp
//   32    2   Beacon Interval (100 TU = 102.4 ms por defecto)
//   34    2   Capability Information
//   36   2+n  SSID element (id 0x00, longitud, bytes)
//   38+n 10  Supported Rates (id 0x01, 8 Rates)
//   48+n 3   DS Parameter Set (id 0x03, longitud 1, canal)
//   51+n 14  HE Capabilities, opcional (vendor specific id 0xFF)
//
// Los bytes por defecto son los de la referencia GhostESP
// (references/ghostesp/main/attacks/wifi/beacon_spam.c, beacon_spam_broadcast),
// con dos correcciones deliberadas documentadas en el .cpp: el octeto de
// longitud del bloque HE Capabilities ahora coincide con su cuerpo, y los SSID
// se truncan a los 32 bytes que fija el estandar.

#include "cbdos/wifi_strike.hpp"
#include "cbdos/wifi_types.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace cbdos {
namespace security {

// Definido al final de este header: solo necesita el tamano del buffer, pero
// los metodos del generador lo devuelven por valor.
struct BeaconFrame;

/**
 * @brief Generador de tramas beacon 802.11
 *
 * Todos los metodos son staticos y sin estado: el llamante decide cuando emitir
 * y con que semilla, lo que hace el modulo trivialmente testeable en host.
 */
class BeaconEmitter {
public:
    /** Tope del buffer de trama. Coincide con BEACON_FRAME_MAX de GhostESP. */
    static constexpr size_t kMaxFrameSize = 104;

    /** Longitud maxima del campo SSID (id 0x00, octeto de longitud). */
    static constexpr size_t kMaxSsidLength = wifi::STRIKE_SSID_MAX; // 32

    // --- Capability Information (16 bits, clausula 9.4.1.4) ----------------
    static constexpr uint16_t kCapEss = 0x0001;
    static constexpr uint16_t kCapIbss = 0x0002;
    static constexpr uint16_t kCapPrivacy = 0x0010;
    static constexpr uint16_t kCapShortPreamble = 0x0040;
    /** Bit reservado que fija la referencia GhostESP en su trama (0x0411). */
    static constexpr uint16_t kCapReferenceReserved = 0x0400;
    /** Valor por defecto, identico al de GhostESP: ESS | Privacy | 0x0400. */
    static constexpr uint16_t kCapDefault =
        kCapEss | kCapPrivacy | kCapReferenceReserved;

    /** Beacon Interval por defecto: 100 TU (102.4 ms). */
    static constexpr uint16_t kDefaultBeaconInterval = 100;

    /**
     * @brief Generador pseudoaleatorio portable (xorshift32)
     *
     * El core no puede llamar a esp_random() porque eso lo ataria al IDF. La
     * semilla llega desde fuera: el servicio la toma de la entropia real que
     * entrega IWifiStrikeRadio::getRandom() al arrancar la sesion, de modo que
     * los SSIDs y BSSIDs generados difieren entre arranques y no se inventan
     * datos. A partir de ahi la generacion es autonoma y reproducible en host.
     */
    struct Rng {
        uint32_t state;

        explicit Rng(uint32_t seed = 0x12345678u)
            : state(seed != 0u ? seed : 0x12345678u) {}

        /** Siguiente valor de 32 bits. */
        uint32_t next() {
            uint32_t x = state;
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            state = x;
            return x;
        }

        /** Valor en [0, bound). Devuelve 0 si bound es 0. */
        uint32_t below(uint32_t bound) {
            return bound == 0u ? 0u : next() % bound;
        }
    };

    /**
     * @brief Parametros comunes a cualquier beacon
     */
    struct Params {
        /** BSSID de origen y destino logico. En modo rotativo es una MAC
         *  localmente administrada generada por fillRandomMac(). */
        uint8_t bssid[wifi::WIFI_MAC_LEN] = {0};
        /** Canal announced en el DS Parameter Set. */
        uint8_t channel = 1;
        uint16_t capabilityInfo = kCapDefault;
        uint16_t beaconInterval = kDefaultBeaconInterval;
        /** Reloj TSF de la AP. 0 = sin estampar (como GhostESP). */
        uint32_t timestamp = 0;
        /** Sequence Control completo; el fragmento va en los 4 bits bajos. */
        uint16_t seqControl = 0x6CC0;
        bool includeHeCapabilities = true;
    };

    // --- Construccion de la trama -----------------------------------------

    /**
     * @brief Construye un beacon con un SSID explicito
     *
     * Cubre los modos SingleSsid, Rickroll (una linea por trama) y CustomList
     * (una entrada por trama). `ssidLength` se trunca a 32 bytes; ssid == nullptr
     * solo es valido con longitud 0 (beacon de AP oculta).
     */
    static BeaconFrame build(const Params& params, const char* ssid,
                             size_t ssidLength);

    /** Sobrecarga de build() para std::string (trunca a 32 bytes). */
    static BeaconFrame build(const Params& params, const std::string& ssid);

    /**
     * @brief Beacon con SSID alfanumerico generado al vuelo
     *
     * Modo Random de GhostESP. Devuelve una trama nueva en cada llamada porque
     * el azar es precisamente lo que distingue una baliza de la siguiente.
     */
    static BeaconFrame buildRandomSsid(const Params& params, uint8_t length,
                                       Rng& rng);

    /**
     * @brief Beacon que clona una AP descubierta
     *
     * Modo CloneAccessPoints: copia BSSID, SSID y canal reales, y calcula el
     * Capability Information a partir del cifrado announced. Es la via para
     * Evil Twin con la identidad exacta de una red legitima.
     */
    static BeaconFrame buildCloneAp(const radio::WifiApRecord& ap,
                                    bool includeHeCapabilities = true);

    // --- Utilidades de generacion -----------------------------------------

    /** Escribe un SSID alfanumerico de `length` bytes + terminador en `out`. */
    static void fillRandomSsid(char* out, size_t capacity, size_t length,
                               Rng& rng);

    /**
     * @brief Escribe una MAC localmente administrada y unicast
     *
     * mac[0] &= 0xFE (bit multicast a 0) y mac[0] |= 0x02 (bit de
     * administracion local a 1), igual que GhostESP: es lo que hace que cada
     * baliza parezca una AP distinta y evita el filtrado por BSSID repetida.
     */
    static void fillRandomMac(uint8_t mac[wifi::WIFI_MAC_LEN], Rng& rng);

    /** Traduce el modo de autenticacion de una AP a Capability Information. */
    static uint16_t capabilityForAuth(radio::WifiAuthMode auth);

    /** Tamano exacto de la trama resultante, sin construirla. */
    static size_t frameSizeFor(size_t ssidLength, bool includeHeCapabilities);

    /** ¿El SSID cabe sin truncar? */
    static bool isSsidLengthValid(size_t ssidLength);

    // --- Rickroll ---------------------------------------------------------

    /** Numero de versos del coro. */
    static size_t rickrollLineCount();

    /** Verso `index` (modulo el numero de versos); nullptr si no hay coro. */
    static const char* rickrollLine(uint8_t index);

    /** Siguiente indice del coro, con wraparound. */
    static uint8_t nextRickrollIndex(uint8_t current);

private:
    BeaconEmitter() = delete;
};

/**
 * @brief Trama beacon construida
 *
 * Se devuelve por valor para que el llamante no tenga que gestionar buffers
 * parciales: si `valid` es false no hay trama que emitir y el servicio la
 * contabiliza como fallo, nunca como una baliza inyectada.
 */
struct BeaconFrame {
    uint8_t data[BeaconEmitter::kMaxFrameSize] = {0};
    size_t len = 0;
    bool valid = false;
};

} // namespace security
} // namespace cbdos
