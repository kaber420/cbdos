# 📻 Especificación Técnica: Sistema de Listas de Reproducción Multi-Lista y Almacenamiento Portable

## 📌 1. Visión y Fundamento Arquitectónico

En **CBDos**, el sistema de audio y Radio Web evoluciona de un modelo simple de una única lista fija ("Favoritos") hacia un **ecosistema de colecciones y listas de reproducción múltiples y portables**, diseñado bajo dos principios fundamentales:

1. **Memoria Flash Interna como Almacenamiento Principal (Offline-First Autónomo):**
   * Todas las listas de reproducción, favoritos y configuraciones del usuario residen primariamente en la memoria Flash interna (`/flash` o almacenamiento local de datos).
   * El dispositivo es **100% autónomo**: no requiere una tarjeta MicroSD insertada para encender, reproducir emisoras, crear listas o guardar cambios.
2. **Tarjeta MicroSD como Medio de Intercambio y Portabilidad (Hot-Plug & Cartucho de Listas):**
   * La tarjeta MicroSD actúa como un medio portable de transporte: permite exportar cualquier lista a un archivo estándar (ej. `/sdcard/playlists/synthwave.json` o `.m3u`) y llevarla a cualquier otro CyberDeck o reproductor sin tener que reescribir URLs ni nombres a mano.
   * Al insertar una MicroSD con listas externas, el sistema las detecta y permite importarlas a la memoria Flash interna o reproducirlas directamente.

---

## 🏛️ 2. Modelo de Datos de Listas de Reproducción

```
┌────────────────────────────────────────────────────────────────────────┐
│                        RadioPlaylistManager (Core)                     │
├────────────────────────────────────────────────────────────────────────┤
│  [Playlist 0: "Favoritos"] (Fija / Default)                            │
│    ├── Estación 1: "SomaFM Groove Salad" (http://...)                  │
│    ├── Estación 2: "Ibiza Global Radio"  (http://...)                  │
│    └── Estación 3: "Radio Paradise"      (http://...)                  │
│                                                                        │
│  [Playlist 1: "Synthwave / Cyberpunk"]                                 │
│    ├── Estación 1: "Nightwave Plaza"     (http://...)                  │
│    └── Estación 2: "Retro Car Synth"     (http://...)                  │
│                                                                        │
│  [Playlist 2: "Noticias & Podcasts"]                                   │
│    └── Estación 1: "BBC World Service"   (http://...)                  │
└──────────────────────────────────┬─────────────────────────────────────┘
                                   │
                ┌──────────────────┴──────────────────┐
                ▼ (Persistencia Principal)            ▼ (Intercambio Portable)
     ┌────────────────────────────┐        ┌────────────────────────────┐
     │   Memoria Flash Interna    │        │      Tarjeta MicroSD       │
     │   /flash/audio/lists.json  │        │   /sdcard/playlists/*.json │
     │   (Autonomía del Sistema)  │        │   (Importación/Exportación)│
     └────────────────────────────┘        └────────────────────────────┘
```

### 2.1. Estructuras de Datos (`core/src/audio/RadioManager.hpp`)

```cpp
namespace cbdos {
namespace audio {

struct RadioStation {
    std::string name;       // Nombre de la emisora
    std::string url;        // URL del flujo HTTP/HTTPS (MP3 / AAC)
    std::string country;    // País o región (ej: "Global", "USA", "España")
    std::string genre;      // Género musical (ej: "Synthwave", "Ambient")
    int bitrate = 128;      // Bitrate estimado en kbps
    bool isFavorite = false;
};

struct RadioPlaylist {
    std::string id;         // Identificador único (ej: "fav", "pl_1740000000")
    std::string name;       // Nombre de la lista visible en UI (ej: "Favoritos", "Chill / Lofi")
    std::vector<RadioStation> stations;
    bool isDefault = false; // true si es la lista principal por defecto
};

} // namespace audio
} // namespace cbdos
```

---

## 💾 3. Estrategia de Almacenamiento y Formatos: MessagePack & M3U

### 3.1. Formato Nativo de CBDos: MessagePack (`.msgpack`)
Para la base de datos interna en Flash (`/flash/audio/playlists.msgpack`) y la exportación/importación nativa entre dispositivos CBDos, se utiliza **MessagePack binario**:
* **Cero fragmentación de memoria:** Parseo binario directo y secuencial sin sobrecarga de cadenas de texto JSON.
* **Tamaño ultra-compacto:** 40-50% menor huella en Flash y MicroSD.
* **Esquema MessagePack:**
```
[
  {
    "id": "fav",
    "name": "Favoritos",
    "def": true,
    "stations": [
      { "name": "SomaFM Groove Salad", "url": "http://ice1.somafm.com/groovesalad-128-mp3", "genre": "Ambient", "br": 128 },
      { "name": "Ibiza Global Radio", "url": "http://listento.ibizaglobalradio.com:8024/stream", "genre": "Electronic", "br": 128 }
    ]
  },
  {
    "id": "pl_synth",
    "name": "Synthwave",
    "def": false,
    "stations": [
      { "name": "Nightwave Plaza", "url": "http://radio.plaza.one/mp3", "genre": "Vaporwave", "br": 128 }
    ]
  }
]
```

### 3.2. Formato Estándar Universal de Audio: M3U / M3U8 (`.m3u`)
Para compatibilidad con listas de radio descargadas de internet, VLC, Winamp o reproductores externos:
* **Estructura M3U soportada:**
```m3u
#EXTM3U
#EXTINF:-1,SomaFM Groove Salad - Ambient
http://ice1.somafm.com/groovesalad-128-mp3
#EXTINF:-1,Ibiza Global Radio - Electronic
http://listento.ibizaglobalradio.com:8024/stream
```
* **Parser M3U en streaming:** Lee línea por línea extrayendo `#EXTINF:` (nombre/género) y la URL del stream directamente hacia una nueva `RadioPlaylist`.

---

## 🔄 4. Operaciones de Portabilidad con Tarjeta MicroSD

### 4.1. Exportación hacia MicroSD
* **Ruta de destino:** `/sdcard/playlists/<nombre_lista>.msgpack` (o `.m3u` si se elige formato estándar).
* **Acción del usuario:** El usuario selecciona la lista y pulsa *"Exportar a MicroSD"*.

### 4.2. Importación desde MicroSD
* **Detección automática (`scanPlaylistsOnSd()`):** Escanea `/sdcard/playlists/` buscando archivos `.msgpack` y `.m3u`.
* **Acción del usuario:** Al pulsar *"Importar"*, el usuario ve la lista de archivos encontrados en la SD y, al seleccionar uno, se importa de inmediato a la Flash interna.

---

## 🎨 5. Flujos de Interfaz de Usuario (LVGL 9.5)

### 5.1. Selector de Listas de Reproducción
* En la cabecera de la vista de Radio o en la barra de pestañas, se añade un selector de listas (Dropdown o Barra de Listas).
* Permite alternar de inmediato entre las diferentes listas creadas (*"Favoritos"*, *"Synthwave"*, *"Noticias"*).

### 5.2. Modal de Gestión de Listas (`ListManagerModal`)
* **Botón `+ Nueva Lista`:** Abre una ventana modal con teclado para asignar un nombre y crear una lista en blanco.
* **Botón `💾 Exportar`:** Guarda la lista activa en la MicroSD.
* **Botón `📥 Importar`:** Abre la lista de archivos encontrados en `/sdcard/playlists/` para importación rápida.
* **Botón `🗑️ Eliminar`:** Elimina la lista seleccionada (protegiendo la lista por defecto *"Favoritos"* contra borrado accidental).

---

## 🛠️ 6. Plan de Implementación de la Fase

| Paso | Componente | Descripción |
| :--- | :--- | :--- |
| **1** | `core/src/audio/RadioManager.hpp` | Definir structs `RadioPlaylist`, `RadioStation` y APIs multi-lista (`createPlaylist`, `deletePlaylist`, `exportToSd`, `importFromSd`). |
| **2** | `core/src/audio/RadioManager.cpp` | Implementar parser y generador JSON agnóstico sobre `cbdos::storage` (eliminando `<ArduinoJson.h>`, `<cJSON.h>` y `<SD.h>`). |
| **3** | `core/src/ui/views/RadioView.hpp/.cpp` | Integrar selector multi-lista, modal de creación de listas y botones de Importar/Exportar MicroSD en la UI LVGL 9.5. |
| **4** | Verificación Multi-Target | Validar compilación limpia en **ESP32-P4** (`idf.py build`) y **ESP32-S3** (`pio run`). |
