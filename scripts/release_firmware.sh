#!/usr/bin/env bash
# Empaqueta firmwares CBDos para GitHub Releases — 100% local, sin GitHub Actions.
#
# Lee la versión desde core/include/cbdos/board_identity.hpp, compila (opcional),
# fusiona a merged-bin único @0x0 y genera el .ini hermano con sha256:
#   dist/cbdos-<board_id>-<soc>-v<ver>-merged.bin
#   dist/cbdos-<board_id>-<soc>-v<ver>.ini
#
# Uso:
#   ./scripts/release_firmware.sh [--version X.Y.Z] [--no-build] [--upload] [--tag]
#
#   --version  Sobrescribe la versión (por defecto: la de board_identity.hpp)
#   --no-build Omite idf.py/pio (empaqueta los build/ ya existentes)
#   --upload   Crea el Release en GitHub y sube dist/* con `gh`
#   --tag      Crea y pushea el tag v<ver> antes de subir (implica --upload)
#
# Requisitos: ESP-IDF exportado (. $HOME/esp/esp-idf/export.sh), platformio (pio),
# esptool.py y gh (solo para --upload).

set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIST="$ROOT/dist"
IDENTITY="$ROOT/core/include/cbdos/board_identity.hpp"

VERSION=""
NO_BUILD=0
UPLOAD=0
TAG=0

while [ $# -gt 0 ]; do
  case "$1" in
    --version=*) VERSION="${1#*=}" ;;
    --version) VERSION="${2:?--version necesita valor}"; shift ;;
    --no-build) NO_BUILD=1 ;;
    --upload) UPLOAD=1 ;;
    --tag) TAG=1; UPLOAD=1 ;;
    -h|--help) sed -n '2,20p' "$0"; exit 0 ;;
    *) echo "Arg desconocido: $1 (usa --help)"; exit 2 ;;
  esac
  shift
done

if [ -z "$VERSION" ]; then
  VERSION="$(grep -oP 'const char\* version\(\) \{ return "\K[^"]+' "$IDENTITY")"
fi
echo "==> CBDos release local v$VERSION"

build_p4() {
  echo "==> [P4] compilando bsp/esp32_p4_jc4880…"
  (cd "$ROOT/bsp/esp32_p4_jc4880" && idf.py build)
}

build_s3() {
  echo "==> [S3] compilando bsp/esp32_s3_jc3248…"
  (cd "$ROOT" && pio run -d bsp/esp32_s3_jc3248)
}

merge_p4() {
  local build="$ROOT/bsp/esp32_p4_jc4880/build"
  local out="$DIST/cbdos-jc4880p443-esp32-p4-v${VERSION}-merged.bin"
  echo "==> [P4] fusionando a $(basename "$out") (offsets de flasher_args.json)…"
  (cd "$build" && python3 - "$out" <<'EOF'
import json, subprocess, sys
out = sys.argv[1]
args = json.load(open('flasher_args.json'))
# flasher_args.json: flash_files = {offset: archivo}
parts = []
for o, f in args['flash_files'].items():
    parts += [o, f]
subprocess.run(['esptool', '--chip', 'esp32p4', 'merge-bin', '-o', out] + parts,
               check=True)
EOF
  )
  python3 "$ROOT/scripts/make_release_ini.py" "$out" jc4880p443 esp32-p4 "$VERSION"
}

merge_s3() {
  local dir="$ROOT/bsp/esp32_s3_jc3248/.pio/build/esp32s3"
  local out="$DIST/cbdos-jc3248w535-esp32-s3-v${VERSION}-merged.bin"
  echo "==> [S3] fusionando a $(basename "$out")…"
  esptool --chip esp32s3 merge-bin -o "$out" \
    0x0 "${dir}/bootloader.bin" \
    0x8000 "${dir}/partitions.bin" \
    0x10000 "${dir}/firmware.bin"
  python3 "$ROOT/scripts/make_release_ini.py" "$out" jc3248w535 esp32-s3 "$VERSION"
}

mkdir -p "$DIST"
if [ "$NO_BUILD" -eq 0 ]; then
  build_p4
  build_s3
fi
merge_p4
merge_s3

echo "==> Listo en dist/:"
ls -la "$DIST" | grep "v${VERSION}"

if [ "$TAG" -eq 1 ]; then
  echo "==> Creando tag v$VERSION…"
  (cd "$ROOT" && git tag "v$VERSION" && git push origin "v$VERSION")
fi

if [ "$UPLOAD" -eq 1 ]; then
  echo "==> Subiendo a GitHub Release v$VERSION…"
  (cd "$ROOT" && gh release create "v$VERSION" dist/cbdos-*-v"$VERSION"-merged.bin dist/cbdos-*-v"$VERSION".ini \
    --title "CBDos v$VERSION" --notes "Firmwares merged-bin @0x0 + .ini (soc+board_id). Ver guía en el sitio #/flasheo.")
fi
