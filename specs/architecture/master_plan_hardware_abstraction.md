# Plan Maestro: Arquitectura de Abstracción de Hardware y Modularidad (CBDos)

**Estado:** Plan Maestro Consolidado (Borrador)  
**Objetivo:** Unificar las especificaciones de Device Tree (CDTc), Universal Resource Manager (URM) y Backpack Manager en una única hoja de ruta coherente y por fases. **No contiene código, solo especificaciones y planificación.**

---

## 1. Visión Global Unificada

La meta es dotar a CBDos de una capa de abstracción de hardware que sea:
- **Segura por diseño (Zero Trust):** Un cortafuegos de hardware estricto.
- **Rápida y Eficiente:** Cero alocaciones de memoria (heap) durante el arranque temprano.
- **Dinámica y Modular:** Soporte Plug & Play para mochilas NFC sin recompilar el núcleo.
- **Distribución Descentralizada (Estilo CoreELEC):** Capacidad de soportar nuevas placas permitiendo a usuarios no técnicos inyectar un archivo `.dtb` compilado en una partición de la memoria flash, sin necesidad de compilar código fuente en C++.

Para lograr esto, tres subsistemas trabajarán en conjunto:
1. **CDTc (Compiled Device Tree):** La fuente de la verdad estática.
2. **URM (Universal Resource Manager):** El árbitro de memoria y conflictos.
3. **BackpackManager:** El orquestador dinámico de periféricos modulares.

---

## 2. Definición de Subsistemas

### 2.1 Compiled Device Tree (CDTc)
En lugar de parsear JSON en tiempo de ejecución (lo cual fragmenta la memoria RAM temprana), CBDos usará un modelo declarativo en tiempo de compilación.
- **Entrada:** Archivos YAML legibles (`boards/jc4880p443.yaml`). Define qué hardware tiene la placa (Display, Audio, MicroSD) y **define estrictamente el pool de pines permitidos para expansión (`JP1_ALLOWED_PINS`)**.
- **Modelo Híbrido (Salida Dual):** El script (`cdtc.py`) convierte este YAML en dos artefactos para distintos casos de uso:
  1. **Placas Oficiales (Seguridad y Fallback):** Cabeceras C++ con estructuras `constexpr`. El OS oficial lleva esto compilado para garantizar un arranque a prueba de balas en 0 milisegundos.
  2. **Placas Genéricas/Comunidad:** Un binario súper compacto `.dtb` (Device Tree Blob) que los usuarios cargan en una partición flash (ej. `SPIFFS` o NVS). 
- **Lógica de Arranque:** El OS busca primero si el usuario ha inyectado un `.dtb` válido en la partición dedicada. Si lo encuentra, lo usa. Si no lo encuentra o está corrupto, el OS usa su configuración oficial en `.h` como *salvavidas* (Fallback) para no quedar inutilizado.

### 2.2 Universal Resource Manager (URM)
Evolución del Gestor de GPIOs. Actúa como el Kernel de recursos de la placa.
- **Alcance:** No solo gestiona pines aislados, sino buses enteros (SPI2, I2C0), direcciones I2C y canales de periféricos (LEDC, DMA).
- **Hardware Firewall:** 
  - El URM nace conociendo el CDTc. 
  - Sabe qué pines son "sagrados" (Pantalla, Audio).
  - Si un subsistema o mochila pide un pin fuera de su pool permitido (`JP1`), el URM *ni siquiera reconoce la petición*. Acceso denegado instántaneo.
- **Ciclo de Vida (RAII):** Si un proceso o app Lua que pidió un recurso crashea, el URM recupera automáticamente la propiedad de ese recurso y lo devuelve a su estado seguro (Hi-Z).

### 2.3 Backpack Manager y Seguridad NFC
El encargado de transformar CBDos en una consola modular (Hot-Plug).
- **Descriptor Compacto:** Lee un payload MsgPack/CBOR del tag NFC de la mochila.
- **Seguridad (Firma/Allowlist):** El tag NFC de la mochila **debe** estar firmado o pertenecer a una lista blanca de identificadores conocidos. No se confía en tags arbitrarios para prevenir ataques RCE (ej. pedir pines de *strapping* para brickear la consola).
- **Flujo de Asignación:** El Backpack Manager decodifica el tag, solicita los pines al URM (los cuales pasan por el Hardware Firewall). Si el URM aprueba (pines libres y dentro del pool), se instancian los drivers de comunicación (`meshcore`, Lua apps).

---

## 3. Plan de Ejecución por Fases (Hoja de Ruta)

Este plan desglosa la construcción de la arquitectura paso a paso. Cada fase debe estar completamente especificada y probada conceptualmente antes de avanzar a la siguiente.

### Fase 0: Extracción de Configuración Estática (Codegen)
*El objetivo es limpiar el código fuente de "números mágicos" sin alterar la lógica de arranque actual.*
- **Tarea 0.1:** Definir el esquema (Schema) exacto del archivo `board.yaml`.
- **Tarea 0.2:** Especificar el funcionamiento del script `cdtc.py` (cómo leerá YAML y qué estructuras C++ o binarios `.dtb` generará).
- **Tarea 0.3:** Especificar cómo los BSPs actuales (`hal_uart_p4.cpp`, `hal_uart_s3.cpp`) reemplazarán sus listas duras por llamadas a la nueva estructura generada `board_config_generated.h`.
- **Tarea 0.4:** Definir el mecanismo de inyección para usuarios no técnicos: cómo cargar el `.dtb` a una partición de la flash de forma fácil (ej. herramienta web o esptool) y cómo el SO temprano leerá esa partición antes de arrancar los drivers.

### Fase 1: Núcleo del Universal Resource Manager (URM)
*Construir el árbitro antes de que alguien lo necesite.*
- **Tarea 1.1:** Diseñar las interfaces C++ del URM (`claimGpio`, `claimBus`, `claimI2CAddr`).
- **Tarea 1.2:** Definir el modelo de propiedad `ResourceHandle` (RAII) para la liberación segura.
- **Tarea 1.3:** Especificar la secuencia de arranque temprana: cómo el URM lee el CDTc (Fase 0) en `app_main` y auto-bloquea los pines del sistema base (Hardware Firewall).

### Fase 2: Integración de Drivers y Capa Lua
*Migrar los consumidores internos al nuevo árbitro.*
- **Tarea 2.1:** Especificar cómo los drivers (`hal_display`, `hal_audio`) pedirán sus recursos al URM en lugar de inicializar GPIOs directamente.
- **Tarea 2.2:** Definir los bindings de Lua (`cbdos.gpio`) para que las apps pasen por el URM con cuotas limitadas (evitando *squatting* de recursos).

### Fase 3: Ecosistema de Mochilas (Backpack Manager)
*Habilitar el hardware dinámico y la seguridad NFC.*
- **Tarea 3.1:** Definir el esquema binario exacto (CBOR/MsgPack) que irá escrito en los tags NFC.
- **Tarea 3.2:** Especificar el modelo de validación de seguridad (Firmas Ed25519 vs Listas Blancas).
- **Tarea 3.3:** Diseñar la máquina de estados del `BackpackManager` (Attach, Negotiate, Power-On, Detach, Hi-Z Fallback).

---

## 4. Criterios de Aceptación Globales
- **Cero JSON en Runtime:** Ninguna librería de parsing de texto debe ejecutarse durante el boot de recursos críticos.
- **Aislamiento Total:** Un cortocircuito lógico o petición maliciosa desde un tag NFC nunca debe apagar la pantalla ni colgar el bus SPI principal del OS.
- **Multi-Target:** El diseño debe aplicar por igual para la placa JC4880 (P4) y JC3248 (S3).
