# Borrador Técnico: Señalización Bidireccional y Control de Flujo mediante Estados de Teclado USB HID

**Documento:** `docs/drafts/draft_hid_led_feedback_synchronization.md`  
**Estado:** Borrador de Arquitectura / Análisis Técnico  
**Módulo:** CBDos Core HID / Lua Engine  

---

## 1. Introducción y Concepto General

En entornos donde un microcontrolador interactúa con un sistema anfitrión (Host) exclusivamente como un dispositivo de entrada estándar (USB HID Keyboard), la comunicación es predominantemente unidireccional (Host 🠄 Dispositivo).

Sin embargo, el estándar USB HID contempla un canal de retorno nativo: los **Output Reports** que el Host envía al teclado para sincronizar el estado de los indicadores luminosos (`Num Lock`, `Caps Lock`, `Scroll Lock`, `Compose`, `Kana`).

Este mecanismo permite implementar un protocolo de señalización y control de flujo basado en eventos, permitiendo que scripts de automatización tomen decisiones dinámicas (bifurcaciones `if/else`, reintentos o métodos alternativos) según las respuestas del sistema anfitrión.

---

## 2. Arquitectura del Protocolo de Señalización

```
┌──────────────────────────────┐              ┌──────────────────────────────┐
│       ESP32 (CBDos)          │              │        Host (PC / OS)        │
│                              │              │                              │
│  1. Inyección de comando     │ ── USB IN ──>│  2. Ejecución de tarea       │
│     (Verificar condición)    │ (Key Events) │     (Evaluación de estado)   │
│                              │              │                              │
│  4. Lectura de estado        │ <── USB OUT ─│  3. Conmutación de indicador │
│     (hid.get_leds() / Event) │ (SET_REPORT) │     (setleds / API de OS)    │
│                              │              │                              │
│  5. Decisión de flujo:       │              │                              │
│     - Condición OK -> Paso 2 │              │                              │
│     - Timeout / Fallo -> Alt │              │                              │
└──────────────────────────────┘              └──────────────────────────────┘
```

### Componentes del Ciclo

1. **Estímulo / Solicitud (Host Input):**
   El dispositivo CBDos tipea o ejecuta una instrucción en la interfaz de línea de comandos o terminal del Host.
2. **Evaluación en el Anfitrión:**
   El comando evalúa una condición (ejemplo: verificación de presencia de archivos, conectividad de red o nivel de privilegios).
3. **Respuesta por Canal de Indicadores (Host Output):**
   Si la condición se cumple, el Host conmuta un indicador específico (por ejemplo, alternar `Caps Lock` o `Scroll Lock`).
4. **Captura en el Microcontrolador:**
   El driver USB (TinyUSB) procesa el evento `SET_REPORT` mediante la función de callback `tud_hid_set_report_cb()` y actualiza el registro interno de estado.
5. **Evaluación Lógica en Lua:**
   El script en CBDos consulta el estado actualizado mediante `hid.get_leds()` o espera un evento con tiempo límite (`timeout`), decidiendo si continúa el flujo principal o ejecuta una rama de contingencia.

---

## 3. Asignación de Indicadores Estándar (Usage Page 0x08)

El reporte de salida estándar de teclado USB define los siguientes bits:

| Bit | Identificador | Uso Convencional | Aplicación en Señalización |
| :---: | :--- | :--- | :--- |
| `Bit 0` | **Num Lock** | Bloqueo del teclado numérico | Bandera de estado / ACK general |
| `Bit 1` | **Caps Lock** | Bloqueo de mayúsculas | Bandera de éxito / Condición A |
| `Bit 2` | **Scroll Lock** | Bloqueo de desplazamiento | Señal de sincronización / Siguiente paso |
| `Bit 3` | **Compose** | Composición de caracteres (UNIX) | Bandera auxiliar |
| `Bit 4` | **Kana** | Modo de entrada japonés | Bandera auxiliar |

---

## 4. Patrón de Control de Flujo en Lua

Un patrón conceptual en Lua para manejar branching dinámico se estructura de la siguiente manera:

```lua
-- Función auxiliar para esperar una bandera con timeout
function esperar_confirmacion(indicador, timeout_ms)
    local inicio = sys.millis()
    while (sys.millis() - inicio) < timeout_ms do
        local leds = hid.get_leds()
        if leds[indicador] == true then
            return true
        end
        cbdos.sleep(50)
    end
    return false
end

-- 1. Intentar método principal
print("[Flujo] Evaluando condición inicial en el host...")
-- Enviar comando al host que active ScrollLock si la condición se cumple
-- ...

-- 2. Esperar confirmación (ejemplo: ScrollLock en menos de 2000 ms)
if esperar_confirmacion("scrolllock", 2000) then
    print("[Flujo] Condición confirmada. Procediendo con flujo principal...")
    -- Continuar con la tarea principal
else
    print("[Flujo] Tiempo agotado o fallo. Activando flujo alternativo...")
    -- Ejecutar método secundario o recuperación de errores
end
```

---

## 5. Ventajas y Limitaciones

### Ventajas
- **Cero dependencias de red:** Opera íntegramente sobre el canal USB físico.
- **Sin drivers adicionales:** Utiliza únicamente la clase estándar USB HID soportada nativamente por todos los sistemas operativos principales.
- **Baja latencia:** La propagación de un `SET_REPORT` desde el Host hacia el microcontrolador toma habitualmente entre 10 y 50 ms.

### Limitaciones
- **Ancho de banda bajo:** Diseñado para señalización de control (banderas booleanas o pequeños estados), no para transferencia masiva de datos.
- **Interferencia en la sesión de usuario:** La alteración de `Caps Lock` o `Num Lock` puede afectar la entrada manual del usuario mientras el flujo está activo, requiriendo restaurar los estados al finalizar.
