# ⏱️ Propuesta Formal: Lanzador Cron Ultra-Ligero por Indicadores Cortos (`CronLauncher` & `cbdos.cron`)

**Fecha:** 20 de Octubre de 2026  
**Estado:** 💡 Propuesta Formal  
**Categoría:** Arquitectura del Sistema, Automatización & Scripting (`core/src/lua/`, `core/src/system/`)  
**Plataformas Objetivo:** ESP32-S3 (`jc3248`) y ESP32-P4 (`jc4880`)

---

## 1. Visión General y Casos de Uso

En misiones de campo, administración de redes WISP/ISP, auditoría táctica o monitoreo industrial, el usuario necesita dejar tareas programadas que ejecuten scripts de Lua (`.lua` / `.luapp`) en una **fecha/hora exacta**, de forma **periódica** o tras una **cuenta regresiva**, sin mantener la máquina virtual de Lua encendida ni consumir memoria RAM con cadenas de texto o código cargado.

### Principio Central de Diseño: "Cero Scripts y Cero Rutas en RAM"
En lugar de mantener scripts pre-cargados en memoria o listas de `std::string` con rutas largas (`/sdcard/scripts/auditoria_nocturna_mikrotik.lua`), el **CronLauncher** de CBDos opera únicamente como un **reloj despertador numérico ultra-compacto**:

1. **En RAM solo existe un arreglo estático de indicadores cortos (`uint32_t scriptId`, ej. `4g2hx`) y su timestamp de disparo (`12 bytes` por tarea).**
2. **Resolución Diferida (Lazy Resolution):** Mientras el sistema espera minutos, horas o días, no sabe ni le importa qué ruta o código tiene ese script; solo compara dos enteros de 32 bits (`now >= nextFire`).
3. **Disparo bajo demanda:** Recién en el instante exacto en que el contador vence para el indicador `4g2hx`, el lanzador consulta el archivo índice en la MicroSD/Flash (`/sdcard/config/crontab.idx`), obtiene la ruta o acción asociada a `4g2hx`, e invoca `LuaRunner::getInstance().startScript(path)`. Una vez que el script termina, la VM de Lua libera toda su memoria.

### Casos de Uso Reales en CBDos
- **Administración de Campo / WISP (SSH + LAN Recon):** Programar el código `bkp01` a las `03:00 AM` para que CBDos despierte, se conecte por SSH a un router MikroTik/Ubiquiti, exporte el backup a la MicroSD y vuelva a reposo.
- **Auditoría Táctica / BadUSB Diferido (HID / Ducky):** Dejar programado el indicador `hid9x` con una cuenta regresiva o a las `14:30` para inyectar una secuencia `cbdos.hid` / `cbdos.ducky` cuando el operador ya no está frente al equipo.
- **Telemetría UART / GPIO y Alertas LoRa/MeshCore:** Ejecutar cada 15 minutos (`900s`) el indicador `sns42` que lee un sensor por `cbdos.uart` y envía el reporte; o disparar una tarea remotamente por radio LoRa enviando únicamente 5 caracteres (`cron run 4g2hx`).

---

## 2. Cumplimiento Arquitectónico (`AGENTS.md`)

1. **Agnosticismo de `core/`:** Todo el servicio `CronLauncher` vive en `core/` usando exclusivamente `cbdos::time::getEpoch()`, `cbdos::system::getTimeMs()` y `cbdos::storage`, sin dependencias de hardware específicas de ESP-IDF o Arduino.
2. **Cero Polling Innecesario (Regla 4):**
   - No existe ningún bucle evaluando cadenas de texto ni expresiones en cada tick.
   - Se engancha al pulso de reloj existente de `cbdos::time` / `HeaderBar` (1 verificación entera por segundo para timers de cuenta regresiva, o 1 vez por minuto para horarios de calendario), tomando **menos de 1 microsegundo (µs) de CPU**.
3. **Huella de RAM Despreciable (100% Estática):**
   - Cada tarea programada ocupa exactamente **12 bytes** (o **16 bytes** alineados con flags).
   - Una tabla máxima de 32 tareas programadas consume apenas **384 a 512 bytes de SRAM fija**, con **cero `malloc` / `new` / `std::string`** residentes en memoria.
4. **Offline-First:** Soporta tanto tiempo de calendario real (`Epoch` vía RTC/NTP/Tower) como tiempo relativo desde el arranque (`getTimeMs()`), permitiendo programar tareas incluso sin conexión a red ni NTP sincronizado.

---

## 3. Arquitectura Técnica y Contratos en C++

### 3.1. Codificación del Indicador Corto (` Base36 / Short-ID de 5-6 caracteres `)
Cada script o tarea registrada recibe (o define en su cabecera `@id: 4g2hx`) un código alfanumérico corto de hasta 6 caracteres en **Base36** (`0-9`, `a-z`), el cual cabe de forma exacta y reversible dentro de un único entero `uint32_t` ($36^6 = 2,176,782,336 \le 2^{31}-1$):

- Ejemplo en texto humano / CLI / Lua: `"4g2hx"`
- Representación en RAM dentro del Cron: `uint32_t id = 0x00729B0D` (**4 bytes exactos**, sin punteros ni `char*`).
- Si el usuario no especifica un `@id` manual, el sistema genera automáticamente los 5 caracteres Base36 a partir del hash CRC32/FNV-1a del nombre del archivo (ej. `backup_ssh.lua` $\rightarrow$ `4g2hx`).

### 3.2. Estructura en Memoria (`CronSlot` — 16 bytes fijos)

```cpp
#pragma once
#include <cstdint>
#include <cstddef>

namespace cbdos {
namespace cron {

enum class TriggerMode : uint8_t {
    Unused      = 0,
    OneShotEpoch = 1, // Fecha/hora exacta Unix Epoch (se desactiva tras disparar)
    RepeatEpoch  = 2, // Cada N segundos alineado a reloj real Epoch
    OneShotUptime= 3, // Cuenta regresiva en segundos desde getTimeMs()/1000
    RepeatUptime = 4, // Cada N segundos usando uptime (no requiere RTC/NTP)
    DailyHm      = 5  // Hora y minuto diario (intervalOrHm = (hour << 8) | min, mask en flags)
};

// Estructura ultra-compacta: 16 bytes exactos en RAM, cero punteros, cero heap.
struct CronSlot {
    uint32_t scriptId;       // Código Base36 empaquetado (ej. "4g2hx" -> uint32_t)
    uint32_t nextFire;       // Próximo segundo exacto de disparo (Epoch o UptimeSec)
    uint32_t intervalOrHm;   // Intervalo de repetición en seg, o (hora << 8 | minuto)
    TriggerMode mode;        // Modo de disparo (1 byte)
    uint8_t dowMask;         // Máscara de días de la semana (bit 0=Dom .. bit 6=Sáb, 0x7F=Todos)
    uint8_t flags;           // Bit 0: enabled, Bit 1: runIfBusyQueue, Bit 2: requireSync
    uint8_t reserved;        // Alineación a 16 bytes
};

static_assert(sizeof(CronSlot) == 16, "CronSlot debe ocupar exactamente 16 bytes en RAM");

class CronLauncher {
public:
    static constexpr size_t kMaxSlots = 32; // 32 * 16 bytes = 512 bytes totales en RAM

    static CronLauncher& getInstance();

    // Carga solo los números (IDs y tiempos) desde /sdcard/config/crontab.idx al arrancar
    bool init();

    // Llamado 1 vez por segundo (entero ligero): solo compara nextFire <= now
    void tick(uint32_t currentEpoch, uint32_t currentUptimeSec, bool timeSynced);

    // Conversión sin heap entre código corto ("4g2hx") y uint32_t
    static uint32_t encodeShortId(const char* shortCode);
    static void decodeShortId(uint32_t id, char outCode[7]);
    static uint32_t hashPathToShortId(const char* scriptPath);

    // Registro / Baja de tareas (actualiza slot en RAM y persiste línea en crontab.idx)
    uint32_t scheduleDaily(const char* scriptPath, uint8_t hour, uint8_t minute, uint8_t dowMask = 0x7F, const char* customId = nullptr);
    uint32_t scheduleEvery(const char* scriptPath, uint32_t intervalSec, bool useEpoch = false, const char* customId = nullptr);
    uint32_t scheduleAtEpoch(const char* scriptPath, uint32_t targetEpoch, const char* customId = nullptr);
    uint32_t scheduleInSeconds(const char* scriptPath, uint32_t delaySec, const char* customId = nullptr);

    bool removeById(uint32_t scriptId);
    bool triggerNow(uint32_t scriptId); // Resolución inmediata en disco y ejecución

private:
    CronLauncher() = default;
    bool resolveAndLaunch(uint32_t scriptId);

    CronSlot m_slots[kMaxSlots]{};
    uint32_t m_pendingIds[4]{}; // Pequeña cola FIFO de 4 uint32_t si LuaRunner está ocupado
    uint8_t  m_pendingCount{0};
};

} // namespace cron
} // namespace cbdos
```

---

## 4. Mecanismo de Resolución Diferida en Disco (`crontab.idx`)

El archivo `/sdcard/config/crontab.idx` es una tabla de texto plano (una línea por tarea) que **solo se abre y se lee cuando una tarea se registra o cuando su reloj llega a cero**:

```text
# ID    | MODE | PARAM1     | PARAM2 | DOW  | RUTA O COMANDO
4g2hx   | D    | 03:00      | 0      | 0x7F | /sdcard/scripts/backup_mikrotik.lua
9k1mz   | E    | 900        | 0      | 0x7F | /sdcard/scripts/check_uart_sensor.lua
7p3qa   | A    | 1761026400 | 0      | 0x7F | /sdcard/apps/alerta_red.luapp
```

### Flujo Paso a Paso durante la Ejecución (`resolveAndLaunch`)
1. **En el loop (`tick`):**
   - `CronLauncher` ve que `currentUptimeSec >= m_slots[1].nextFire` para el `scriptId` que representa `"9k1mz"`.
   - Reprograma inmediatamente `m_slots[1].nextFire += 900` (operación de 1 ciclo de CPU).
2. **Resolución bajo demanda:**
   - Llama a `resolveAndLaunch(0x... /* 9k1mz */)`.
   - Abre `/sdcard/config/crontab.idx` en modo lectura secuencial, busca la línea que empieza por `9k1mz` y extrae la ruta `"/sdcard/scripts/check_uart_sensor.lua"` en un buffer local temporal (stack).
   - Cierra el archivo inmediatamente.
3. **Ejecución en `LuaRunner`:**
   - Si `LuaRunner::getInstance().getState() != LuaRunnerState::RUNNING`, invoca `LuaRunner::getInstance().startScript(path)`.
   - Si `LuaRunner` está ocupado corriendo otro script, guarda el entero `uint32_t scriptId` en `m_pendingIds` para lanzarlo en cuanto `LuaRunner` quede `IDLE`.

---

## 5. Integración con Scripting (`LuaBridge` — `cbdos.cron.*`) y Cabeceras `@cron`

### 5.1. API desde Lua (`cbdos.cron` / alias global `cron`)
```lua
-- 1. Programar por hora diaria (Lunes a Viernes a las 08:30) -> devuelve el código corto ej. "4g2hx"
local id = cron.daily("08:30", "/sdcard/scripts/reporte_lan.lua", "mon-fri")
print("Tarea agendada con indicador: " .. id)

-- 2. Programar por intervalo en segundos (cada 10 minutos = 600s)
local id2 = cron.every(600, "/sdcard/scripts/ping_watchdog.lua")

-- 3. Programar cuenta regresiva única (en 45 segundos)
local id3 = cron.after(45, "/sdcard/scripts/payload_hid.lua")

-- 4. Programar en fecha/hora exacta ("YYYY-MM-DD HH:MM:SS")
local id4 = cron.at("2026-10-21 22:00:00", "/sdcard/scripts/apagar_nodo.lua")

-- 5. Ejecutar o eliminar usando únicamente el indicador corto de 5 caracteres
cron.run("4g2hx")
cron.remove("4g2hx")

-- 6. Consultar los slots activos en RAM (devuelve tabla ligera con id, next_fire, mode)
local list = cron.list()
```

### 5.2. Auto-Registro por Cabecera en Archivos `.lua` / `.luapp`
Cuando el usuario coloca un script en `/sdcard/scripts/` o `/sdcard/apps/`, puede incluir metadatos opcionales en el bloque inicial:
```lua
--[[
@name: Watchdog Gateway
@id: wdg01
@cron_daily: 04:00
@cron_every: 1800
]]--
```
Al escanear o importar scripts desde la UI, `CronLauncher` registra el indicador `wdg01` en `crontab.idx` y carga únicamente los 16 bytes numéricos en `m_slots`.

### 5.3. Comandos por Consola Serie / Remoto (LoRa / MeshCore)
Gracias a los indicadores cortos de 5 caracteres, se añade soporte directo por CLI serie (`/dev/ttyACM0`):
- `CBDOS:CRON:LIST` $\rightarrow$ Lista los indicadores en RAM (`4g2hx`, `9k1mz`, segundos restantes).
- `CBDOS:CRON:RUN:4g2hx` $\rightarrow$ Resuelve `4g2hx` en disco y lanza el script al instante.
- `CBDOS:CRON:DEL:4g2hx` $\rightarrow$ Desactiva y elimina el slot `4g2hx`.

---

## 6. Diseño de Interfaz en LVGL 9.5 (Pestaña / Modal en `LuaRunnerView`)

Para no saturar el Dashboard con un ícono nuevo innecesario, la gestión visual de tareas programadas se integra dentro de **`LuaRunnerView`** (o mediante un botón de acción rápida en su `HeaderBar`):
1. **Lista Compacta de Indicadores:** Muestra cada slot activo con su badge `[4g2hx]`, el contador regresivo o la hora programada (`03:00 AM` / `en 12m 30s`), y un switch LVGL 9.5 (`lv_switch`) para pausar/activar el slot.
2. **Detalle bajo demanda:** Solo cuando el usuario toca la fila `[4g2hx]`, la vista lee la línea correspondiente de `crontab.idx` para mostrar la ruta completa del script y los botones **"Ejecutar Ahora"** o **"Eliminar"**.

---

## 7. Plan de Implementación por Fases

1. **Fase 1 — Núcleo `CronLauncher` (`core/include/cbdos/cron.hpp` y `core/src/system/CronLauncher.cpp`):**
   - Implementar codificador/decodificador Base36 (`encodeShortId` / `decodeShortId`) y hash CRC32 para indicadores cortos de 5-6 caracteres.
   - Implementar el arreglo estático `CronSlot m_slots[32]` (512 bytes totales) y el lector/escritor bajo demanda de `/sdcard/config/crontab.idx`.
2. **Fase 2 — Enganche Reactivo al Reloj del Sistema y `LuaRunner`:**
   - Conectar `CronLauncher::getInstance().tick(...)` al pulso de segundo/minuto del sistema y enlazar `resolveAndLaunch(scriptId)` con `LuaRunner::getInstance().startScript()`.
3. **Fase 3 — Bindings en Lua (`LuaBindings_System.cpp` o `LuaBindings_Cron.cpp`) y Comandos Serie:**
   - Exponer la tabla `cbdos.cron.*` (`daily`, `every`, `after`, `at`, `run`, `remove`, `list`) y los comandos `CBDOS:CRON:*` por consola interactiva.
4. **Fase 4 — Integración Visual en `LuaRunnerView` (LVGL 9.5):**
   - Añadir vista/modal de gestión de slots programados y botón para agendar cualquier script de `/sdcard/scripts/` con un par de toques.
