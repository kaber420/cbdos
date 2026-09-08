#!/usr/bin/env python3
"""
Genera un wallpaper RGB565 (.bin con cabecera LVGL 9) para cargarlo
desde MicroSD (/wallpapers/) a SPIFFS (/wallpaper.bin).

Ya NO se genera ningun .c embebido en el firmware: el RGB en flash
ocupaba >700KB de particion app (768KB en P4 480x800). El firmware
solo trae animados por defecto; el RGB lo aporta el usuario:

  1. python3 scripts/set_default_wallpaper.py foto.jpg --output wallpapers/mi_fondo.bin
  2. Copiar mi_fondo.bin a la MicroSD en /wallpapers/
  3. En CBDos: Ajustes > Fondo de Pantalla > elegir el archivo.
     El sistema lo copia a LittleFS (/wallpaper.bin) y lo aplica.

Formatos aceptados en SD: .bin (LVGL RGB565 + cabecera 12B o raw),
.bmp 24-bit 320x480. Ver WallpaperManager::setWallpaper().
"""
import os
import sys
import struct
import argparse
from PIL import Image, ImageOps


def rgb888_to_rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def generate_wallpaper_bin(image_path, output_path, width, height):
    if not os.path.exists(image_path):
        print(f"Error: {image_path} does not exist!")
        return False

    print(f"Reading image: {image_path}")
    img = Image.open(image_path).convert("RGB")
    img_resized = ImageOps.fit(img, (width, height), method=Image.Resampling.LANCZOS)

    # Cabecera LVGL 9 (12 bytes): magic, cf, flags, w, h, stride, reserved
    header = struct.pack("<BBHHHHH", 0x19, 0x12, 0, width, height, width * 2, 0)
    raw = bytearray(header)
    for y in range(height):
        for x in range(width):
            r, g, b = img_resized.getpixel((x, y))
            raw.extend(struct.pack("<H", rgb888_to_rgb565(r, g, b)))

    os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)
    with open(output_path, "wb") as f:
        f.write(raw)

    print(f"OK: {output_path} ({len(raw)} bytes, {width}x{height} RGB565 + LVGL header)")
    print("Copialo a la MicroSD en /wallpapers/ y elijelo en Ajustes > Fondo de Pantalla.")
    print("El sistema lo guardara en SPIFFS como /wallpaper.bin (no en la particion del firmware).")
    return True


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description="Convierte imagen a wallpaper .bin para SD->SPIFFS")
    ap.add_argument("image", help="Imagen de entrada (jpg/png/...)")
    ap.add_argument("--output", default="wallpapers/custom_wallpaper.bin",
                    help="Archivo .bin de salida (default: wallpapers/custom_wallpaper.bin)")
    ap.add_argument("--width", type=int, default=320, help="Ancho (default 320, P4: 480)")
    ap.add_argument("--height", type=int, default=480, help="Alto (default 480, P4: 800)")
    args = ap.parse_args()
    ok = generate_wallpaper_bin(args.image, args.output, args.width, args.height)
    sys.exit(0 if ok else 1)
