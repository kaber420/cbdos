#pragma once

// Constructor de tramas de gestion 802.11 de desautenticacion (subtipo 0x0C) y
// desasociacion (subtipo 0x0A) de la suite Wi-Fi Strike.
//
// hermano de BeaconEmitter y con la misma frontera: codigo 100% agnostico, sin
// esp_wifi.h ni FreeRTOS. Solo escribe los bytes de una trama de gestion en un
// buffer del llamante; quien decide cuando, contra quien y con que secuencia es
// WifiStrikeService.
//
// Estas tramas son las que evaluan la resiliencia de una red ante la perdida de
// gestion: una AP con WPA2 sin 802.11w (Management Frame Protection) obeyece el
// deauth y expulsa al cliente, mientras que una AP WPA3 con MFP cifra las tramas
// de gestion y las descarta. La diferencia se ve en el aire comparando el bit
// "Protected Frame" y el resultado en el cliente, no en un contador inventado.
//
// Diseno de la trama (802.11-2016, clausula 9.4.3.5 deauthentication y
// 9.4.3.6 disassociation). Ambas comparten el mismo cuerpo de 2 bytes, el codigo
// de razon, y no llevan parameters ni IEs:
//
//   off  tam  campo
//   ---  ---  ----------------------------------------------------------
//    0    1   Frame Control, byte 0: Protocol Version 0 | Type 0 (gestion)
//                        | Subtype (0x0C deauth / 0x0A disassoc)
//    1    1   Frame Control, byte 1: To/From DS | More Fragments | Retry |
//                        Power Management | bit Protected Frame (802.11w)
//    2    2   Duration / ID (0x0000: no hay duracion en tramas de gestion)
//    4    6   Addr1 (destino: MAC del cliente, BSSID de la AP o broadcast)
//   10    6   Addr2 (origen: BSSID de la AP o MAC de la STA que la suplanta)
//   16    6   Addr3 (BSSID delinear al que pertenece la trama)
//   22    2   Sequence Control (12 bits de secuencia + 4 bits de fragmento)
//   24    2   Reason Code
//
// Los tres campos Addr son el alma del ataque bidireccional y por eso se
// dejan explicitos en Params en vez de derivarlos: es el servicio quien sabe si
// la trama la emite la AP hacia sus clientes (Addr2 = BSSID) o una STA
// suplantada hacia la AP (Addr2 = MAC localmente administrada).

#include "BeaconEmitter.hpp"

#include <cstddef>
#include <cstdint>

namespace cbdos {
namespace security {

// Definido al final de este header: solo necesita el tamano del buffer, pero
// los metodos del generador lo devuelven por valor.
struct MgmtFrame;

/**
 * @brief Generador de tramas de gestion de desasociacion 802.11
 *
 * Todos los metodos son staticos y sin estado: el llamante decide el momento de
 * la inyeccion, la semilla del PRNG y el sentido de la suplantacion, de modo que
 * el modulo es trivialmente validable en host con los bytes esperados.
 */
class MgmtEmitter {
public:
    /** Tamano exacto de la trama: cabecera MAC de 24 bytes + codigo de razon. */
    static constexpr size_t kMaxFrameSize = 26;

    /** Subtipos de gestion que la suite inyecta (byte 0 del Frame Control). */
    static constexpr uint8_t kSubtypeDeauth = 0x0C;
    static constexpr uint8_t kSubtypeDisassoc = 0x0A;

    /**
     * @brief Razon por defecto: Class 3 frame received from nonassociated STA
     *
     * Es el codigo queGhostESP y las herramientas de auditoria usan por defecto
     * (0x0007) porque es el que un cliente WPA2 legitimo acepta sin discusion:
     * aparenta que la trama llega de una STA que nunca completo la asociacion.
     */
    static constexpr uint16_t kReasonClass3FromNonassociated = 0x0007;

    /** Sequence Control inicial, el mismo valor por defecto que los beacons. */
    static constexpr uint16_t kInitialSeqControl = 0x6CC0;

    /**
     * @brief Bit Protected Frame del Frame Control (byte 1, bit 2 -> 0x04)
     *
     * Es el bit 18 del campo Frame Control de 16 bits (802.11-2016, Tabla 10-3):
     * a 1 significa que el cuerpo de la trama va cifrado con CCMP, que es
     * justamente lo que exige 802.11w (Management Frame Protection). Marcarlo
     * permite comprobar si la AP de destino filtra la trama por tener MFP activo.
     */
    static constexpr uint8_t kFcProtectedFrame = 0x04;

    /** Tipo de trama: 0 = Management (bits 2-3 del byte 0 del Frame Control). */
    static constexpr uint8_t kTypeManagement = 0x00;

    /** PRNG portable compartido con BeaconEmitter: misma semantica de semilla. */
    using Rng = BeaconEmitter::Rng;

    /**
     * @brief Direcciones y cuerpo de una trama de gestion
     *
     * Los tres campos Addr se dejan a criterio del llamante porque cada sentido
     * de la inyeccion los rellena de forma distinta:
     *
     *  - AP -> STA (la AP expulsa a sus clientes): Addr1 = ff:ff:ff:ff:ff:ff
     *    (broadcast: no conocemos las MAC de los clientes reales, y la trama
     *    valida para todos), Addr2 = Addr3 = BSSID de la AP.
     *  - STA -> AP (una STA suplantada acusa a la AP): Addr1 = BSSID de la AP,
     *    Addr2 = MAC localmente administrada generada al vuelo, Addr3 = BSSID.
     */
    struct Params {
        uint8_t da[wifi::WIFI_MAC_LEN] = {0};    // Addr1: destino
        uint8_t sa[wifi::WIFI_MAC_LEN] = {0};    // Addr2: origen
        uint8_t bssid[wifi::WIFI_MAC_LEN] = {0}; // Addr3: BSSID de referencia
        /** Sequence Control completo; el fragmento va en los 4 bits bajos. */
        uint16_t seqControl = kInitialSeqControl;
        /** Codigo de razon de 16 bits (Tabla 10-18 del estandar). */
        uint16_t reasonCode = kReasonClass3FromNonassociated;
        /** Marca la trama como cifrada (802.11w) para evaluar el filtrado MFP. */
        bool protectedFrame = false;
    };

    // --- Construccion de la trama -----------------------------------------

    /**
     * @brief Construye la trama con el subtipo indicado
     *
     * Un subtipo distinto de 0x0C y 0x0A devuelve una trama invalida: emitir un
     * subtipo de gestion arbitrario no es una prueba de nada y si un analizador
     * lo viera creeria que el dispositivo emite tramas que no emite.
     */
    static MgmtFrame build(uint8_t subtype, const Params& params);

    /** Deauthentication (0x0C). */
    static MgmtFrame buildDeauth(const Params& params);

    /** Disassociation (0x0A). */
    static MgmtFrame buildDisassoc(const Params& params);

    // --- Composicion de direcciones ----------------------------------------

    /** Escribe ff:ff:ff:ff:ff:ff en `mac`. */
    static void fillBroadcast(uint8_t mac[wifi::WIFI_MAC_LEN]);

    /**
     * @brief Escribe una MAC localmente administrada y unicast
     *
     * `mac[0] &= 0xFE` (bit multicast a 0) y `mac[0] |= 0x02` (bit de
     * administracion local a 1), igual que en BeaconEmitter. Es la identidad de
     * la STA suplantada en el sentido STA -> AP: cada trama pretende venir de una
     * estacion distinta, que es lo que hace inutil el filtrado por origen.
     */
    static void fillRandomStaMac(uint8_t mac[wifi::WIFI_MAC_LEN], Rng& rng);

    /**
     * @brief Sequence Control pseudoaleatorio (12 bits de secuencia)
     *
     * Una secuencia incremental es una firma trivial para un filtro: GhostESP y
     * Bruce la randomizan por el mismo motivo. El numero de fragmento se deja a
     * 0 porque estas tramas nunca se fragmentan.
     */
    static uint16_t randomSequenceControl(Rng& rng);

    // --- Utilidades ---------------------------------------------------------

    /** Tamano de la trama, sin construirla. */
    static size_t frameSize();

    /** ¿El subtipo corresponde a una trama que la suite sabe construir? */
    static bool isSubtypeValid(uint8_t subtype);

    /** Byte 1 del Frame Control con el bit Protected Frame aplicado. */
    static uint8_t frameControlByte1(bool protectedFrame);
};

/**
 * @brief Trama de gestion de desasociacion construida
 *
 * Se devuelve por valor para que el llamante no gestione buffers parciales: si
 * `valid` es false el servicio la contabiliza como fallo, nunca como una trama
 * de gestion inyectada.
 */
struct MgmtFrame {
    uint8_t data[MgmtEmitter::kMaxFrameSize] = {0};
    size_t len = 0;
    bool valid = false;
};

} // namespace security
} // namespace cbdos