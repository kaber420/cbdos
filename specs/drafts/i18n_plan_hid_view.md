# Plan de Internacionalización (i18n): HidView

Este documento detalla el plan específico para aplicar la macro `cbdos::lang::tr()` al archivo `HidView.cpp`.

## Análisis del Archivo
- **Archivo:** `core/src/ui/views/HidView.cpp`
- **Complejidad:** Media-Alta. Contiene interfaces para emulación de teclado, macros (BadUSB) y controles de StreamDeck.

## Estrategia de Traducción
1. **Inyección de Dependencias:**
   - Añadir `#include "cbdos/language.hpp"` al inicio del archivo.
2. **Traducción de Cadenas Fijas:**
   - Textos como `"Control HID / BadUSB"`, `"Probar Inyección"`, etc.
   - Textos de las pestañas o tarjetas si las hubiera.

## Identificadores (StrId) a utilizar
Los siguientes `StrId` están registrados en `language.hpp` para esta vista:
- `STR_HID_TITLE`
- `STR_HID_BTN_TEST`
- `STR_HID_TAB_KB`
- `STR_HID_TAB_PAD`
- `STR_HID_TAB_DECK`
- `STR_HID_BTN_CLEAR`
- `STR_HID_KB_PH`
- `STR_HID_PAD_HINT`

## Tareas (Checklist)
- [x] Incorporar cabecera de lenguaje.
- [x] Sustituir título de la vista y botones.
- [x] Revisar si hay otras cadenas estáticas de interfaz que requieran nuevos identificadores.
- [x] Compilar y verificar consumo de memoria.

