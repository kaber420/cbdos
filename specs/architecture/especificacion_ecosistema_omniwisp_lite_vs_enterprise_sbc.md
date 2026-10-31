# 🏛️ Especificación de Arquitectura y Hoja de Ruta: Ecosistema OmniWISP (Lite vs. Pro / Enterprise Appliance)

**Documento:** `specs/architecture/especificacion_ecosistema_omniwisp_lite_vs_enterprise_sbc.md`  
**Versión:** 1.0.0 (RFC-OMNIWISP-PRO-01)  
**Estado:** 📋 Especificación de Visión y Arquitectura Registrada  
**Target Consola:** Multi-Target (Principalmente ESP32-P4 MIPI-DSI 7" / 10.1" con Ethernet RMII + Teclado USB Host, y ESP32-S3)  
**Target Servidor:** SBC Linux ARM64/x86_64 Headless (Raspberry Pi 4/5, CM4, Orange Pi o MiniPC) con Docker Engine  
**Fecha:** Octubre 2026  

---

## 1. Visión General y Separación de Alcances

El ecosistema **OmniWISP** se divide estratégicamente en dos niveles de producto complementarios pero independientes en su ciclo de despliegue, evitando desviar el desarrollo actual del kernel **CBDos** y eliminando la necesidad de escribir interfaces pesadas en PyQt sobre Linux:

1. **OmniWISP Lite (Integrado actualmente en CBDos):**
   * Herramienta táctica portátil ("Offline-First" e Instant-On en ~1.5 segundos) orientada al técnico de campo y trabajo en torre.
   * Opera 100% sobre el microcontrolador (**ESP32-S3** de 3.5" o **ESP32-P4** de 4.3") sin depender de servidores externos.
   * Aprovecha los motores nativos ya construidos en `core/`:
     * **Descubrimiento de Vecinos L2/L3 (`NeighborDiscoveryService`):** Escaneo activo/pasivo de equipos Ubiquiti (UDP 10001) y MikroTik MNDP (UDP 5678).
     * **Telemetría y Alineación en Vivo (`AirOSClient`):** Lectura de cadenas MIMO (`Chain 0 / Chain 1`), desbalance, piso de ruido, capacidad airMAX y diagnóstico físico de cable LAN PoE (detección de enlaces degradados a 10 Mbps o desconectados).
     * **Control y Adopción por SSH (`ISshClient` + `libvterm`):** Ejecución directa de comandos (`mca-cli-op info`, adopción L3 mediante `mca-cli-op set-inform`, reinicio de radios y consola VT100 interactiva).

2. **OmniWISP Pro / Enterprise (Proyecto Independiente: SBC Dockerizada + Consola Dedicada ESP32):**
   * Solución integral de administración de red WISP (CRM, corte de clientes, facturación, RADIUS, VPN y controladores centrales) concebida como un **Appliance Comercial** (similar en filosofía al ecosistema *TableHub*).
   * Separa el **Cómputo Pesado de Red y Base de Datos** (en una SBC Linux *headless* 100% dockerizada y endurecida en seguridad) de la **Consola Física de Administración** (una pantalla táctil dedicada de **7" o 10.1"** gobernada por un ESP32 con CBDos y soporte para **Teclado USB Host**).

---

## 2. Justificación Arquitectónica: ¿Por qué Consola ESP32 (LVGL v9.5) + SBC Headless en lugar de SBC + PyQt?

| Criterio Técnico | SBC con Entorno Gráfico (X11/Wayland + PyQt) | Arquitectura Híbrida OmniWISP Pro (ESP32 UI + SBC Headless) |
| :--- | :--- | :--- |
| **Tiempo de Arranque (Boot)** | Lento (25 a 45 segundos hasta cargar escritorio y Python). | **Instantáneo (~1-2 segundos)** en la pantalla; monitorea el arranque de la SBC en tiempo real. |
| **Resiliencia ante Fallos** | Si un contenedor Docker agota la RAM o cuelga el kernel, la pantalla táctil se congela por completo. | **Aislamiento Total:** Si la SBC se sobrecarga, la pantalla ESP32 sigue fluida, alerta al operador y puede reiniciar la SBC por GPIO (Hardware Watchdog) o apagarla limpiamente. |
| **Consumo de Recursos en SBC** | El servidor gráfico + PyQt consume ~400-800 MB de RAM y ciclos de GPU/CPU que deberían dedicarse a UniFi/RADIUS. | **100% de la RAM y CPU de la SBC** dedicados exclusivamente a los contenedores Docker (FreeRADIUS, DB, VPN, OmniWISP Web). |
| **Reutilización de Ingeniería** | Obliga a mantener dos bases de código UI distintas (C++ LVGL y Python PyQt). | **Reutilización del 100% de `core/` de CBDos:** Usa el mismo motor LVGL v9.5, `VaultService`, `usb_host` (teclado físico), `http_client` y `ssh_client`. |
| **Flexibilidad de Ubicación** | La pantalla debe estar pegada a la SBC por cable corto HDMI/DSI. | La SBC puede estar encerrada en el rack/nodo y la pantalla de 7" o 10.1" en el escritorio del administrador conectada por **Ethernet o Wi-Fi**. |

---

## 3. Arquitectura del Servidor: SBC OmniWISP Pro v2.0 (100% Dockerizada y Segura)

El proyecto de software para la SBC retoma la aplicación web original de **OmniWISP**, sometida a una **refactorización profunda, purga de código innecesario y endurecimiento de seguridad (Security Hardening)**:

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                   SBC HEADLESS (Linux ARM64 / x86_64)                          │
│  ┌──────────────────────────────────────────────────────────────────────────┐  │
│  │                       DOCKER ENGINE (Red Interna Aislada)                │  │
│  │                                                                          │  │
│  │  ┌─────────────────────┐  ┌─────────────────────┐  ┌──────────────────┐  │  │
│  │  │  omniwisp-core-api  │  │   omniwisp-radius   │  │   omniwisp-vpn   │  │  │
│  │  │ (Web App v2 Purgada,│  │ (FreeRADIUS 3.x +   │  │ (WireGuard /     │  │  │
│  │  │  CRM, Cortes, API)  │  │  Auth PPPoE/Hotspot)│  │  Túnel de Gestión│  │  │
│  │  └──────────┬──────────┘  └──────────┬──────────┘  └────────┬─────────┘  │  │
│  │             │                        │                      │            │  │
│  │  ┌──────────▼────────────────────────▼──────────┐  ┌────────▼─────────┐  │  │
│  │  │              omniwisp-database               │  │  unifi-controller│  │  │
│  │  │  (PostgreSQL / MariaDB - Sin puerto externo) │  │ (Adopción L3 APs,│  │  │
│  │  └──────────────────────────────────────────────┘  │  API REST Local) │  │  │
│  │                                                    └──────────────────┘  │  │
│  │  ┌────────────────────────────────────────────────────────────────────┐  │  │
│  │  │                       omniwisp-sbc-agent                           │  │  │
│  │  │  (Daemon ligero de telemetría de host, control de Docker y energía)│  │  │
│  │  └────────────────────────────────────────────────────────────────────┘  │  │
│  └──────────────────────────────────────────────────────────────────────────┘  │
└───────────────────────────────────────▲────────────────────────────────────────┘
                                        │
                    API REST (HTTPS / JWT / mTLS) + SSH / UART
                    (Vía Ethernet RMII, Wi-Fi o Enlace Local)
                                        │
┌───────────────────────────────────────▼────────────────────────────────────────┐
│            CONSOLA DEDICADA OMNIWISP PRO (ESP32-P4 7" o 10.1" + CBDos)         │
│  • Perfil de Compilación: CONFIG_CBDOS_PRODUCT_OMNIWISP (Modo Kiosco WISP)     │
│  • Entrada Rápida: Pantalla Táctil Capacitiva + Teclado Físico vía USB Host    │
│  • Seguridad: Credenciales y Tokens API cifrados en VaultService (NVS/AES)     │
└────────────────────────────────────────────────────────────────────────────────┘
```

### 3.1. Pilares de Refactorización y Seguridad de la Web App OmniWISP v2.0
1. **Purga de Deuda Técnica:** Eliminación de módulos experimentales incompletos, librerías obsoletas y endpoints sin uso del desarrollo previo.
2. **Aislamiento en Contenedores (`docker-compose.yml`):**
   * La base de datos (`omniwisp-database`) vive en una red interna de Docker (`internal: true`), completamente invisible desde la LAN/WAN.
   * Uso estricto de variables de entorno inyectadas por Docker Secrets / `.env` con permisos `0600`.
3. **API Unificada y Segura (`omniwisp-core-api`):**
   * Autenticación obligatoria mediante **Tokens JWT / API Keys de alta entropía** (almacenadas en el `VaultService` del ESP32).
   * Endpoints específicos y ligeros en JSON diseñados para que tanto el navegador web como la pantalla ESP32 puedan consultarlos en milisegundos.
4. **Agente de Sistema en la SBC (`omniwisp-sbc-agent`):**
   * Pequeño servicio local que expone métricas del hardware de la SBC (temperatura del SoC, uso de RAM/CPU, espacio en SSD/SD) y el estado de los contenedores Docker (`running`, `restarting`, `exited`), permitiendo reiniciar contenedores o ejecutar un `poweroff` seguro desde la pantalla del ESP32.

---

## 4. La Consola Dedicada: Perfil `OMNIWISP_PRO` en CBDos

Siguiendo exactamente el patrón arquitectónico validado para **TableHub** en [cbdos_build_profile.h](file:///home/kaber420/Documentos/proyectos/cbdos/core/include/cbdos_build_profile.h), la consola dedicada se implementa como un **Perfil de Producto de CBDos** (`CONFIG_CBDOS_PRODUCT_OMNIWISP`), proyectado idealmente para pantallas de **7 pulgadas o 10.1 pulgadas con ESP32-P4** (conectadas por Ethernet o Wi-Fi y con un **teclado USB** conectado al puerto USB Host del ESP32).

### 4.1. Módulos de la Interfaz Dedicada en la Pantalla (7" / 10.1")

1. **Gestor de Clientes y Servicio (CRM / Billing / Corte Rápido):**
   * Buscador instantáneo de clientes (aprovechando el **teclado físico USB Host** ya soportado por CBDos para escribir nombres, IPs o contratos con comodidad).
   * **Alta de Clientes:** Formulario rápido para registrar un nuevo suscriptor, asignarle plan de velocidad, IP/PPPoE y darlo de alta automáticamente en FreeRADIUS y en el router MikroTik/Ubiquiti.
   * **Corte y Reconexión en 1 Toque (Suspend / Restore):** Botón de acción directa para suspender morosos o reactivar servicios pagados (la consola envía la orden a `omniwisp-core-api`, que actualiza el perfil en FreeRADIUS / Address-List de MikroTik y patea la sesión activa).

2. **Panel de Control de la SBC y Contenedores Docker:**
   * Monitoreo en vivo de la SBC: Temperatura, carga de CPU, uso de RAM y estado del almacenamiento.
   * Lista visual de contenedores (`OmniWISP Web`, `FreeRADIUS`, `WireGuard VPN`, `UniFi Controller`, `Database`) con indicadores de salud y botones para **Reiniciar Contenedor**, **Ver Logs** o ejecutar **Apagado Seguro de la SBC**.

3. **Centro de Control VPN y RADIUS:**
   * Visualización de túneles VPN activos (nodos/torres conectadas al servidor central) con estado de latencia y tráfico.
   * Monitor de sesiones activas en **FreeRADIUS** (clientes autenticados, intentos fallidos de conexión y desconexión forzada de sesión CoA/PoD).

4. **Controlador UniFi y Aprovisionamiento en Red (Puente con Herramientas Nativas):**
   * Combina el descubrimiento local del ESP32 (`NeighborDiscoveryService`) con la API del Controlador UniFi que corre en la SBC:
     * Detecta un AP nuevo en la red por UDP 10001 -> Le inyecta por SSH (`ISshClient`) el `set-inform` apuntando a la IP de la SBC -> Le ordena por API REST al contenedor UniFi de la SBC que adopte el equipo.

---

## 5. Hoja de Ruta y Fases de Ejecución (Sin Interferir con el Desarrollo Actual)

Para garantizar orden total y cero desvíos sobre el trabajo actual en CBDos:

* **Fase Actual (En Curso en este Repositorio):**
  * Consolidación del kernel **CBDos** (homologación de pool PSRAM en LVGL v9.5 para ESP32-P4 y ESP32-S3, estabilidad de vistas y herramientas **OmniWISP Lite** de diagnóstico directo).
* **Fase Futura 1 (Repositorio / Proyecto Independiente `omniwisp-server`):**
  * Auditoría de seguridad, limpieza y dockerización al 100% de la Web App **OmniWISP**, integrando FreeRADIUS, VPN y el agente de control para la SBC.
* **Fase Futura 2 (Perfil de Consola Dedicada en CBDos para 7" / 10.1"):**
  * Creación del perfil `CONFIG_CBDOS_PRODUCT_OMNIWISP` en `cbdos_build_profile.h` y las vistas LVGL de pantalla panorámica optimizadas para teclado USB y comunicación con la API de la SBC.

