#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace cbdos {
namespace network {

// Información detallada de cada cliente CPE conectado al AP
struct AirOSStation {
    std::string mac;
    std::string ip;
    std::string name;
    int signalDbm = 0;
    int signalChain0 = 0;       // Cadena 0 en dBm (chainrssi[0])
    int signalChain1 = 0;       // Cadena 1 en dBm (chainrssi[1])
    int chainDelta = 0;         // Desbalance |Chain0 - Chain1|
    int noiseFloor = -95;
    
    // Tasas y Capacidad
    int txRateKbps = 0;
    int rxRateKbps = 0;
    int dlCapacityMbps = 0;     // airmax.dl_capacity / 1000
    int ulCapacityMbps = 0;     // airmax.ul_capacity / 1000
    int txThroughputKbps = 0;   // remote.tx_throughput
    int rxThroughputKbps = 0;   // remote.rx_throughput
    
    // Cable LAN PoE local del CPE
    bool ethPlugged = false;    // remote.ethlist[0].plugged
    int ethSpeedMbps = 0;       // remote.ethlist[0].speed (10, 100, 1000)
    bool cableFault = false;    // Alerta de conector o cable si es 10M o unplugged
    
    int distanceM = 0;
    int uptimeSec = 0;
    int ccq = 0;
};

// Telemetría completa del Sector / AP
struct AirOSTelemetry {
    bool success = false;
    std::string errorMessage;
    
    // Identidad del radio
    std::string hostname = "Inaccesible";
    std::string model = "-";
    std::string firmware = "-";
    int uptimeSec = 0;
    
    // Parámetros RF
    std::string ssid = "-";
    int frequency = 0;          // MHz
    int channelWidth = 0;       // chanbw MHz
    int noiseFloor = -95;       // dBm
    
    // Ocupación de Espectro (Airtime)
    int airtimeTotal = 0;       // %
    int airtimeTx = 0;          // %
    int airtimeRx = 0;          // %
    
    int gpsSats = 0;            // Satélites GPS
    std::vector<AirOSStation> stations;
};

class AirOSClient {
public:
    AirOSClient();
    ~AirOSClient() = default;

    // Configura credenciales y host de destino
    void setTarget(const std::string& host, const std::string& user, const std::string& pass, uint16_t port = 443);

    // Consulta telemetría síncrona (debe llamarse dentro de una tarea worker de FreeRTOS)
    AirOSTelemetry fetchTelemetry(uint32_t timeoutMs = 4500);

    const std::string& getHost() const { return m_host; }
    const std::string& getUser() const { return m_user; }
    uint16_t getPort() const { return m_port; }

private:
    std::string m_host = "";
    std::string m_user = "ubnt";
    std::string m_pass = "";
    uint16_t m_port = 443;
    std::string m_cookie;
    std::string m_csrfToken;

    bool authenticate(uint32_t timeoutMs);
};

} // namespace network
} // namespace cbdos
