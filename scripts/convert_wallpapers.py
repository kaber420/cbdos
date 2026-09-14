#!/usr/bin/env python3
"""
Convertidor de Wallpapers por Lotes para CBDos (ESP32-P4 / ESP32-S3).

Convierte imágenes (JPG, PNG, BMP, WEBP) al formato nativo LVGL 9.5 (.bin con
cabecera de 12 bytes + RGB565 little-endian) listo para usar en CBDos (/wallpapers/).

Características:
  - Presets para hardware:
      --target p4 -> 480x800 (Guition JC4880P443C / ESP32-P4) [Por defecto]
      --target s3 -> 320x480 (JC3248W535 / ESP32-S3)
  - Resolución en el nombre del archivo (ej: foto_480x800.bin)
  - Salta imágenes ya convertidas por defecto (usa --force para reescribir)
  - Admite archivos individuales, carpetas completas o patrones comodín (*.jpg)

Uso:
  python3 scripts/convert_wallpapers.py mi_foto.jpg
  python3 scripts/convert_wallpapers.py fotos/ --target p4
  python3 scripts/convert_wallpapers.py *.png --target s3 --all-formats
"""

import os
import sys
import glob
import struct
import argparse
from PIL import Image, ImageOps

SUPPORTED_EXTENSIONS = {".jpg", ".jpeg", ".png", ".bmp", ".webp"}

TARGET_PRESETS = {
    "p4": (480, 800),  # Guition JC4880P443C (4.3" MIPI-DPI)
    "s3": (320, 480),  # JC3248W535 (3.5" QSPI)
}


def rgb888_to_rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def build_lvgl9_header(width, height):
    """
    Cabecera estándar de LVGL 9 (12 bytes):
    struct lv_image_header_t {
        uint8_t magic: 0x19 (LV_IMAGE_HEADER_MAGIC)
        uint8_t cf: 0x12 (LV_COLOR_FORMAT_RGB565)
        uint16_t flags: 0
        uint16_t w: width
        uint16_t h: height
        uint16_t stride: width * 2 (2 bytes por pixel en RGB565)
        uint16_t reserved: 0
    }
    """
    return struct.pack("<BBHHHHH", 0x19, 0x12, 0, width, height, width * 2, 0)


def collect_images(inputs, default_input_dirs=("input_wallpapers", "raw_wallpapers")):
    image_paths = []

    if inputs:
        for item in inputs:
            # Expandir posibles comodines en caso de que la shell no los expanda
            matches = glob.glob(item)
            candidates = matches if matches else [item]

            for cand in candidates:
                if os.path.isdir(cand):
                    for root, _, files in os.walk(cand):
                        for f in files:
                            ext = os.path.splitext(f)[1].lower()
                            if ext in SUPPORTED_EXTENSIONS:
                                image_paths.append(os.path.join(root, f))
                elif os.path.isfile(cand):
                    ext = os.path.splitext(cand)[1].lower()
                    if ext in SUPPORTED_EXTENSIONS:
                        image_paths.append(cand)
                    else:
                        print(f"[AVISO] Extensión no soportada ignorada: {cand}")
                else:
                    print(f"[AVISO] Archivo o directorio no encontrado: {cand}")
    else:
        # Búsqueda por defecto en carpetas sugeridas
        for d in default_input_dirs:
            if os.path.isdir(d):
                for f in os.listdir(d):
                    ext = os.path.splitext(f)[1].lower()
                    if ext in SUPPORTED_EXTENSIONS:
                        image_paths.append(os.path.join(d, f))

    # Eliminar duplicados preservando el orden
    seen = set()
    unique_paths = []
    for p in image_paths:
        abs_p = os.path.abspath(p)
        if abs_p not in seen:
            seen.add(abs_p)
            unique_paths.append(p)

    return unique_paths


def process_image(src_path, output_dir, width, height, use_suffix, force, all_formats):
    base_name = os.path.splitext(os.path.basename(src_path))[0]
    # Limpiar posibles espacios o caracteres conflictivos
    base_name = base_name.strip().replace(" ", "_")

    out_name = f"{base_name}_{width}x{height}" if use_suffix else base_name
    bin_path = os.path.join(output_dir, f"{out_name}.bin")

    if os.path.exists(bin_path) and not force:
        print(f"⏩ [SALTADO] Ya existe: {bin_path} (usa --force para reescribir)")
        return False

    try:
        with Image.open(src_path) as img:
            img = img.convert("RGB")
            # Redimensionar y recortar inteligentemente manteniendo proporción
            img_resized = ImageOps.fit(img, (width, height), method=Image.Resampling.LANCZOS)

            # Generar archivo binario nativo LVGL 9.5
            header = build_lvgl9_header(width, height)
            raw_bytes = bytearray(header)

            for y in range(height):
                for x in range(width):
                    r, g, b = img_resized.getpixel((x, y))
                    raw_bytes.extend(struct.pack("<H", rgb888_to_rgb565(r, g, b)))

            with open(bin_path, "wb") as f:
                f.write(raw_bytes)

            generated = [f"{out_name}.bin ({len(raw_bytes)} bytes)"]

            # Si se piden todos los formatos, exportar también JPG, BMP, PNG
            if all_formats:
                jpg_path = os.path.join(output_dir, f"{out_name}.jpg")
                img_resized.save(jpg_path, "JPEG", quality=95)
                generated.append(f"{out_name}.jpg")

                bmp_path = os.path.join(output_dir, f"{out_name}.bmp")
                img_resized.save(bmp_path, "BMP")
                generated.append(f"{out_name}.bmp")

                png_path = os.path.join(output_dir, f"{out_name}.png")
                img_resized.save(png_path, "PNG")
                generated.append(f"{out_name}.png")

            print(f"✅ [OK] {os.path.basename(src_path)} -> {', '.join(generated)}")
            return True

    except Exception as e:
        print(f"❌ [ERROR] Fallo al procesar {src_path}: {e}")
        return False


def main():
    parser = argparse.ArgumentParser(
        description="Convertidor de fondos de pantalla para CBDos (LVGL 9.5 RGB565 .bin)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Ejemplos:
  python3 scripts/convert_wallpapers.py foto.jpg --target p4
  python3 scripts/convert_wallpapers.py carpeta_fotos/ --target s3
  python3 scripts/convert_wallpapers.py *.jpg --target p4 --all-formats
  python3 scripts/convert_wallpapers.py foto.png --width 480 --height 800 --force
        """
    )

    parser.add_argument("inputs", nargs="*", help="Archivos de imagen, carpetas o patrones (*.jpg, *.png, etc.)")
    parser.add_argument("-t", "--target", choices=["p4", "s3"], default="p4",
                        help="Target de hardware: 'p4' (480x800) o 's3' (320x480). Default: p4")
    parser.add_argument("--width", type=int, default=None, help="Ancho manual en píxeles (anula el target)")
    parser.add_argument("--height", type=int, default=None, help="Alto manual en píxeles (anula el target)")
    parser.add_argument("-o", "--output-dir", default="wallpapers",
                        help="Directorio de destino (default: 'wallpapers')")
    parser.add_argument("--no-suffix", action="store_true",
                        help="No agregar el sufijo _ANCHOxALTO al nombre del archivo")
    parser.add_argument("-f", "--force", action="store_true",
                        help="Sobrescribir archivos si ya existen")
    parser.add_argument("--all-formats", action="store_true",
                        help="Generar también versiones .jpg, .bmp y .png además del .bin")

    args = parser.parse_args()

    # Determinar resolución
    preset_w, preset_h = TARGET_PRESETS[args.target]
    width = args.width if args.width is not None else preset_w
    height = args.height if args.height is not None else preset_h

    # Recolectar imágenes
    images = collect_images(args.inputs)

    if not images:
        print(f"No se encontraron imágenes para procesar.")
        print(f"Por favor indica las rutas de las imágenes o colócalas en 'input_wallpapers/'.")
        print(f"Ejemplo: python3 scripts/convert_wallpapers.py mi_foto.jpg --target {args.target}")
        sys.exit(1)

    os.makedirs(args.output_dir, exist_ok=True)
    use_suffix = not args.no_suffix

    print(f"=== Conversor de Wallpapers CBDos ===")
    print(f"Target: {args.target.upper()} | Resolución: {width}x{height} px")
    print(f"Directorio de salida: {args.output_dir}/")
    print(f"Sufijo de resolución: {'Activado' if use_suffix else 'Desactivado'}")
    print(f"Sobrescribir existentes: {'Sí' if args.force else 'No (saltar)'}")
    print(f"Total de imágenes a procesar: {len(images)}\n")

    success_count = 0
    skipped_count = 0
    error_count = 0

    for img_path in images:
        result = process_image(img_path, args.output_dir, width, height, use_suffix, args.force, args.all_formats)
        if result is True:
            success_count += 1
        elif result is False and not args.force and os.path.exists(
            os.path.join(args.output_dir, f"{os.path.splitext(os.path.basename(img_path))[0]}_{width}x{height}.bin" if use_suffix else f"{os.path.splitext(os.path.basename(img_path))[0]}.bin")
        ):
            skipped_count += 1
        else:
            error_count += 1

    print(f"\n--- Resumen ---")
    print(f"Convertidos con éxito: {success_count}")
    print(f"Saltados (ya existían): {skipped_count}")
    if error_count > 0:
        print(f"Errores: {error_count}")
    print(f"Archivos listos en: {os.path.abspath(args.output_dir)}/")
    print(f"Copia los archivos .bin a la carpeta /wallpapers/ de tu MicroSD.")


if __name__ == "__main__":
    main()
