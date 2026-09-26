#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <vector>

namespace cbdos {
namespace usb {

enum class DeviceClass {
    Unknown,
    CdcAcm,         // Serial CDC estándar
    VendorSpecific, // ESP32 USB-Serial-JTAG / CP210x / CH340
    MassStorage,    // Memorias USB
    Hid             // Teclados, lectores de código de barras
};

enum class DeviceRole {
    Unassigned,               // Recién conectado, sin driver asignado
    FieldTarget,              // Microcontrolador listo para programar con Flasher
    PhysicalNetworkInterface, // Interfaz física de red (L1/L2) para NetworkInterfaceManager
    SerialTerminal,           // Dispositivo serial para consola o GPS/TNC
    StorageDrive,             // Unidad de disco montada
    InputPeripheral           // Dispositivo de entrada
};

struct UsbDeviceInfo {
    uint16_t vid = 0;
    uint16_t pid = 0;
    DeviceClass devClass = DeviceClass::Unknown;
    DeviceRole role = DeviceRole::Unassigned;
    char manufacturer[32] = {0};
    char product[32] = {0};
    char serialNumber[32] = {0};
    void* handle = nullptr;
    bool isConnected = false;
};

class IUsbDriver {
public:
    virtual ~IUsbDriver() = default;
    virtual const char* getDriverName() const = 0;
    virtual uint8_t getPriority() const { return 10; }
    virtual bool match(const UsbDeviceInfo& dev) = 0;
    virtual bool onAttach(UsbDeviceInfo& dev) = 0;
    virtual void onDetach(const UsbDeviceInfo& dev) = 0;
};

typedef void (*UsbDeviceEventCallback)(const UsbDeviceInfo& dev, bool connected, void* user_ctx);

class UsbDeviceManager {
public:
    static UsbDeviceManager& getInstance();

    bool init();
    void deinit();

    bool registerDriver(IUsbDriver* driver);
    bool unregisterDriver(IUsbDriver* driver);

    bool isDeviceConnected() const;
    const UsbDeviceInfo* getActiveDevice() const;

    void registerEventCallback(UsbDeviceEventCallback cb, void* user_ctx);

    // Métodos internos llamados por los callbacks nativos de ESP-IDF
    void handleDeviceConnected(uint16_t vid, uint16_t pid, uint8_t dev_class);
    void handleDeviceDisconnected();

private:
    UsbDeviceManager();
    ~UsbDeviceManager();
    UsbDeviceManager(const UsbDeviceManager&) = delete;
    UsbDeviceManager& operator=(const UsbDeviceManager&) = delete;

    bool m_initialized = false;
    UsbDeviceInfo m_activeDevice;
    IUsbDriver* m_activeDriver = nullptr;
    std::vector<IUsbDriver*> m_drivers;
    UsbDeviceEventCallback m_eventCb = nullptr;
    void* m_eventUserCtx = nullptr;
};

} // namespace usb
} // namespace cbdos
