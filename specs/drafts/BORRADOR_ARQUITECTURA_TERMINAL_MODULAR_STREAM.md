# Especificación de Arquitectura: Terminal Universal Modularizada por Flujos (`ITerminalStream`)

**Documento:** `docs/drafts/BORRADOR_ARQUITECTURA_TERMINAL_MODULAR_STREAM.md`  
**Estado:** Borrador de Arquitectura y Refactorización  
**Módulos Afectados:** `core/src/ui/views/TerminalView.*`, `core/src/ui/components/terminal/*`, `core/include/cbdos/terminal_stream.hpp`  
**Targets:** ESP32-P4 (JC4880P443C) / ESP32-S3 (JC3248W535)

---

## 📌 1. Diagnóstico del Problema: Monolito de 924 Líneas

Actualmente, el archivo `SerialTerminalView.cpp` concentra en una sola clase cuatro responsabilidades independientes:
1. **Control de Hardware Serie/USB:** Baudrates, detección hotplug de USB CDC, selección manual de pines GPIO y pulsos de control RST/DFU.
2. **Motor de Renderizado Gráfico de Texto:** Manejo de buffer, inserción incremental, pausa (*Hold*), truncado de memoria y eliminación de códigos de escape ANSI/VT100.
3. **Barra de Protocolo e Interacción:** Entrada de comandos, selección de fin de línea (`CRLF`, `LF`, `CR`, `None`), toggle de eco local y teclas de control rápido (`ENTER`, `^C`, `^Z`, `TAB`).
4. **Teclado en Pantalla y Ciclo de Vida LVGL:** Apertura/cierre del teclado virtual, callbacks y gestión de temas.

### Consecuencias de no modularizar:
* Dificultad para mantener y depurar el código.
* Imposibilidad de reutilizar el área de texto o la barra de comandos para otras herramientas (como SSH, Telnet o monitores de logs).
* Incrustar la lógica de conexión SSH (IP, credenciales, sockets de red) inflaría el archivo por encima de las 1,400 líneas.

---

## 🎯 2. Principio Arquitectónico: Abstracción de Transporte (`ITerminalStream`)

La interfaz gráfica de la terminal no debe saber si los bytes provienen de un cable de cobre (UART), de un periférico USB, o de un túnel cifrado a través de Wi-Fi (SSH). 

Toda la terminal se basa en un contrato C++ abstracto y puro:

```
                               ┌─────────────────────────────────────────────────────────┐
                               │                 ITerminalStream (Core)                  │
                               │  - write(buffer, length) -> size_t                      │
                               │  - read(buffer, maxLength) -> size_t                    │
                               │  - available() -> size_t                                │
                               │  - isConnected() -> bool                                │
                               │  - close() -> void                                      │
                               └────────────────────────────┬────────────────────────────┘
                                                            │
                     ┌──────────────────────────────────────┼──────────────────────────────────────┐
                     ▼                                      ▼                                      ▼
      ┌─────────────────────────────┐        ┌─────────────────────────────┐        ┌─────────────────────────────┐
      │      SerialStreamAdapter    │        │       UsbCdcStreamAdapter   │        │        SshStreamAdapter     │
      │  - UART JP1 / Pines Libres  │        │  - USB OTG Host CDC-ACM     │        │  - Sesión interactiva SSH   │
      │  - Control GPIO 34/54       │        │  - Control DTR / RTS        │        │  - PTY remota por Wi-Fi     │
      └─────────────────────────────┘        └─────────────────────────────┘        └─────────────────────────────┘
```

---

## 🧩 3. Descomposición Modular en Componentes (< 200 Líneas)

Para garantizar un código limpio, legible y fácil de depurar, la terminal se descompone en 4 piezas especializadas:

```
core/src/ui/
│
├── components/terminal/

│   ├── TerminalDisplay.hpp/.cpp       (~200 líneas)
│   │   - Responsabilidad: Área visual de texto monoespaciado en LVGL 9.5.
│   │   - Renderizado incremental rápido (sin recalcular layout completo).
│   │   - Stripper de secuencias de escape ANSI / VT100 (\x1b[...m).
│   │   - Mecanismo de pausa (Hold) y auto-scroll inteligente.
│   │   - Volcado del búfer visual a archivo en MicroSD.
│   │   - 100% desacoplado del hardware.
│   │
│   ├── TerminalCommandBar.hpp/.cpp    (~160 líneas)
│   │   - Responsabilidad: Entrada de comandos del usuario.
│   │   - Selector de fin de línea (CRLF, LF, CR, None).
│   │   - Toggle de Eco Local (ON / OFF).
│   │   - Botones rápidos de control (^C, ^Z, ENTER, TAB, SPACE).
│   │   - Botón para alternar visibilidad del teclado táctil virtual.
│   │
│   ├── SerialControlBar.hpp/.cpp      (~180 líneas)
│   │   - Responsabilidad: Barra de herramientas específica para UART/USB.
│   │   - Desplegable de puertos con soporte Hotplug USB CDC.
│   │   - Selector de Baudrate (9600 .. 921600).
│   │   - Botón Conectar/Desconectar.
│   │   - Botones de hardware dual [ ⚡ RST ] (Run) y [ 📥 DFU ] (Bootloader).
│   │
│   └── SshControlBar.hpp/.cpp         (~120 líneas)
│       - Responsabilidad: Barra de herramientas para transporte SSH.
│       - Indicador de Host conectado (`ubnt@192.168.1.20:22`).
│       - Botón `[ ⚙ Host / Perfil ]` para invocar el `SshConnectModal`.
│       - Botón `[ ▶ Conectar / ⏹ Desconectar ]`.
│
├── modals/
│   └── SshConnectModal.hpp/.cpp       (~180 líneas)
│       - Responsabilidad: Diálogo modal desacoplado para configuración y perfiles SSH.
│       - Selector de perfiles rápidos (Ubiquiti, MikroTik, RPi, Personalizado).
│       - Formulario: IP, Puerto, Usuario, Tipo Auth (Password / Clave privada).
│       - Feedback de conexión interactivo con barra de estado y errores.
│       - Guardado y carga de perfiles en MicroSD (`/sdcard/system/ssh_hosts.json`).
│
└── views/
    └── TerminalView.hpp/.cpp          (~180 líneas)
        - Vista orquestadora principal (derivada de BaseView).
        - Ensambla el layout vertical: [Barra de Transporte Activa] + [TerminalDisplay] + [TerminalCommandBar].
        - Conecta los eventos del usuario con el transporte activo (Serial o SSH).
```

---

## 🖼️ 3.1. Especificación Detallada de `SshConnectModal`

El diálogo modal se implementa como una clase independiente en `core/src/ui/modals/SshConnectModal.hpp/.cpp` para no sobrecargar la vista principal:

```
┌────────────────────────────────────────────────────────────────────────┐
│  🌐 Conexión SSH Remota                       [ ✕ Cerrar ]             │
├────────────────────────────────────────────────────────────────────────┤
│ Perfil: [ 📂 Ubiquiti NanoStation AC (192.168.1.20)               ▼ ]  │
├────────────────────────────────────────────────────────────────────────┤
│ Host / IP:  [ 192.168.1.20                     ]  Puerto: [ 22       ] │
│ Usuario:    [ ubnt                             ]                       │
│ Autentic.:  [ Contraseña                    ▼ ]                        │
│ Password:   [ ••••••••••••                     ] [ 👁 Ver ]             │
│ Clave Priv: [ /sdcard/keys/id_ed25519          ] [ 📁 Explorar ]       │
├────────────────────────────────────────────────────────────────────────┤
│ Estado: 🟡 Esperando acción del usuario...                             │
├────────────────────────────────────────────────────────────────────────┤
│ [ 💾 Guardar Perfil ]         [ Cancelar ]      [ ⚡ Conectar Sesión ]  │
└────────────────────────────────────────────────────────────────────────┘
```

### Funcionalidad y Ciclo de Vida del Modal:
1. **Invocación:** Se abre automáticamente si el usuario selecciona `[ 🌐 SSH Remoto ]` sin conexión activa, o al presionar el botón `[ ⚙ Host ]` en la barra superior.
2. **Carga de Perfiles:** Lee `/sdcard/system/ssh_hosts.json`. Al seleccionar un perfil del desplegable (ej. *Ubiquiti* o *MikroTik*), los campos de IP, puerto y usuario se autocompletan de inmediato.
3. **Feedback de Conexión en Vivo:**
   - Al presionar **`[ ⚡ Conectar Sesión ]`**, el modal **no se cierra a ciegas**:
   - Cambia la etiqueta de estado: `🟡 Resolviendo host...` -> `🟡 Intercambiando claves SSH...` -> `🟡 Autenticando...`
   - **Si tiene éxito:** El modal emite el callback `onConnected(session)` a la terminal y se cierra suavemente con animación.
   - **Si falla (ej. clave incorrecta o host inalcanzable):** Muestra el mensaje de error en rojo (`🔴 Error: Autenticación rechazada por el servidor`) y permite corregir el campo sin perder los datos escritos.
4. **Persistencia:** Permite guardar nuevos hosts en la MicroSD con el botón `[ 💾 Guardar Perfil ]`.

---

## 🔄 4. Experiencia de Usuario Unificada (UI Flow)

```
┌──────────────────────────────────────────────────────────────────────────────────────────┐
│ [ Modo: UART JP1 ▼ ] [ 115200 ▼ ] [ ▶ Conectar ] [ ⚡ RST ] [ 📥 DFU ] [ ⏸️ Hold ] [ 🗑️ ] │ <- SerialControlBar
├──────────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                          │
│  MikroTik RouterOS 7.15.2 (c) 1999-2026                                                  │
│  [admin@MikroTik-NodoNorte] > /interface print                                           │
│  Flags: R - RUNNING                                                                      │
│  Columns: NAME, TYPE, ACTUAL-MTU, MAC-ADDRESS                                            │
│  #   NAME    TYPE  ACTUAL-MTU  MAC-ADDRESS                                               │
│  0 R ether1  ether       1500  48:8F:5A:12:34:56                                         │
│  1 R ether2  ether       1500  48:8F:5A:12:34:57                                         │
│                                                                                          │ <- TerminalDisplay
│                                                                                          │
├──────────────────────────────────────────────────────────────────────────────────────────┤
│ [ CRLF ▼ ] [ Echo: OFF ] | [ ENTER ] [ ^C ] [ ^Z ] [ TAB ] [ / ]                         │ <- TerminalCommandBar
├──────────────────────────────────────────────────────────────────────────────────────────┤
│ [ /system reboot                                       ] [ ▶ Enviar ] [ ⌨️ Teclado ]     │
└──────────────────────────────────────────────────────────────────────────────────────────┘
```

### Alternancia Dinámica entre Serie y SSH:
1. El usuario toca el selector de modo: `[ Modo: UART JP1 ▼ ]` y selecciona `[ 🌐 SSH Remoto ]`.
2. La barra superior reemplaza instantáneamente los controles de baudrate y pines por el panel de SSH:
   `[ Modo: SSH Remoto ▼ ] [ 192.168.88.1:22 ] [ ⚙ Config ] [ ▶ Conectar ] [ ⏸️ Hold ] [ 🗑️ ]`.
3. El área de texto (`TerminalDisplay`) y la barra de entrada (`TerminalCommandBar`) se mantienen exactamente iguales, sin parpadeos ni recargas de layout.
4. Si el usuario presiona `^C`, el byte `0x03` se envía al socket SSH con la misma naturalidad con la que se enviaría por el pin TX de la UART.

---

## 📈 5. Beneficios Inmediatos

| Métrica | Antes (Monolito) | Después (Modular) |
| :--- | :--- | :--- |
| **Tamaño del archivo mayor** | 924 líneas (`SerialTerminalView.cpp`) | < 200 líneas (`TerminalDisplay.cpp`) |
| **Responsabilidad Única (SRP)** | Violada (Hardware + Parser + UI + Teclado) | Cumplida (1 archivo = 1 propósito) |
| **Soporte de Nuevos Protocolos** | Implica reescribir y arriesgar la vista | Se añade un adapter `ITerminalStream` sin tocar la UI |
| **Reutilización de Código** | Nula (código atrapado en la vista) | Alta (`TerminalDisplay` reusable en otras apps) |
