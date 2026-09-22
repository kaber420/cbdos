# Reglas de Desarrollo CBDos (AGENTS.md)

1. **Persistencia Obligatoria (`specs/`):**
   - Hardware: Actualizar siempre `/home/kaber420/Documentos/proyectos/cbdos/specs/hardware/pinouts_and_ports.md`.
   - Arquitectura: Documentar HAL y core en `/home/kaber420/Documentos/proyectos/cbdos/specs/architecture/hal_and_core_architecture.md`.
   - APIs: Registrar en `/home/kaber420/Documentos/proyectos/cbdos/specs/api/core_apis_reference.md`.

2. **Arquitectura Core y UI:**
   - La carpeta `core/` es pura lógica C++ y LVGL agnóstica a la plataforma.
   - **UI en LVGL v9.5 estricto:** Prohibidas las macros y funciones obsoletas de LVGL v8 (`LV_MEM_CUSTOM`, `lv_scr_act()`, etc.). Usar las APIs v9.5.
   - Vistas derivadas de `BaseView` usan `m_container` y deben integrarse obligatoriamente con `HeaderBar`.

3. **Ejecución y Flujo de Trabajo (Zero Acciones Silenciosas):**
   - **Prohibido** editar, borrar archivos o flashear sin proponerlo primero y recibir autorización explícita del usuario.
   - **Multi-Target:** Cualquier cambio en `core/` debe compilar limpio tanto para ESP32-P4 (`idf.py build`) como para ESP32-S3 (`pio run`).
   - **Depuración en Caliente:** Usar la consola interactiva por puerto serie para validar APIs y comandos en caliente (`/dev/ttyACM0`). Evitar el ciclo lento de compilar y flashear repetitivamente para pruebas menores.

4. **Desacoplamiento y Sistema Reactivo:**
   - **Arranque "Offline-First":** Prohibido iniciar la red (Wi-Fi/BT) de fondo de forma síncrona al arranque. El SO debe funcionar de forma autónoma.
   - **Cero Polling Innecesario:** La UI y el sistema deben ser reactivos (basados en interrupciones, callbacks y flags como `m_statusDirty = true`). Estrictamente prohibido usar temporizadores cíclicos para consultar el hardware a menos que sea físicamente inevitable.

5. **Uso de Opencode y Modelos AI (Evaluación de Planes):**
   - El CLI de opencode se encuentra en `/home/kaber420/.opencode/bin/opencode`.
   - Para evaluar planes o archivos con un modelo específico, ejecutar en background:
     `/home/kaber420/.opencode/bin/opencode run -m "<provider/model>" "<prompt>"`
   - Los modelos preferidos (como los de Muse Spark) son:
     - `opencode/muse-spark-1.2-contributor-free`
     - `opencode/muse-spark-1.3-contributor-free`
   - Para listar modelos disponibles: `/home/kaber420/.opencode/bin/opencode models`
