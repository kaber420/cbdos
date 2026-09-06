# 📋 PLAN MAESTRO DE IMPLEMENTACIÓN: CYBERBBS Y MESHCORE OFICIAL (meshcore-dev)

**Documento:** `docs/drafts/PLAN_TRANSICION_BBS_Y_PORTADO_MESHCORE_OFICIAL.md`  
**Estado:** Aprobado para Ejecución en la Siguiente Sesión  
**Autor:** Antigravity / CBDos Core Team  
**Módulos Afectados:** `core/src/apps/bbs/`, `core/src/apps/meshcore/`, `bsp/esp32_s3_jc3248/`, `bsp/esp32_p4_jc4880/`  

---

## 🏛️ 1. Arquitectura Dual y Separación de Responsabilidades

El subsistema de comunicaciones de CBDos opera bajo dos aplicaciones complementarias y no excluyentes:

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                            ARQUITECTURA DUAL CBDos                           │
├──────────────────────────────────────┬───────────────────────────────────────┤
│                                      │                                       │
│   📟 1. CYBER-BBS (App Local)        │   📡 2. MESHCORE OFICIAL (meshcore-dev)│
│   • Tablón / Foro Descentralizado    │   • Protocolo Oficial de Scott Powell │
│   • Motor ligero broadcast propio    │   • Ruteo con Rutas (Path Routing)    │
│   • MessagePack en Flash interna     │   • Cifrado ECDH Curve25519 + AES-128 │
│   • Para anuncios públicos locales   │   • 100% Compatible con red oficial   │
│                                      │                                       │
└──────────────────────────────────────┴───────────────────────────────────────┘
```

---

## 📟 PARTE 1: CyberBBS (`core/src/apps/bbs/`)

### 1.1. Objetivo
Convertir el motor ligero actual en un tablón de anuncios y foro offline descentralizado (estilo Packet Radio BBS y Usenet).

### 1.2. Especificaciones Técnicas
- **Firma Binaria:** `'BB'` (`0x4242`).
- **Tramas:**
  - `BBS_BEACON (0x01)`: Presencia de estación BBS o usuario.
  - `BBS_POST (0x02)`: Publicación en tablón (`#general`, `#anuncios`, `#trueque`, `#urgencias`).
  - `BBS_SYNC_REQ (0x03)`: Sincronización de últimos posts entre nodos.
- **Persistencia en Flash Interna:**
  - `/spiffs/bbs/posts.msgpack`: Almacena los últimos 100 posts estructurados en MessagePack.
  - `/spiffs/bbs/stations.msgpack`: Libreta de estaciones descubiertas en radar.
- **Interfaz Gráfica en LVGL 9.5 (Pantallas 3.5" y 4.3"):**
  - **Pestaña Tablones:** Hilos de discusión con inserción incremental fluida y scroll instantáneo (`LV_ANIM_OFF`).
  - **Pestaña Estaciones (Radar BBS):** Tarjetas a **2 renglones con marquesina circular** para eliminar textos encimados:
    ```
    ┌─────────────────────────────────────────────────────────────┐
    │ 📡 [0xCAFE] Estacion-Base-Taller-Sur                       │ ◄── Marquesina circular
    │ 📶 -42 dBm  •  0 saltos (Directo)  •  ESP-NOW CH 13         │ ◄── Telemetría compacta
    └─────────────────────────────────────────────────────────────┘
    ```
  - **Pestaña Radio:** Configuración manual de canal/modo con botón explícito `[ Guardar ]`.

---

## 📡 PARTE 2: MeshCore Oficial (`core/src/apps/meshcore/`)

### 2.1. Fuente de la Verdad y Dependencias
- **Código Fuente Upstream:** Ubicado en `tools/meshcore_upstream/src/` (clonado del repositorio oficial `meshcore-dev/MeshCore`).
- **Archivos Clave:**
  - `MeshCore.h` / `Packet.h` / `Packet.cpp`: Estructura de paquetes, cabeceras y hashes.
  - `Identity.h` / `Identity.cpp`: Criptografía de identidad (Ed25519 / Curve25519 y SHA-256).
  - `Mesh.h` / `Mesh.cpp` / `Dispatcher.h`: Máquina de estados de ruteo (*Hybrid Flood & Path-based Routing*).
- **Consumo Estimado de Flash:** ~160 KB (0.16 MB), menos del 3% de la memoria libre del ESP32-S3.

### 2.2. Adaptador HAL (`MeshCoreRadioBridge`)
Implementar la clase puente entre el motor oficial `mesh::Mesh` y los transceptores de CBDos:
- **Transceptor LoRa Semtech SX1262:** Conectado mediante SPI dinámico configurado por el `BackpackManager` (Mochilas NFC).
- **ESP-NOW / LR (2.4 GHz):** Para malla local de alta velocidad.

### 2.3. Interfaz de Cliente Oficial en LVGL 9.5
- **Lista de Contactos Oficiales:** Con validación de clave pública y cálculo de secreto compartido ECDH.
- **Visualizador de Rutas (Path & Trace):** Vista de repetidores por salto y telemetría de SNR en tiempo real.
- **Canales Cifrados AES-128:** Soporte de canales de grupo oficiales.

---

## 🗺️ 3. Plan de Ejecución Paso a Paso para la Siguiente Sesión

```
┌────────────────────────────────────────────────────────────────────────┐
│                        CRONOGRAMA DE EJECUCIÓN                         │
├────────────────────────────────────────────────────────────────────────┤
│  FASE 1: Transición y Renombrado a CyberBBS (UI + MessagePack)         │
│  FASE 2: Integración de Cabeceras y Núcleo C++ Oficial de MeshCore     │
│  FASE 3: Implementación del Adaptador RadioBridge HAL                  │
│  FASE 4: Construcción de la UI de Cliente Oficial MeshCore en LVGL 9.5 │
│  FASE 5: Validación Cruzada Multi-Target (S3 y P4) y con meshcore-cli  │
└────────────────────────────────────────────────────────────────────────┘
```

---

### 🔹 FASE 1: Transición a CyberBBS (`apps/bbs`)
1. Mover `core/src/apps/meshcore/` a `core/src/apps/bbs/` y renombrar clases a `BbsEngine` y `BbsView`.
2. Actualizar las tramas binarias a la firma `'BB'` (`0x4242`).
3. Rediseñar la lista de estaciones en el Radar a **2 renglones con marquesina circular** (`LV_LABEL_LONG_SCROLL_CIRCULAR`).
4. Integrar persistencia en MessagePack (`/spiffs/bbs/posts.msgpack` y `/spiffs/bbs/stations.msgpack`).
5. Registrar el icono de CyberBBS en el Dashboard principal de CBDos.

---

### 🔹 FASE 2: Integración del Núcleo Oficial de MeshCore
1. Crear el nuevo directorio `core/src/apps/meshcore/` con las fuentes oficiales de `tools/meshcore_upstream/src/`:
   - `MeshCore.h`, `Packet.h`, `Packet.cpp`, `Identity.h`, `Identity.cpp`, `Mesh.h`, `Mesh.cpp`, `Dispatcher.h`, `Dispatcher.cpp`.
2. Adaptar macros y wrappers de compatibilidad agnóstica para compilación limpia tanto en ESP-IDF 5.5 (P4) como en PlatformIO (S3).
3. Verificar compilación cruzada preliminar del núcleo sin errores.

---

### 🔹 FASE 3: Adaptador HAL RadioBridge
1. Implementar `cbdos::network::MeshCoreRadioBridge` derivado de la interfaz de transporte de MeshCore.
2. Conectar el despacho y recepción de paquetes a los controladores físicos de radio (`INetworkInterface` / SX1262 LoRa y ESP-NOW).
3. Encapsular el ciclo de ruteo de `MeshCore` en una tarea FreeRTOS desacoplada en Core 1.

---

### 🔹 FASE 4: UI de Cliente Oficial MeshCore en LVGL 9.5
1. Crear `MeshCoreView.hpp` y `MeshCoreView.cpp`:
   - **Pestaña Mensajes:** Chat con soporte de ruteo por rutas (`ROUTE_TYPE_DIRECT`) y flooding.
   - **Pestaña Contactos:** Lista de contactos oficiales con clave pública y estado de enlace.
   - **Pestaña Malla / Repetidores:** Visualización de repetidores descubiertos, saltos y calidad SNR (`PAYLOAD_TYPE_TRACE`).
   - **Pestaña Canales:** Gestión de canales cifrados AES-128.

---

### 🔹 FASE 5: Pruebas de Interoperabilidad y Validación
1. **Compilación Multi-Target:**
   - `pio run -d bsp/esp32_s3_jc3248`
   - `. /home/kaber420/esp/esp-idf/export.sh && cd bsp/esp32_p4_jc4880 && idf.py build`
2. **Prueba de Interoperabilidad con `meshcore-cli` Oficial:**
   - Conectar la laptop con `meshcore-cli` oficial.
   - Transmitir paquetes directos y de canal entre el Cyberdeck S3 y la laptop.
   - Verificar decodificación idéntica, ruteo por path y confirmaciones ACK en ambos extremos.
