#!/usr/bin/env python3
import struct
import sys
from pathlib import Path

if len(sys.argv) != 2:
    raise SystemExit("usage: patch-fadal64-header.py PATH")
path = Path(sys.argv[1])
data = bytearray(path.read_bytes())
if len(data) < 32 or data[:8] != b"46LDF\x00\x00\x00":
    raise SystemExit("error: invalid FDL64 payload header")
struct.pack_into("<Q", data, 16, len(data))
path.write_bytes(data)
print(f"patched FDL64 image_size={len(data)} bytes: {path}")
