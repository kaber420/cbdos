#!/usr/bin/env python3
"""Genera el manifiesto .ini hermano de cada merged-bin.

Uso:
    python3 scripts/make_release_ini.py <bin> <board_id> <soc> <version> [--offset 0x0]

Escribe <bin-sin-ext>.ini con formato:
    [firmware]
    board_id=jc4880p443
    soc=esp32-p4
    version=0.2.4
    file=cbdos-jc4880p443-esp32-p4-v0.2.4-merged.bin
    sha256=<...>
    size=<bytes>
    flash_offset=0x0
"""
import hashlib
import os
import sys


def main() -> int:
    if len(sys.argv) < 5:
        print(__doc__)
        return 2
    bin_path, board_id, soc, version = sys.argv[1:5]
    offset = "0x0"
    for i, a in enumerate(sys.argv):
        if a == "--offset" and i + 1 < len(sys.argv):
            offset = sys.argv[i + 1]
    with open(bin_path, "rb") as f:
        data = f.read()
    sha = hashlib.sha256(data).hexdigest()
    base = os.path.basename(bin_path)
    ini_path = os.path.splitext(bin_path)[0] + ".ini"
    ini = (
        "[firmware]\n"
        f"board_id={board_id}\n"
        f"soc={soc}\n"
        f"version={version}\n"
        f"file={base}\n"
        f"sha256={sha}\n"
        f"size={len(data)}\n"
        f"flash_offset={offset}\n"
    )
    with open(ini_path, "w", encoding="utf-8") as f:
        f.write(ini)
    print(f"OK: {ini_path} (sha256={sha[:12]}… size={len(data)})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
