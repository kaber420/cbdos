# Reglas del Proyecto y Guía de Desarrollo (CBDos v0.2.3)



---
## ⚠️ 3. Reglas Obligatorias de Desarrollo

1. **Persistencia Obligatoria y Modularidad de Especificaciones e Ingeniería (`specs/`):**
   - **Hardware y Pines:** Toda información técnica (GPIOs, buses, LDOs, puertos UART) DEBE documentarse y actualizarse en `/home/kaber420/Documentos/proyectos/cbdos/specs/hardware/pinouts_and_ports.md`.
   - **Arquitectura de Software y HAL:** Toda interfaz abstracta, módulo HAL o patrón arquitectónico DEBE documentarse en `/home/kaber420/Documentos/proyectos/cbdos/specs/architecture/hal_and_core_architecture.md`.
   - **APIs para Desarrolladores y SDK:** Todo nuevo servicio o API accesible para aplicaciones DEBE documentarse con ejemplos en `/home/kaber420/Documentos/proyectos/cbdos/specs/api/core_apis_reference.md` y guías en `/home/kaber420/Documentos/proyectos/cbdos/specs/api/how_to_create_an_app.md`.
   - **Portal Público (`docs/`):** La carpeta `docs/` queda reservada exclusivamente para el portal web limpio de GitHub Pages.

2. **Verificación Multi-Target Obligatoria:**
   - Cada cambio en `core/` debe compilar limpiamente en **AMBOS** entornos:
     - `idf.py build` (ESP32-P4)
     - `pio run -d bsp/esp32_s3_jc3248` (ESP32-S3)

3. **UI Framework (LVGL v9.5 Estricto):** 
   - El proyecto utiliza **LVGL v9.5** con el tema base `DefaultTheme` y gestión de memoria en PSRAM.
   - **ESTRICTAMENTE PROHIBIDO** usar macros o sintaxis obsoletas de **LVGL v8** (ej. `LV_MEM_CUSTOM`, `lv_scr_act()`, etc.). Usar exclusivamente las APIs de LVGL 9.5 (`lv_screen_active()`, `lv_button_create()`, `lv_image_create()`, etc.).

4. **Integración con HeaderBar y Navegación:**
   - Toda vista derivada de `BaseView` debe usar el contenedor base `m_container` (sin redeclararlo en la clase derivada para evitar *variable shadowing*).
   - Los botones de navegación de cabecera deben usar `HeaderBar::setRightAction()`.
   - Si la aplicación no usa internet, debe llamar a `HeaderBar::showWifi(false)`.

5. **Control de Ejecución Estricto (Zero Presumption & Zero Acciones Silenciosas):**
   - **El usuario dirige y autoriza; la IA propone y ejecuta únicamente con aprobación previa.**
   - **PROHIBIDO** editar código fuente, crear archivos, borrar ficheros o flashear sin la previa propuesta, explicación y **autorización explícita** del usuario.
   - **PROHIBIDO** ejecutar comandos ocultos, descargas o bucles de herramientas en silencio. Siempre se debe explicar brevemente qué se va a hacer antes de tocar nada.
   - Ante cualquier falla o diagnóstico, la IA debe presentar el diagnóstico al usuario, explicar la causa y la solución propuesta, y **esperar a que el usuario dé la orden de aplicar los cambios**.

6. **Uso de OpenCode (Modelos externos):**
   - Usar `opencode run "<instrucción>"` para ahorrar tokens de contexto.
   - **Selección de modelo:** Para usar modelos específicos (como deepseek o mimo), indicarlo con `-m` (ej. `opencode run -m opencode/deepseek-v4-flash-free "<instrucción>"`).

7. **Desacoplamiento Total del Arranque (Offline-First Estricto):**
   - **ESTRICTAMENTE PROHIBIDO** inicializar la red (Wi-Fi, Bluetooth, ESP-Hosted, DHCP o tareas de fondo de red) de forma síncrona o automática en `app_main()` o durante el encendido del sistema.
   - El sistema operativo CBDos **DEBE ser 100% funcional y autónomo** (pantalla, táctil, audio, almacenamiento MicroSD, interfaz gráfica LVGL) **con o sin red conectada**, sin depender de la presencia, alimentación o respuesta de ningún coprocesador inalámbrico (ESP32-C6).
   - Toda inicialización de red debe ser **exclusivamente bajo demanda** cuando el usuario lo solicite explícitamente desde la UI o API.

8. **Ley de Pureza Arquitectónica de `core/` (Zero Platform Pollution):**
   - **Agnosticismo Estricto de `core/`:** `core/` DEBE ser código C++ estándar y LVGL 9.5 puro.
   - **PROHIBIDO** incluir headers de plataformas o frameworks dentro de `core/` (ej. `<Arduino.h>`, `<Preferences.h>`, `<SD.h>`, `<WiFi.h>`, `<esp_wifi.h>`, `<driver/...>`).
   - **PROHIBIDO** bifurcar la lógica de negocio mediante `#ifdef ARDUINO` o `#ifdef ESP_PLATFORM` dentro de `core/`.
   - **Patrón de Abstracción HAL / Interfaces:** Todo acceso a hardware, NVS, sistema de archivos o red DEBE realizarse mediante interfaces abstractas (`IStorageBackend`, `IAudioSink`, `INetworkAdapter`). Las implementaciones concretas residen ÚNICAMENTE en sus respectivos directorios `bsp/` y se inyectan en tiempo de inicialización.

9. **Integridad Absoluta de Documentación e Historial (`specs/`):**
   - **ESTRICTAMENTE PROHIBIDO** sobreescribir, borrar, truncar o reciclar archivos de documentación existentes en `specs/` o `specs/drafts/` a menos que el usuario lo ordene explícitamente indicando el archivo.
   - Toda nueva investigación, borrador o análisis DEBE crearse en un archivo nuevo con nombre único y descriptivo.

10. **Prohibición Universal de Resúmenes y Ediciones Destructivas (Aplica a TODO el Proyecto):**
    - **ESTRICTAMENTE PROHIBIDO** condensar, simplificar, resumir, truncar o sobreescribir de forma destructiva **CUALQUIER archivo del repositorio** (código fuente `.cpp`/`.c`, cabeceras `.hpp`/`.h`, documentación `.md`, planes, especificaciones, scripts `.lua`/`.py`, Makefiles o configuraciones).
    - Toda modificación en cualquier archivo del proyecto debe ser **estrictamente quirúrgica**, preservando intacto todo el código preexistente, comentarios, contratos de funciones, diagramas ASCII, tablas y explicaciones técnicas previamente desarrolladas.

11. **Flujo de Pruebas y Depuración en Caliente por Serial (Zero Compilaciones Innecesarias):**
    - **Consola Serial Interactiva:** El BSP cuenta con una tarea de depuración en caliente (`ENABLE_CBDOS_SERIAL_DEBUG_CLI`) a través de la consola `/dev/ttyACM0` (USB-Serial-JTAG).
    - **Pruebas Inmediatas sin Flashear:** Para validar APIs, ataques BadUSB, eventos HID (`hid.*`), DuckyScript (`ducky: ...`), o llamadas del sistema (`sys.*`, `cbdos.*`), la IA **DEBE conectarse por puerto serie y enviar los comandos de prueba en caliente** para obtener feedback inmediato, en lugar de compilar y flashear repetitivamente.
    - **Modo Producción:** La macro `ENABLE_CBDOS_SERIAL_DEBUG_CLI` en `hal_hid_p4.cpp` debe mantenerse disponible para desarrollo y comentarse/desactivarse (`0`) únicamente al generar builds finales de producción.

12. **Principio de Arquitectura Reactiva / Cero Polling Innecesario:**
    - **ESTRICTAMENTE PROHIBIDO** implementar temporizadores periódicos de sondeo (*polling*) para consultar el estado del hardware si la capa física, los buses (USB, SDIO, I2C/SPI) o el sistema operativo ya cuentan con eventos, interrupciones o callbacks nativos (ej. inserción/extracción USB, conexión/desconexión de red, llegada de paquetes, eventos de almacenamiento o cambios de batería).
    - **Prohibición de Atajos en la UI:** La interfaz gráfica (LVGL) no debe gastar ciclos de CPU ni recursos preguntando en bucle cada segundo si un periférico cambió. La UI debe operar mediante banderas reactivas (`m_statusDirty = true`) disparadas exclusivamente por eventos y callbacks legítimos del backend.
    - **Excepcionalidad del Polling:** El sondeo periódico solo se permite en buses o sensores *legacy* que carezcan físicamente de líneas de interrupción (ej. algunos sensores ambientales I2C básicos), justificando y documentando siempre dicha limitación física.

13. **Seguimiento Local y Registro de Tareas (Project Management para Agentes):**
    - El directorio `specs/project_management/` funciona como nuestro "tablero local" para el equipo (Usuario + Agentes IA).
    - Al completar una tarea significativa, refactorización o corrección de bugs, la IA **DEBE actualizar proactivamente** `specs/project_management/CHANGELOG.md` y marcar el progreso en `specs/project_management/REFACTORING_STATUS.md`.
    - Si se detecta o reporta un nuevo error, la IA debe registrarlo en `specs/project_management/KNOWN_BUGS.md`. Al solucionarlo, debe moverlo de dicho archivo al Changelog.
    - Antes de iniciar un nuevo sprint o tarea compleja, la IA debe consultar y mantener actualizado `specs/project_management/CURRENT_STATUS.md` para evitar desviaciones.