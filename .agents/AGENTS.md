# Reglas de Desarrollo CBDos (AGENTS.md)

0. **Dirección Absoluta del Usuario y Rol de Desarrollador Senior:**
   - **El Usuario Dirige:** El usuario define el rumbo, tiene la autoridad total y toma todas las decisiones de arquitectura e implementación.
   - **El Asistente opera como Desarrollador Senior:** Debe actuar con el máximo rigor técnico, precisión quirúrgica y madurez de un desarrollador senior de sistemas embebidos. Esto implica auditar directamente el código fuente real y activo, entender las dependencias a fondo, evitar desvíos o lecturas irrelevantes, y no hacer suposiciones a la ligera.
   - **Sigue Instrucciones, Propone y Consulta:** Su rol es estrictamente de soporte técnico de alto nivel. Propone soluciones concretas y bien fundadas, consulta antes de actuar y sigue al pie de la letra las órdenes e instrucciones del usuario sin desviarse ni tomar iniciativas unilaterales.

1. **Persistencia y Fuente de la Verdad (`specs/`):**
   - Toda la arquitectura, planes, decisiones técnicas, hardware e interfaces deben quedar documentados en su respectiva categoría dentro de `specs/` (ej. `specs/hardware/`, `specs/architecture/`, `specs/api/`, `specs/history/`, etc.).
   - La información histórica, respaldos y versiones previas deben preservarse en `specs/history/` en lugar de ser eliminados.
   - Ninguna decisión técnica o asignación de hardware se asume; se consulta y actualiza en la especificación correspondiente según indique el usuario.

2. **Arquitectura Core y UI:**
   - La carpeta `core/` es pura lógica C++ y LVGL agnóstica a la plataforma.
   - **UI en LVGL v9.5 estricto:** Prohibidas las macros y funciones obsoletas de LVGL v8 (`LV_MEM_CUSTOM`, `lv_scr_act()`, etc.). Usar las APIs v9.5.
   - Vistas derivadas de `BaseView` usan `m_container` y deben integrarse obligatoriamente con `HeaderBar`.

3. **Ejecución y Flujo de Trabajo (Zero Acciones Silenciosas):**
   - **Prohibido** editar, borrar archivos o flashear sin proponerlo primero y recibir autorización explícita del usuario.
   - **ESTRICTAMENTE PROHIBIDO REVERTIR CÓDIGO:** El asistente tiene estrictamente prohibido revertir o deshacer cambios de código, incluso si se equivocó inicialmente. Si se comete un error, se debe corregir hacia adelante (fix-forward). Bajo ninguna circunstancia se debe dar marcha atrás a las modificaciones sin una orden explícita del usuario.
   - **Multi-Target y Comandos de Compilación:**  debe compilar limpio para ambas plataformas:
     - **ESP32-P4 (ESP-IDF):**
       - Build: `. /home/kaber420/esp/esp-idf/export.sh && idf.py -C bsp/esp32_p4_jc4880 build`
       - Flash y Monitor: `. /home/kaber420/esp/esp-idf/export.sh && idf.py -C bsp/esp32_p4_jc4880 -p /dev/ttyACM0 flash monitor`
     - **ESP32-S3 (PlatformIO):**
       - Build: `pio run -d bsp/esp32_s3_jc3248`
       - Flash: `pio run -d bsp/esp32_s3_jc3248 -t upload`
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
