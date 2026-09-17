# Plan de Internacionalización (i18n): FlasherView

Este documento detalla el plan específico para aplicar la macro `cbdos::lang::tr()` al archivo `FlasherView.cpp`.

## Análisis del Archivo
- **Archivo:** `core/src/ui/views/FlasherView.cpp`
- **Líneas aproximadas:** 800+
- **Complejidad:** Alta. Contiene listas de pines (TX, RX, BOOT, RST), textos de botones, etiquetas de información de estado, y menús desplegables para templates de hardware.

## Estrategia de Traducción
1. **Inyección de Dependencias:**
   - Añadir `#include "cbdos/language.hpp"` al inicio del archivo.
2. **Traducción de Cadenas Fijas (UI Estática):**
   - El título heredado de `BaseView` y fijado en `HeaderBar`.
   - Textos de tarjetas como `"Plantilla / Preset de Hardware:"`, `"Guia y Diagrama de Conexion:"` y `"Configuración de Pines y UART:"`.
3. **Traducción de Arrays Estáticos (Pines/Opciones):**
   - Extraer las descripciones de pines (ej. `"TX: GPIO 32..."`) si es necesario, o mantenerlas estáticas si se consideran nombres técnicos universales. *Nota: Se recomienda mantener los nombres de pines como texto estático (sin traducir) para evitar sobrecargar la memoria con identificadores `StrId` innecesarios para términos técnicos como "GPIO".*
4. **Validación de Memoria:**
   - Debido al uso intensivo de `lv_dropdown_set_options`, las cadenas generadas dinámicamente con `\n` deben ser manejadas con cuidado.

## Identificadores (StrId) a utilizar
Los siguientes `StrId` ya fueron registrados en `language.hpp` para esta vista:
- `STR_FLASH_TITLE`
- `STR_FLASH_TARGET`
- `STR_FLASH_FIRMWARE`
- `STR_FLASH_SELECT_BIN`
- `STR_FLASH_BTN_FLASH`
- `STR_FLASH_BTN_VERIFY`

## Tareas (Checklist)
- [ ] Incorporar cabecera de lenguaje.
- [ ] Sustituir títulos principales de las tarjetas.
- [ ] Sustituir los textos de los botones principales (Flashear, Verificar, Seleccionar BIN).
- [ ] Compilar para `esp32_p4` y verificar uso de memoria.
- [ ] Compilar para `esp32_s3` y verificar.
