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
Los siguientes `StrId` ya fueron registrados en `language.hpp` para esta vista:
- `STR_HID_TITLE`
- `STR_HID_BTN_TEST`

## Tareas (Checklist)
- [ ] Incorporar cabecera de lenguaje.
- [ ] Sustituir título de la vista y botones.
- [ ] Revisar si hay otras cadenas estáticas de interfaz que requieran nuevos identificadores.
- [ ] Compilar y verificar consumo de memoria.
