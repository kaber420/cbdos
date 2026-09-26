#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>
#include <string>
#include "cbdos/serial.hpp"

namespace cbdos {
namespace usb {

// Códigos de resultado explícitos para I/O robusto (vital para Flasher a 921600 bps)
enum class UsbIoResult {
    Ok,
    Timeout,
    Disconnected,
    BufferOverflow,
    HardwareError
};

enum class DeviceClass : uint8_t {
    Unknown,
    CdcAcm,         // Serial CDC estándar
    VendorSpecific, // ESP32 USB-Serial-JTAG / CP210x / CH340 / FTDI
    MassStorage,    // Pendrives / Discos USB MSC
    Hid             // Teclados, ratones, lectores barcode
};

enum class DeviceRole : uint8_t {
    Unassigned,               // Conectado, sin arbitrar
    FieldTarget,              // Microcontrolador externo para programar
    PhysicalNetworkInterface, // Módem de radio / LoRa / SLIP
    SerialTerminal,           // Consola interactiva UART/CDC
    StorageDrive,             // Almacenamiento masivo montado en /usb/
    InputPeripheral           // Periférico de entrada
};

// Causa del evento de conexión/desconexión
enum class UsbEventCause : uint8_t {
    PhysicalHotPlug,          // Conexión/desconexión física de cable
    TargetResetInduced,       // Desconexión temporal provocada por secuencia DTR/RTS
    DriverError               // Fallo en bus o protocolo
};

struct UsbDeviceInfo {
    uint16_t vid = 0;
    uint16_t pid = 0;
    DeviceClass devClass = DeviceClass::Unknown;
    DeviceRole role = DeviceRole::Unassigned;
    char manufacturer[32] = {0};
    char product[32] = {0};
    char serialNumber[32] = {0};
    bool isConnected = false;
};

// Arbitraje y desalojo prioritario
enum class CdcOwner : uint8_t {
    None,       // Libre
    Terminal,   // Consumo por terminal interactiva
    Flasher     // Consumo exclusivo y prioritario por el programador
};

// Interfaz unificada del Canal CDC (Hereda de ISerialPort para evitar duplicar el HAL Serial)
class IUsbCdcChannel : public cbdos::serial::ISerialPort {
public:
    virtual ~IUsbCdcChannel() = default;

    // Desocultar sobrecargas de ISerialPort
    using cbdos::serial::ISerialPort::read;
    using cbdos::serial::ISerialPort::write;

    // Arbitraje con soporte de preemption (Flasher puede desalojar a Terminal con aviso)
    virtual bool tryAcquire(CdcOwner owner, uint32_t timeoutMs = 1000) = 0;
    virtual void release(CdcOwner owner) = 0;
    virtual CdcOwner getCurrentOwner() const = 0;

    // I/O de alta velocidad con discriminación de errores
    virtual UsbIoResult read(uint8_t* buffer, size_t maxLen, size_t& outLen, uint32_t timeoutMs) = 0;
    virtual UsbIoResult write(const uint8_t* data, size_t len, size_t& outWritten, uint32_t timeoutMs) = 0;
    virtual void purge() = 0;

    // Configuración completa de línea
    virtual bool setLineCoding(uint32_t baud, uint8_t dataBits = 8, uint8_t parity = 0, uint8_t stopBits = 1) = 0;

    // Secuencia de Reset adaptativa según VID/PID (4 pasos para JTAG nativo, 2 para puentes USB-UART)
    virtual bool resetTarget(bool enterBootloader) = 0;
};

// Interfaz de Plugin de Driver USB (para CDC, MSC, HID Host)
class IUsbDriver {
public:
    virtual ~IUsbDriver() = default;
    virtual const char* getDriverName() const = 0;
    virtual uint8_t getPriority() const { return 10; }
    virtual bool match(const UsbDeviceInfo& dev) = 0;
    virtual bool onAttach(const UsbDeviceInfo& dev) = 0;
    virtual void onDetach(const UsbDeviceInfo& dev) = 0;
};

// Evento de hot-plug con causa explícita
using UsbEventCallback = std::function<void(const UsbDeviceInfo& dev, bool connected, UsbEventCause cause)>;

// Interfaz Central del Backend USB Host
class IUsbHostBackend {
public:
    virtual ~IUsbHostBackend() = default;

    virtual bool init() = 0;
    virtual void deinit() = 0;
    virtual bool supportsHost() const = 0; // Consulta explícita de hardware (P4 true, S3 false)

    virtual bool registerDriver(IUsbDriver* driver) = 0;
    virtual bool unregisterDriver(IUsbDriver* driver) = 0;

    virtual bool isDeviceConnected() const = 0;
    virtual bool getActiveDevice(UsbDeviceInfo& outInfo) const = 0; // Copia segura, no puntero dangling

    virtual void registerEventCallback(UsbEventCallback callback) = 0;
    virtual IUsbCdcChannel* getCdcChannel() = 0;
};

// Inyección e instanciación global
IUsbHostBackend* getUsbHostBackend();
void setUsbHostBackend(IUsbHostBackend* backend);

} // namespace usb
} // namespace cbdos
