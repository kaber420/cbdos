# 🏛️ Especificación de la Suite de Gestión y SaaS para Torres, Gateways y Nodos Mesh (CBDos v0.2.1)

**Documento:** `docs/network/especificacion_suite_gestion_saas_torres_y_gateways.md`  
**Estado:** Especificación de Arquitectura de Software de Gestión  
**Versión:** 1.0.0 (RFC-CBDOS-GATEWAY-MGMT)  
**Fecha:** 25 de Agosto, 2026  
**Ámbito:** Panel de Administración Web, Microservicios de Gestión de Red (Router/Proxy/Hosting) y Seguridad.

---

## 🧭 1. Visión General y Filosofía de Diseño

En una infraestructura de red en malla descentralizada como **CBDos**, cada Torre o Estación Base actúa como un micro-proveedor de servicios autónomo (*Edge Cloud / Self-Hosted SaaS*). Para operar de forma profesional y segura, el sistema requiere herramientas de gestión dedicadas para cada uno de los tres pilares de la red:

```
                      ┌─────────────────────────────────────────────────────────────┐
                      │    PANEL MAESTRO DE ADMINISTRACIÓN Y SAAS DE TORRE (UI WEB)  │
                      └──────────────────────────────┬──────────────────────────────┘
                                                     │
             ┌───────────────────────────────────────┼───────────────────────────────────────┐
             ▼                                       ▼                                       ▼
 ┌───────────────────────────────┐       ┌───────────────────────────────┐       ┌───────────────────────────────┐
 │   1. GESTIÓN DEL ROUTER L3    │       │     2. GESTIÓN DEL PROXY      │       │     3. SAAS DE HOSTING .MESH  │
 ├───────────────────────────────┤       ├───────────────────────────────┤       ├───────────────────────────────┤
 │ • Topología & Vecinos Radio   │       │ • ACL de Salida a Internet    │       │ • Editor Visual con Preview   │
 │ • Pseudo-ARP SQLite (clients) │       │ • Cuotas de Tráfico WAN (GB)  │       │ • Cuotas de Almacenamiento    │
 │ • Mitigación de Flood / DoS   │       │ • Rate Limiting (reqs/min)    │       │ • Ingesta de Sensores IoT     │
 │ • Monitoreo de RSSI y Canales │       │ • Lista Blanca/Negra Dominios │       │ • Baneo de Nodos Abusivos     │
 └───────────────────────────────┘       └───────────────────────────────┘       └───────────────────────────────┘
```

---

## 📡 2. Módulo de Gestión del Router (`router.db`)

El subsistema de enrutamiento opera sobre `data/router.db` y provee:

### 2.1. Funcionalidades Clave:
1. **Monitor de Nodos en Tiempo Real:**
   - Lista en vivo de clientes asociados (`IPv4 10.x.y.z`, `Short ID`, `MAC física de silicio`, `Señal RSSI en dBm`, `Tiempo desde último paquete`).
2. **Defensa contra Abusos en Radio (Anti-Flood / Anti-DoS):**
   - Límite de paquetes por segundo por `Short ID` / `MAC`.
   - Bloqueo temporal automático si un cliente satura el medio 802.11 ESP-NOW.
3. **Control de Frecuencia y Potencia:**
   - Conmutación en caliente de canal Wi-Fi (1-13) y modo de radio (`Normal` vs `Long Range LR`).

---

## 🌍 3. Módulo de Gestión del Proxy Web (`proxy.db`)

El servicio de Proxy controla la salida hacia la WAN externa (Fibra óptica, 4G/LTE, Starlink):

### 3.1. Esquema de Base de Datos (`proxy.db`):
```sql
CREATE TABLE IF NOT EXISTS proxy_acls (
    mac TEXT PRIMARY KEY,
    ipv4 TEXT NOT NULL,
    status INTEGER DEFAULT 1,         -- 1 = Autorizado, 0 = Bloqueado
    bandwidth_quota_mb INTEGER DEFAULT 500, -- Cuota mensual en MB
    used_bandwidth_mb REAL DEFAULT 0,
    rate_limit_rpm INTEGER DEFAULT 60, -- Máximo de peticiones por minuto
    expires_at REAL DEFAULT 0          -- 0 = Permanente, o timestamp expiración
);

CREATE TABLE IF NOT EXISTS proxy_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    mac TEXT NOT NULL,
    ipv4 TEXT NOT NULL,
    timestamp REAL NOT NULL,
    url TEXT NOT NULL,
    bytes_transferred INTEGER NOT NULL,
    status_code INTEGER NOT NULL
);
```

### 3.2. Reglas de Negocio del Proxy:
- **Cuotas de Ancho de Banda:** Al alcanzar la cuota (ej: 2 GB), el proxy responde con un mensaje TLVGL informando que su cuota mensual ha sido consumida.
- **Protección de la Conexión de la Torre:** Rate limiting estricto para evitar que un solo nodo acapare el enlace satelital o celular de la comunidad.

---

## 🌐 4. SaaS de Hosting y Publicación Mesh (`hosting.db`)

Permite a los miembros de la comunidad crear y publicar sus propios sitios `.mesh`, blogs personales o tableros de sensores sin necesidad de saber programar en binario:

### 4.1. Esquema de Base de Datos (`hosting.db`):
```sql
CREATE TABLE IF NOT EXISTS accounts (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    mac TEXT UNIQUE NOT NULL,
    username TEXT UNIQUE NOT NULL,
    storage_quota_mb INTEGER DEFAULT 10, -- Cuota de disco para sus páginas
    used_storage_bytes INTEGER DEFAULT 0,
    is_banned INTEGER DEFAULT 0,
    created_at REAL NOT NULL
);

CREATE TABLE IF NOT EXISTS mesh_sites (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    account_id INTEGER NOT NULL,
    domain TEXT UNIQUE NOT NULL,       -- ej: "pedro.mesh"
    raw_html TEXT NOT NULL,
    compiled_tlv BLOB NOT NULL,
    hits_count INTEGER DEFAULT 0,
    updated_at REAL NOT NULL,
    FOREIGN KEY(account_id) REFERENCES accounts(id)
);

CREATE TABLE IF NOT EXISTS iot_streams (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    account_id INTEGER NOT NULL,
    sensor_name TEXT NOT NULL,
    protocol TEXT DEFAULT 'HTTP_JSON', -- HTTP_JSON, MQTT, TLV_MICRO
    last_value_json TEXT,
    updated_at REAL NOT NULL
);
```

---

## 🎨 5. Editor Visual con Live Preview para LVGL 9.5

El panel web incorpora un editor interactivo en el navegador:

```
 ┌─────────────────────────────────────────────────────────────────────────────────────────────┐
 │                           EDITOR VISUAL DE PÁGINAS .MESH (WEB UI)                           │
 ├──────────────────────────────────────────────┬──────────────────────────────────────────────┤
 │  [ CÓDIGO HTML / WIDGETS ]                   │  [ VISTA PREVIA SIMULADA (CYBERDECK DISPLAY) ]│
 │                                              │                                              │
 │  <panel style="left:10px; top:10px; ...">    │  ┌────────────────────────────────────────┐  │
 │    <h1>Mi Estación Solar</h1>                │  │ ⚡ Mi Estación Solar                   │  │
 │    <p>Batería: 12.8V (95%)</p>               │  │ Batería: 12.8V (95%)                   │  │
 │    <progress value="95" max="100"/>          │  │ [██████████████████████████████░░] 95% │  │
 │  </panel>                                    │  └────────────────────────────────────────┘  │
 │                                              │                                              │
 │  📊 Métricas de Compilación:                 │  📱 Resolución: 480x320 / 800x480            │
 │  • Peso HTML: 840 Bytes                      │  • Bytecode TLVGL: 184 Bytes (-78% compresión)│
 │                                              │                                              │
 │  [ 💾 Guardar Borrador ]                     │  [ 🚀 PUBLICAR EN LA MALLA (1-CLICK) ]        │
 └──────────────────────────────────────────────┴──────────────────────────────────────────────┘
```

---

## 🔒 6. Seguridad, Moderación y Aislamiento

1. **Aislamiento por MAC/IP:** Cada usuario solo puede editar los dominios `.mesh` asociados a su cuenta.
2. **Defensa contra Ataques XSS / Inyecciones:** El compilador TLVGL solo emite widgets seguros parseados formalmente (sin JavaScript ejecutable en el microcontrolador).
3. **Panel de Moderador:** Permite suspender sitios ofensivos o nodos abusivos con un solo clic.

---

## 📋 7. Plan de Implementación por Fases

1. **Fase 1 (Backend de Servicios):** Creación de `data/proxy.db` y `data/hosting.db` con sus respectivos modelos de datos.
2. **Fase 2 (API REST del Gateway):** Endpoints JSON en `tlvgl_server.py` para consultar estado de nodos, modificar cuotas ACL y subir páginas.
3. **Fase 3 (Frontend Web Dashboard):** Interfaz web moderna (HTML5/Vanilla CSS/JS) servida en el puerto 8080 para administración y edición visual.
