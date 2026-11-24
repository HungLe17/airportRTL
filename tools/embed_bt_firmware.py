#!/usr/bin/env python3
"""Verify the pinned RTL8852B USB firmware and emit a kernel C++ array."""
import hashlib
from pathlib import Path
import sys

SHA256 = "c2b0d34a4ff8a8ea136993f0dc4ba657522d84231b4094c2a53cda39afc91c57"

def main():
    source, target = map(Path, sys.argv[1:])
    data = source.read_bytes()
    if hashlib.sha256(data).hexdigest() != SHA256:
        raise SystemExit("RTL8852B Bluetooth firmware SHA-256 mismatch; refusing to embed")
    if data[:8] != b"RTBTCore":
        raise SystemExit("Unexpected Bluetooth firmware format")
    target.parent.mkdir(parents=True, exist_ok=True)
    rows = [", ".join(f"0x{byte:02x}" for byte in data[n:n + 16]) for n in range(0, len(data), 16)]
    target.write_text("// Generated from verified rtl8852bu_fw.bin; do not edit.\n"
                      "extern const unsigned char rtw89_bt_firmware[] = {\n" +
                      ",\n".join(rows) + "\n};\n" +
                      f"extern const unsigned int rtw89_bt_firmware_size = {len(data)};\n", encoding="utf-8")

if __name__ == "__main__":
    main()
