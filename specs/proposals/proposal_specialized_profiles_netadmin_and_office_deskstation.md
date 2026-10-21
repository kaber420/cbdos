# 🏢 Propuesta Técnica: Perfiles de Sistema Especializados — Perfil NetAdmin (WISP & SysAdmin) vs Perfil Office DeskStation (VoIP, Intercom & Productividad)

* **Estado:** 💡 Propuesta Formal (En Revisión por el Usuario)  
* **Autor:** Equipo CBDos / kaber420  
* **Fecha:** 2026-10-20  
* **Targets:** ESP32-P4 (JC4880P443C) y ESP32-S3 (JC3248W535)  
* **Framework Gráfico:** LVGL v9.5 estricto  
* **Arquitectura:** Modulación de Apps sobre `core/` + Sistema de Perfiles ([Build Profiles](file:///home/kaber420/Documentos/proyectos/cbdos/specs/proposals/proposal_build_profiles_and_modular_subsets.md))  

---

## 📋 1. Visión General y Justificación

Con el diseño de las nuevas suites de red (**NetRescue**, **NetTester**, **VoIP SIP** y **Voceo Multicast**), **CBDos** supera la concepción de un sistema operativo monolítico de juguete o demo para consolidarse como una plataforma modular capaz de generar **Appliances Profesionales de Hardware Dedicado**.

El usuario identifica dos nichos de despliegue claramente diferenciados:
1. **Perfil `PROFILE_NETADMIN` (La Navaja Suiza de Campo):** Para técnicos de soporte, administradores de sistemas y operadores WISP que van al rack, a la torre o a la camioneta a certificar cables, desbriquear routers y administrar equipos de red.
2. **Perfil `PROFILE_OFFICE_DESKSTATION` (Estación de Escritorio e Intercom):** Para oficinas, talleres, despachos, recepción o puestos de control, transformando el dispositivo en un teléfono IP de sobremesa con pantalla táctil, megafonía para altavoces Grandstream, reloj/alarmas de turnos y libreta de notas rápida.

```
                            ┌────────────────────────────────────────┐
                            │          CBDos Core Compartido         │
                            │  • HAL Display/Touch/Power             │
                            │  • LVGL v9.5 & HeaderBar               │
                            │  • lwIP Network Stack (Ethernet/Wi-Fi) │
                            │  • Audio Pipeline (I2S DMA & Codecs)   │
                            └───────────────────┬────────────────────┘
                                                │
                 ┌──────────────────────────────┴──────────────────────────────┐
                 ▼                                                             ▼
 ┌──────────────────────────────────────────────┐  ┌──────────────────────────────────────────────┐
 │          PERFIL: CBDos NetAdmin              │  │      PERFIL: CBDos Office DeskStation        │
 │       (Herramienta Táctica de Campo)         │  │     (Terminal de Oficina & Telefonía IP)     │
 ├──────────────────────────────────────────────┤  ├──────────────────────────────────────────────┤
 │ 🛠️ NetRescue (Ubiquiti/MikroTik/Agile Flash)  │  │ 📞 Teléfono IP SIP (Extensión PBX Asterisk)  │
 │ 🌐 LAN NetLab (Tester L1/L2, Recon, Tools)    │  │ 📢 Voceo & Megafonía Multicast (Grandstream) │
 │ 💻 Cliente SSH & Terminal Serie para Switches│  │ ⏰ Reloj de Turnos, Alarma & Temporizador    │
 │ 📂 Servidor TFTP de Campo para Backups       │  │ 📝 Bloc de Notas Rápido (Táctil + Voz I2S)   │
 │ ⚡ Fast Ping & Diagnóstico de Enlace          │  │ 🚨 Sirena de Alerta & Botón de Emergencia    │
 └──────────────────────────────────────────────┘  └──────────────────────────────────────────────┘
```

---

## 🛠️ 2. Perfil 1: `CBDos NetAdmin` (WISP, ISP & SysAdmin de Campo)

### Objetivo:
Reemplazar la laptop, el certificador de cables, la consola serie y los programas pesados de rescate por un único dispositivo autónomo que cabe en el bolsillo.

### Aplicaciones y Módulos Incluidos:
1. **NetRescue (Suite de Flasheo y Rescate):**
   - **Ubiquiti TFTP Flasher:** Inyección directa a `192.168.1.20` con `blksize=1428 bytes`.
   - **MikroTik Netinstall de Bolsillo:** Servidor BOOTP + entrega de Kernel ELF y formato de paquetes `.npk` (TCP 2000).
   - **TP-Link Agile Config Server:** Emulación del servidor oficial en `192.168.66.10` por puerto WAN.
   - **Servidor TFTP Universal:** Para recuperación de U-Boot, CFE y Cisco ROMmon.
2. **LAN NetLab (Suite Unificada de Diagnóstico y Laboratorio de Red):**
   - **Tab 1 (Tester L1/L2):** Medición TDR de cable (abierto/corto/metros), detección PoE (24V/48V), captura LLDP/CDP/MNDP (Switch Name, Puerto, VLAN ID).
   - **Tab 2 (Recon L3/L4/L7):** Barrido ARP de hosts, vendor MAC OUI en MicroSD, sondeo de top ports y descubrimiento de cámaras ONVIF / streaming RTSP.
   - **Tab 3 (Tools):** Detección de Rogue DHCP, Triple Ping interactivo con gráfica, Wake-on-LAN (WoL) y test iperf3.
3. **Administración en Rack:**
   - Cliente SSH embebido y terminal VT100 serie UART para configuración de switches sin laptop.

---

## 🏢 3. Perfil 2: `CBDos Office DeskStation` (Telefonía IP & Asistente de Oficina)

### Objetivo:
Crear una estación de trabajo de sobremesa fija para oficinas, despachos, recepción o garajes que combina telefonía corporativa, comunicación interna y organización personal.

### Aplicaciones y Módulos Incluidos:

#### 3.1. Teléfono IP SIP Corporativo (`VoipDialpadView`)
* Registro como extensión estándar (ej. Extensión `102`) en conmutadores **Asterisk, FreePBX o Grandstream UCM6xxx**.
* Marcador numérico táctil en LVGL v9 con directorio telefónico de contactos frecuentes en la MicroSD.
* Llamada en curso con control de volumen, silenciador de micrófono (*Mute*) y soporte manos libres con cancelación de eco (AEC).
* Soporte para llamadas directas por IP (*Direct IP Call*) a teléfonos Grandstream o Yealink.

#### 3.2. Voceo y Megafonía Multicast (*Office Paging*)
* **Botón Push-to-Talk (PTT) de Voceo:**
  - Al presionar el botón en pantalla, emite audio RTP directamente a la IP multicast de megafonía de la oficina (`224.0.1.75:50008`).
  - **Efecto:** Todos los teléfonos Grandstream y altavoces IP del edificio abren automáticamente su altavoz sin timbrar para transmitir anuncios generales ("Atención equipo, reunión en sala 2").

#### 3.3. Reloj de Oficina, Alarma y Avisador de Turnos (`OfficeClockView`)
* **Reloj de Escritorio en Pantalla Completa:** Modo salvapantallas con reloj digital elegante y estado del conmutador SIP.
* **Alarmas y Avisador de Turnos:**
  - Configuración de timbres periódicos (ej. inicio de jornada, descanso, cierre).
  - Emisión de campanada acústica por el altavoz local o retransmisión por megafonía.
* **Temporizador Pomodoro / Tareas:** Cronómetro visual para concentración en el escritorio.

#### 3.4. Bloc de Notas y Memos Rápidos (`OfficeNotesView`)
* **Notas Rápidas de Pantalla:** Teclado táctil QWERTY para apuntar mensajes de llamadas, recados o números de teléfono pendientes.
* **Memos de Voz (Grabadora rápida):**
  - Grabación inmediata de notas de voz de 10-60 segundos utilizando el micrófono I2S y almacenamiento directo en `/sd/notes/`.
  - Reproducción con un toque desde la lista de recados.

#### 3.5. Sirena y Alerta de Emergencia General
* Botón de alerta de seguridad en la barra superior o vista dedicada.
* Al confirmarse, emite un tono de sirena continuo de evacuación por la red multicast hacia todos los teléfonos IP de la oficina.

---

## 🎛️ 4. Métodos de Selección de Perfil

Se proponen dos modalidades operativas según las necesidades del usuario:

### Modalidad A: Selección en Tiempo de Compilación (Firmwares Especializados)
Mediante flags en CMake (ESP-IDF) o PlatformIO:
```bash
# Compilar imagen para Administradores de Red
idf.py -DCONFIG_PROFILE_NETADMIN=1 build

# Compilar imagen para Estación de Oficina / VoIP
idf.py -DCONFIG_PROFILE_OFFICE_DESKSTATION=1 build
```
* **Ventaja:** Excluye completamente el código y las dependencias no utilizadas del binario, reduciendo el consumo de Flash y RAM al mínimo estricto.

### Modalidad B: Selector de "Persona" en Tiempo de Ejecución (Firmware Universal)
Para usuarios que desean tener todo en una sola consola y cambiar de rol según el contexto:
* En el menú de **Ajustes de CBDos**, el usuario puede seleccionar la **"Persona Activa"**:
  - `[ Modo NetAdmin ]`: El Dashboard reordena la interfaz priorizando NetRescue, NetTester, LAN Recon y Terminal.
  - `[ Modo Oficina / DeskStation ]`: El dispositivo arranca en la vista de Reloj/Teléfono IP, bloquea herramientas técnicas y prioriza llamadas, notas y megafonía.

---

## 🚦 5. Próximos Pasos de Implementación

1. **Alineación con la propuesta de Build Profiles:** Incorporar estos dos perfiles formales en la taxonomía de [proposal_build_profiles_and_modular_subsets.md](file:///home/kaber420/Documentos/proyectos/cbdos/specs/proposals/proposal_build_profiles_and_modular_subsets.md).
2. **Desarrollo del Core VoIP & RTP:** Implementar la base compartida de audio y transporte SIP/RTP ([proposal_ip_phone_voip_sip_and_intercom.md](file:///home/kaber420/Documentos/proyectos/cbdos/specs/proposals/proposal_ip_phone_voip_sip_and_intercom.md)).
3. **Desarrollo del Core NetRescue:** Implementar la base TFTP/DHCP compartida ([proposal_field_router_flasher_unbrick_netrescue.md](file:///home/kaber420/Documentos/proyectos/cbdos/specs/proposals/proposal_field_router_flasher_unbrick_netrescue.md)).
4. **Vistas de Oficina:** Implementar `OfficeClockView` y `OfficeNotesView` en LVGL v9.5.
