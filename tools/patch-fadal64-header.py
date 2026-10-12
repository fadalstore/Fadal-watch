#!/usr/bin/env python3
import os
import struct
import subprocess
import sys
from pathlib import Path

MAX_MEMORY_SIZE = 64 * 1024 * 1024

if len(sys.argv) != 3:
    raise SystemExit("usage: patch-fadal64-header.py PAYLOAD_BIN PAYLOAD_ELF")
path = Path(sys.argv[1])
elf_path = Path(sys.argv[2])
data = bytearray(path.read_bytes())
if len(data) < 32 or data[:8] != b"46LDF\x00\x00\x00":
    raise SystemExit("error: invalid FDL64 payload header")
version, header_size = struct.unpack_from("<II", data, 8)
entry_offset = struct.unpack_from("<Q", data, 24)[0]
if (version != 2 or header_size != 32 or entry_offset < header_size or
        entry_offset >= len(data)):
    raise SystemExit("error: unsupported FDL64 v2 header layout")

nm = os.environ.get("NM", "nm")
symbols = subprocess.run([nm, "-n", str(elf_path)], check=True, text=True,
                         capture_output=True).stdout.splitlines()
symbol_values = {}
for line in symbols:
    fields = line.split()
    if len(fields) >= 3 and fields[-1] in {"__payload_bss_start", "__payload_mem_end"}:
        symbol_values[fields[-1]] = int(fields[0], 16)

bss_start = symbol_values.get("__payload_bss_start")
memory_size = symbol_values.get("__payload_mem_end")
if bss_start is None or memory_size is None:
    raise SystemExit("error: linker did not export the payload memory contract")
if bss_start < len(data) or memory_size <= bss_start or memory_size > MAX_MEMORY_SIZE:
    raise SystemExit("error: invalid payload BSS or runtime-memory extent")

# In v2 the existing image_size field describes the complete in-memory image,
# including the zero-filled tail; the file length remains the filesystem size.
struct.pack_into("<Q", data, 16, memory_size)
path.write_bytes(data)
print(f"patched FDL64 memory_size={memory_size} bytes (file={len(data)} bytes): {path}")
