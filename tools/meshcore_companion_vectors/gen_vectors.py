#!/usr/bin/env python3
"""Genera vectores binarios Fase 1 para MeshCoreClient (framing '>' + len LE)."""
import argparse
import os
import struct


def frame(payload: bytes) -> bytes:
    return b">" + struct.pack("<H", len(payload)) + payload


def contact_body(pubkey: bytes, ctype=1, flags=0, path_len=-1, name=b"Probe",
                 last_advert=1718000000, lat=41_388_000, lon=2_168_000, lastmod=1718000001) -> bytes:
    path = bytes(64)
    nm = name[:32].ljust(32, b"\x00")
    return (pubkey[:32].ljust(32, b"\x01") + struct.pack("B", ctype) + struct.pack("B", flags)
            + struct.pack("b", path_len) + path + nm
            + struct.pack("<I", last_advert) + struct.pack("<i", lat)
            + struct.pack("<i", lon) + struct.pack("<I", lastmod))


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="/tmp/meshvec")
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)
    pk1 = bytes(range(32))
    pk2 = bytes([0xAA] * 32)
    vecs = {
        "contact_start_2.bin": frame(b"\x02" + struct.pack("<I", 2)),
        "contact_1.bin": frame(b"\x03" + contact_body(pk1, name=b"Alice")),
        "contact_2.bin": frame(b"\x03" + contact_body(pk2, ctype=2, name=b"Rep-1")),
        "contact_end.bin": frame(b"\x04" + struct.pack("<I", 1718000001)),
        "advert.bin": frame(b"\x80" + contact_body(pk1, name=b"Alice")),
        "new_advert.bin": frame(b"\x8a" + contact_body(pk2, ctype=2, name=b"Rep-1")),
        "msg_sent.bin": frame(b"\x06\x00" + struct.pack("<II", 0x12345678, 15000)),
        "ack.bin": frame(b"\x82" + struct.pack("<I", 0x12345678)),
        "current_time.bin": frame(b"\x09" + struct.pack("<I", 1718000000)),
        "contact_msg_v3.bin": frame(b"\x10" + struct.pack("b", 40) + b"\x00\x00"
                                    + pk1[:6] + bytes([1, 0])
                                    + struct.pack("<I", 1718000010) + b"hola DM"),
        "channel_msg_v3.bin": frame(b"\x11" + struct.pack("b", 20) + b"\x00\x00"
                                    + bytes([0, 2, 0]) + struct.pack("<I", 1718000020) + b"hola canal"),
    }
    for name, data in vecs.items():
        with open(os.path.join(args.out, name), "wb") as f:
            f.write(data)
    print(f"wrote {len(vecs)} vectors to {args.out}")


if __name__ == "__main__":
    main()
