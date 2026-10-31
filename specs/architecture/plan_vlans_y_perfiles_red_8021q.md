# Especificación y Plan de Arquitectura: Soporte de VLANs (IEEE 802.1Q) y Perfiles de Segmentación de Red en CBDos

**Documento:** `specs/architecture/plan_vlans_y_perfiles_red_8021q.md`  
**Estado:** Planificado (Pendiente de implementación)  
**Targets:** ESP32-P4 (JC4880P443C / Futura placa con RJ45 EMAC) y ESP32-S3 (JC3248W535)  
**Módulos Afectados:** `core/network`, `core/system/ConfigManager`, `bsp/*/hal_network_*`, `NetworkManagerView`, `OmniWISP`, `NeighborDiscoveryService`

---

## 1. Propósito y Caso de Uso en Campo

En despliegues de telecomunicaciones (WISP / ISP / Nodos de Torre y Redes Empresariales), el tráfico se encuentra estrictamente segmentado mediante **VLANs (IEEE 802.1Q)**:
* **VLAN de Administración (Management VLAN):** Segmento aislado donde residen las interfaces de control de radios sectoriales (Ubiquiti airMAX/LTU, MikroTik, Mimosa), switches y routers de borde.
* **VLAN de Clientes (Access / CPE VLAN):** Segmento por donde cursa el tráfico de los suscriptores (PPPoE, DHCP o IP estática de cliente) con políticas de firewall que bloquean el acceso a la administración.
* **Subredes de Rescate / Adopción (Factory Default):** Segmentos estáticos locales (`192.168.1.0/24` para Ubiquiti, `192.168.88.0/24` para MikroTik) para aprovisionar equipos nuevos o reseteados en campo.

Este documento establece el plan arquitectónico para que **CBDos** opere sobre múltiples VLANs tanto **hoy mediante Wi-Fi** como **próximamente sobre hardware con puerto físico RJ45 (Ethernet)**, sin necesidad de reescribir la lógica de negocio ni las vistas de usuario.

---

## 2. Arquitectura en Dos Etapas (Agnóstica al Medio Físico)

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                     APLICACIONES DE CAMPO EN CORE (C++)                      │
│   OmniWISP (AirOS)  │  Cliente SSH  │  LAN Recon  │  Neighbor Discovery      │
└──────────────────────────────────────┬───────────────────────────────────────┘
                                       │
┌──────────────────────────────────────▼───────────────────────────────────────┐
│                 GESTOR DE PERFILES DE RED Y VLAN (ConfigManager)             │
│  Perfiles Preconfigurados:                                                   │
│  • [VLAN Admin]    (VID: 99, IP Estática / DHCP, SSID Admin o Trunk RJ45)    │
│  • [VLAN Clientes] (VID: 20, DHCP, SSID Clientes o Trunk RJ45)               │
│  • [Rescue UBNT]   (VID: 0/Untagged, IP: 192.168.1.254/24)                   │
│  • [Rescue MT]     (VID: 0/Untagged, IP: 192.168.88.254/24)                  │
└──────────────────────────────────────┬───────────────────────────────────────┘
                                       │ Contrato INetworkAdapter
                     ┌─────────────────┴─────────────────┐
                     ▼                                   ▼
┌─────────────────────────────────────────┐ ┌──────────────────────────────────┐
│   FASE 1: MEDIO WI-FI (Actual)          │ │   FASE 2: PUERTO RJ45 (Futuro)   │
│  • Mapeo VLAN <-> SSID + IP/Subred      │ │  • Ethernet RMII (P4) / SPI / USB│
│  • Etiquetado 802.1Q en LwIP opcional   │ │  • Trunk 802.1Q Nativo (0x8100)  │
│    (para puentes WDS / MikroTik mAP)    │ │  • Sub-interfaces esp_netif      │
└─────────────────────────────────────────┘ └──────────────────────────────────┘
```

---

## 3. Modelo de Datos Propuesto (`core/include/cbdos/config_manager.hpp`)

Se extenderá el sistema de configuración para soportar una lista de perfiles de red/VLAN (`NetworkVlanProfile`) en lugar de una única estructura Wi-Fi plana:

```cpp
namespace cbdos {
namespace network {

enum class VlanTransportMode : uint8_t {
    Untagged = 0,       // Tráfico sin etiqueta 802.1Q (Nativo / Basado en SSID)
    Tagged8021Q = 1     // Inyección de etiqueta IEEE 802.1Q (0x8100 + VID 1..4094)
};

struct NetworkVlanProfile {
    std::string id;              // ej: "mgmt", "clients", "ubnt_rescue"
    std::string name;            // ej: "VLAN 99 - Administración"
    uint16_t vlanId = 0;         // 0 = Untagged, 1..4094 = IEEE 802.1Q VID
    VlanTransportMode mode = VlanTransportMode::Untagged;

    // Credenciales inalámbricas (Fase 1 - Wi-Fi)
    std::string ssid;            // SSID asociado a esta VLAN (si aplica)
    std::string password;

    // Configuración IPv4 (Común para Wi-Fi y RJ45)
    bool useStaticIp = false;
    std::string staticIp;        // ej: "10.200.99.254" o "192.168.1.254"
    std::string subnet = "255.255.255.0";
    std::string gateway;
    std::string dns1;
};

} // namespace network
} // namespace cbdos
```

---

## 4. Plan de Implementación por Fases

### Fase 1: Conmutación Rápida de VLANs y Segmentos sobre Wi-Fi (Hardware Actual)
1. **Perfiles de Red Persistentes (`ConfigManager`):**
   * Almacenar en `/sdcard/config/vlan_profiles.msgpack` (con respaldo básico en NVS) la colección de perfiles de red del operador.
   * Incluir por defecto 4 plantillas listas para usar en campo:
     1. **`VLAN Admin`**: Para gestión de infraestructura.
     2. **`VLAN Clientes`**: Con DHCP para pruebas de navegación y throughput de abonados.
     3. **`Rescate Ubiquiti`**: IP estática `192.168.1.254/24` (apuntando a `192.168.1.20`).
     4. **`Rescate MikroTik`**: IP estática `192.168.88.254/24` (apuntando a `192.168.88.1`).
2. **Selector Rápido "1-Tap VLAN Switcher" en UI:**
   * En `NetworkManagerView` y en la barra de herramientas de `OmniwispMainView` y `LanReconView`, incorporar un selector desplegable o píldoras táctiles para alternar entre **VLAN Admin**, **VLAN Clientes** y **Modo Rescate** con un solo toque, aplicando al instante el cambio de SSID/IP en `INetworkAdapter`.
3. **Soporte 802.1Q en LwIP sobre Wi-Fi (para APs en Bridge/Trunk transparente):**
   * Cuando el técnico utilice un router portátil en bolsillo (ej. MikroTik mAP lite) configurado en bridge transparente pasando tags 802.1Q por aire, habilitar `ETHARP_SUPPORT_VLAN=1` en LwIP para etiquetar tramas salientes con el `vlanId` activo.

---

### Fase 2: Soporte Nativo de Troncales IEEE 802.1Q por Puerto RJ45 (Próxima Placa)
Cuando se incorpore la placa con puerto físico **RJ45** (mediante el controlador **EMAC RMII interno del ESP32-P4**, chip PHY externo como IP101/LAN8720/RTL8201, o controlador Ethernet SPI/USB):

1. **Driver HAL Ethernet (`hal_eth_p4.cpp`):**
   * Inicialización del bus Ethernet mediante `esp_eth_driver_install()` y vinculación con `esp_netif`.
   * Registro automático del puerto RJ45 como slot dedicado en `NetworkInterfaceManager`.
2. **Etiquetado y Filtrado 802.1Q en LwIP (`ETHARP_SUPPORT_VLAN`):**
   * Configurar los ganchos de LwIP (`LWIP_HOOK_VLAN_SET` y `LWIP_HOOK_VLAN_CHECK`) en el BSP:
     * **TX (Salida):** Todo paquete enviado desde el perfil activo con `vlanId > 0` recibe la cabecera IEEE 802.1Q (`TPID = 0x8100`, `VID = vlanId`).
     * **RX (Entrada):** Acepta tramas correspondientes al `vlanId` activo y permite múltiples sub-interfaces virtuales (`esp_netif` para Admin + `esp_netif` para Clientes simultáneamente si se requiere Dual-Homing).
3. **Inspector de VLANs en Descubrimiento L2 (`NeighborDiscoveryService`):**
   * Ampliar `NeighborDevice` en `core/include/cbdos/neighbor_discovery.hpp` añadiendo el campo `uint16_t discoveredVlanId = 0;`.
   * Al escuchar paquetes de capa 2 / broadcast (**MNDP de MikroTik**, **Ubiquiti Discovery UDP 10001**, **LLDP** y **CDP**) en un puerto Trunk RJ45, extraer el `VID` de la cabecera 802.1Q o del TLV de VLAN nativa y mostrarlo directamente en la tarjeta del equipo descubierto en pantalla (ej. `[VLAN 99] PowerBeam 5AC - 10.200.99.14`).

---

## 5. Archivos que se Modificarán al Retomar este Plan

| Archivo | Acción Planificada |
| :--- | :--- |
| `core/include/cbdos/network.hpp` | Añadir `setVlanId(uint16_t vid)`, `getVlanId()` y `applyProfile(const NetworkVlanProfile&)` a `INetworkAdapter`. |
| `core/include/cbdos/config_manager.hpp` | Añadir gestión y persistencia de `std::vector<NetworkVlanProfile>`. |
| `core/include/cbdos/neighbor_discovery.hpp` | Añadir campo `uint16_t vlanId` a `NeighborDevice` para reportar la VLAN detectada en troncales. |
| `bsp/esp32_p4_jc4880/hal/hal_network_p4.cpp` | Implementar ganchos 802.1Q de LwIP y soporte para interfaz RJ45 (`esp_eth`) + Wi-Fi (`esp_wifi`). |
| `bsp/esp32_s3_jc3248/hal/hal_network_s3.cpp` | Implementar conmutación rápida de perfiles de red/VLAN e IP estática/DHCP. |
| `core/src/ui/views/NetworkManagerView.cpp` | Añadir panel de selección rápida y edición de Perfiles de VLAN. |
| `core/src/ui/views/omniwisp/OmniwispMainView.cpp` | Añadir acceso rápido en cabecera para conmutar entre VLAN Admin / Clientes / Rescate sin salir de OmniWISP. |

