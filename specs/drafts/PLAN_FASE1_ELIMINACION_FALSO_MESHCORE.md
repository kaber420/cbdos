# 📋 Plan Técnico Detallado: Eliminación y Depuración del Falso MeshCore (Fase 1)

---

## 📌 1. Resumen Ejecutivo y Contexto Histórico

Durante las etapas tempranas de desarrollo de **CBDos v0.2.1**, se implementó un motor de red preliminar para validar la conectividad en pantalla y el enlace serie con el coprocesador USB. Este motor adoptó de forma tentativa y superficial el nombre *"MeshCore"* y el delimitador mágico `0x4D43` (`'MC'`), pero técnicamente constituyó un **mockup simulado**:
* **Criptografía inexistente:** No generaba claves asimétricas de curva elíptica (`Curve25519` / `Ed25519`); utilizaba un simple identificador de 2 bytes derivado de la MAC del chip y un cifrado simétrico por sustitución/XOR de carácter decorativo.
* **Enrutamiento por inundación ciega (*Flooding*):** Los paquetes recibidos simplemente incrementaban un contador `hops + 1` y se retransmitían por todas las interfaces activas si `hops < 7`. Carecía de cálculo de rutas inversas, retardos basados en calidad de enlace (SNR/CAD) o deduplicación criptográfica.
* **Herramientas de escritorio desalineadas:** Para interactuar desde la PC con este protocolo ficticio, se creó un entorno Dockerizado en `tools/meshcore_container/` que implementaba exactamente este mismo framing cerrado e incompatible.

Tras validar en el hardware real que el firmware oficial de **[meshcore.io](https://meshcore.io)** de Liam Cottle compila y corre de forma autónoma tanto en el **ESP32-C3** como en el **ESP32-S3** con soporte nativo de **ESP-NOW** y **LoRa**, este ecosistema simulado carece de valor operativo y representa deuda técnica, confusión arquitectónica y código muerto.

Este documento establece el **procedimiento operativo estándar (SOP)** para erradicar de forma segura, quirúrgica y auditable la suite de emulación de escritorio, aplicando la política de seguridad **Zero-Trust**.

---

## 🏗️ 2. Topología del Código Muerto vs. Estándar Oficial

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                            ARQUITECTURA OBSOLETA                             │
│                                                                              │
│  [tools/meshcore_container/cli.py]                                           │
│                 │                                                            │
│                 ▼                                                            │
│  [mesh_protocol.py (Trama 0x4D43 'MC')]                                      │
│                 │                                                            │
│                 ▼                                                            │
│  [mesh_framing.py (USB 0xAA 0x55)]                                           │
│                 │                                                            │
│                 ▼ (USB Serie CDC)                                            │
│  [tools/espnow_usb_bridge (ESP32-C3)] ───(ESP-NOW 2.4G)───► [P4 Falso Mesh]  │
│                                                                              │
│  ❌ Sin firmas Ed25519   ❌ Sin Path Building   ❌ Incompatible con MeshCore  │
└──────────────────────────────────────────────────────────────────────────────┘
                                      │
                                      ▼
┌──────────────────────────────────────────────────────────────────────────────┐
│                         NUEVA ARQUITECTURA OFICIAL                           │
│                                                                              │
│  [Cyberdeck ESP32-P4 (CBDos)]                                                │
│                 │                                                            │
│                 ▼ (USB CDC Host @ 12 Mbps / Protocolo Companion)             │
│  [Mochila ESP32-S3 / C3 con Firmware Oficial MeshCore (meshcore.io)]         │
│                 │                                                            │
│                 ▼ (ESP-NOW 2.4 GHz o LoRa 915 MHz con SX1262)                │
│  (((( 📡 AIRE ))))                                                           │
│                 ▲                                                            │
│                 │                                                            │
│  [Nodos Autónomos MeshCore (Repetidores, Room Servers BBS, Clientes Móviles)]│
│                                                                              │
│  ✅ Claves Curve25519/Ed25519   ✅ Ruteo Directo   ✅ Ecosistema Mundial      │
└──────────────────────────────────────────────────────────────────────────────┘
```

---

## 🗂️ 3. Auditoría Exhaustiva de Archivos a Eliminar (`tools/meshcore_container/`)

A continuación se detalla cada uno de los archivos que componen el contenedor obsoleto, su peso, su función interna y la justificación de su remoción:

| Archivo | Tamaño | Rol Técnico en la Simulación | Justificación de Eliminación |
| :--- | :--- | :--- | :--- |
| **`tools/meshcore_container/mesh_protocol.py`** | 2,391 B | Empaquetado binario de la trama falsa `0x4D43` (`PKT_BEACON`, `PKT_CHAT`, `PKT_ACK`) con structs fijos de 16 bytes. | **Obsoleto:** Incompatible con las tramas `mesh::Packet` de MeshCore oficial. |
| **`tools/meshcore_container/mesh_framing.py`** | 876 B | Enmarcado delimitador serie `0xAA 0x55` con cálculo de CRC-8 Dallas/Maxim. | **Obsoleto:** El firmware oficial de MeshCore utiliza el framing estándar SLIP/KISS o el protocolo serie binario de Companion Radio. |
| **`tools/meshcore_container/mesh_node.py`** | 10,285 B | Motor de eventos en Python que despachaba callbacks de recepción y transmisión en hilos separados. | **Obsoleto:** Acoplado exclusivamente a `mesh_protocol.py`. |
| **`tools/meshcore_container/meshcore_node.py`** | 20,648 B | Implementación monolítica alternativa que duplicaba la lógica de recepción serie y emulación de paquetes. | **Código duplicado y muerto:** Script experimental en desuso. |
| **`tools/meshcore_container/cli.py`** | 7,571 B | Interfaz de terminal interactiva para enviar mensajes de texto y balizas desde la consola de la PC. | **Obsoleto:** Reemplazado por el monitor serie interactivo nativo del propio firmware (`Generic_ESPNOW_terminal_chat`). |
| **`tools/meshcore_container/Dockerfile`** | 234 B | Definición de contenedor Docker para aislar dependencias de Python (`pyserial`). | **Innecesario:** El firmware oficial se monitorea y flashea directamente con PlatformIO nativo. |
| **`tools/meshcore_container/run.sh`** | 494 B | Script de bash para construir y ejecutar el contenedor montando el dispositivo `/dev/ttyACM*`. | **Innecesario:** Sin contenedor que ejecutar. |
| **`tools/meshcore_container/requirements.txt`** | 14 B | Archivo de requerimientos de Python (`pyserial`). | **Innecesario.** |
| **`tools/meshcore_container/__pycache__/`** | Directorio | Bytecode compilado de Python generado durante ejecuciones previas. | **Basura de compilación.** |

**Total de código a depurar:** ~43 KB de código Python/Docker acoplado al protocolo obsoleto.

---

## 🔍 4. Análisis de Dependencias y Acoplamiento Cruzado

Antes de ejecutar cualquier eliminación, se auditó la totalidad del árbol de directorios de CBDos para verificar si algún componente del sistema invoca o enlaza estos scripts:

1. **Sistema de Construcción (`CMakeLists.txt` / `Makefile`):**
   - El archivo raíz [`CMakeLists.txt`](file:///home/kaber420/Documentos/proyectos/cbdos/CMakeLists.txt) y los de [`bsp/esp32_p4_jc4880/`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/CMakeLists.txt) **NO referencian** ningún archivo dentro de `tools/meshcore_container/`.
2. **PlatformIO (`platformio.ini`):**
   - Las configuraciones de compilación de los BSPs y de `tools/meshcore_upstream` **NO tienen dependencias** hacia `tools/meshcore_container/`.
3. **Firmware del Cyberdeck (`core/` y `bsp/`):**
   - Ni el núcleo C++ ni los controladores del ESP32-P4 o ESP32-S3 llaman a los scripts de Python. El contenedor era una herramienta puramente externa de escritorio.
4. **Documentación (`docs/`):**
   - Existen menciones cruzadas en borradores preliminares (`ROADMAP_PORTADO_HERRAMIENTAS_CYBERDECK.md`, `auditoria_usb_host_eventos_nativos_y_cero_simulaciones.md`, `arquitectura_thread_safe_y_estabilizacion_meshcore_chat.md`). 
   - Dichos documentos de investigación **no se alterarán destructivamente** en esta fase para respetar la regla de integridad de historial técnico.

---

## ⚠️ 5. Matriz de Riesgos y Mitigaciones (Política Zero-Trust)

| Riesgo Identificado | Severidad | Probabilidad | Estrategia de Mitigación Obligatoria |
| :--- | :--- | :--- | :--- |
| **Borrado accidental de código crítico** | Crítica | Muy baja | **Respaldo físico completo e independiente** (`cp -r`) fuera del repositorio activo antes de cualquier comando destructivo. |
| **Fallo en scripts de automatización** | Media | Baja | Auditoría manual de referencias y comprobación con `grep` antes de remover directorios. |
| **Pérdida de historial de pruebas** | Baja | Media | El respaldo físico preservará íntegramente los scripts eliminados en caso de requerir análisis forense. |

---

## ⚙️ 6. Protocolo de Ejecución Paso a Paso (SOP)

### Paso 1: Ejecución del Respaldo Físico Completo
Antes de modificar el repositorio, se ejecutará una copia física idéntica de la raíz del proyecto hacia una ruta independiente de almacenamiento:
```bash
cp -r /home/kaber420/Documentos/proyectos/cbdos /home/kaber420/Documentos/proyectos/cbdos_backup_meshcore_cleanup
```
* **Criterio de Aceptación:** Verificar que el directorio de respaldo existe, comprobar que su tamaño coincida (`du -sh`) y que los archivos sean idénticos mediante suma de control.

### Paso 2: Eliminación Quirúrgica del Directorio Obsoleto
Una vez validada la existencia y consistencia del respaldo físico, se procederá a la remoción recursiva del directorio:
```bash
rm -rf /home/kaber420/Documentos/proyectos/cbdos/tools/meshcore_container
```

### Paso 3: Limpieza de Cachés y Rastro en Git
Verificar mediante `git status` que únicamente se registren como eliminados los ficheros contenidos dentro de `tools/meshcore_container/`, sin alterar ningún otro fichero del proyecto:
```bash
git status --short
```

---

## 🧪 7. Protocolo de Verificación y Pruebas de No-Regresión

Para considerar concluida con éxito la Fase 1, se deben superar satisfactoriamente las siguientes tres pruebas técnicas:

### Test 1: Verificación de Integridad del Respaldo Físico
* Comprobar que el respaldo en `/home/kaber420/Documentos/proyectos/cbdos_backup_meshcore_cleanup/tools/meshcore_container/` conserva intactos los 8 archivos eliminados.

### Test 2: Comprobación de Limpieza en el Directorio Activo
* Ejecutar:
  ```bash
  ls -d /home/kaber420/Documentos/proyectos/cbdos/tools/meshcore_container
  ```
  Debe retornar error indicando que el directorio no existe.

### Test 3: Compilación Limpia Multi-Target de CBDos
Asegurar que la remoción de las herramientas no generó ningún efecto colateral en la compilación de los firmwares del sistema:

1. **Target Principal ESP32-P4 (Guition JC4880P443C):**
   ```bash
   . /home/kaber420/esp/esp-idf/export.sh
   cd /home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880
   idf.py build
   ```
   * **Resultado esperado:** Compilación exitosa (`ninja: no work to do` o build completo sin errores).

2. **Target Secundario ESP32-S3 (JC3248W535):**
   ```bash
   cd /home/kaber420/Documentos/proyectos/cbdos
   pio run -d bsp/esp32_s3_jc3248
   ```
   * **Resultado esperado:** `SUCCESS` en el entorno PlatformIO.

---

## 🚀 8. Próximos Pasos (Hacia la Fase 2)

Una vez completada y verificada esta Fase 1 de purga:
1. Se redactará en `docs/drafts/` el plan formal de la **Fase 2: Arquitectura del Driver Host para MeshCore Companion**.
2. Dicho plan abordará el rediseño quirúrgico de `MeshCoreEngine.cpp` en `core/` para conectarlo al flujo de tramas de MeshCore oficial a través del driver USB CDC del P4, preservando la interfaz gráfica de LVGL 9.5.
