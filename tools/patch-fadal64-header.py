#!/usr/bin/env python3
import os
import struct
import subprocess
import sys
from pathlib import Path

PAGE_SIZE = 4096
MAX_MEMORY_SIZE = 64 * 1024 * 1024
HEADER_SIZE = 32
LAYOUT_OFFSET = HEADER_SIZE
LAYOUT_SIZE = 40
LAYOUT_MAGIC = b"FPM1"
LAYOUT_MAGIC_U32 = int.from_bytes(LAYOUT_MAGIC, "little")
SYMBOLS = {
    "fadal64_entry",
    "__payload_text_start",
    "__payload_text_end",
    "__payload_rodata_start",
    "__payload_rodata_end",
    "__payload_data_start",
    "__payload_data_end",
    "__payload_bss_start",
    "__payload_mem_end",
}

if len(sys.argv) != 3:
    raise SystemExit("usage: patch-fadal64-header.py PAYLOAD_BIN PAYLOAD_ELF")
payload_path = Path(sys.argv[1])
elf_path = Path(sys.argv[2])
data = bytearray(payload_path.read_bytes())
if (len(data) < HEADER_SIZE + LAYOUT_SIZE or
        data[:8] != b"46LDF\x00\x00\x00"):
    raise SystemExit("error: invalid FDL64 payload header or missing layout descriptor")
version, header_size = struct.unpack_from("<II", data, 8)
if version != 3 or header_size != HEADER_SIZE:
    raise SystemExit("error: expected FDL64 v3 with the fixed 32-byte header")
if data[LAYOUT_OFFSET:LAYOUT_OFFSET + 4] != LAYOUT_MAGIC:
    raise SystemExit("error: missing FPM1 descriptor immediately after the FDL64 header")
if struct.unpack_from("<I", data, LAYOUT_OFFSET + 4)[0] != 1:
    raise SystemExit("error: unsupported FPM1 descriptor version")

nm = os.environ.get("NM", "nm")
symbol_lines = subprocess.run([nm, "-n", str(elf_path)], check=True,
                              text=True, capture_output=True).stdout.splitlines()
symbol_values = {}
for line in symbol_lines:
    fields = line.split()
    if len(fields) >= 3 and fields[-1] in SYMBOLS:
        symbol_values[fields[-1]] = int(fields[0], 16)
missing = SYMBOLS - symbol_values.keys()
if missing:
    raise SystemExit("error: linker omitted payload layout symbols: " + ", ".join(sorted(missing)))

entry = symbol_values["fadal64_entry"]
text_start = symbol_values["__payload_text_start"]
text_end = symbol_values["__payload_text_end"]
rodata_start = symbol_values["__payload_rodata_start"]
rodata_end = symbol_values["__payload_rodata_end"]
data_start = symbol_values["__payload_data_start"]
data_end = symbol_values["__payload_data_end"]
bss_start = symbol_values["__payload_bss_start"]
memory_size = symbol_values["__payload_mem_end"]
page_bounds = (text_start, text_end, rodata_start, rodata_end,
               data_start, data_end, bss_start, memory_size)

if memory_size < len(data) or memory_size > MAX_MEMORY_SIZE:
    raise SystemExit("error: invalid FDL64 runtime-memory extent")
if any(value > 0xffffffff for value in page_bounds):
    raise SystemExit("error: FPM1 segment offset exceeds its 32-bit format")
if any(value % PAGE_SIZE for value in page_bounds):
    raise SystemExit("error: linker segment boundaries are not 4 KiB aligned")
if (text_start < PAGE_SIZE or not text_start < text_end or
        text_end != rodata_start or not rodata_start < rodata_end or
        rodata_end != data_start or not data_start < data_end or
        data_end != bss_start or not bss_start < memory_size):
    raise SystemExit("error: payload segments are empty, overlapping, or not contiguous")
if entry < text_start or entry >= text_end:
    raise SystemExit("error: payload entry point is outside the executable text range")
if len(data) != data_end:
    raise SystemExit("error: payload file must end at the initialized-data/BSS boundary")

struct.pack_into("<Q", data, 16, memory_size)
struct.pack_into("<Q", data, 24, entry)
struct.pack_into("<II8I", data, LAYOUT_OFFSET,
                 LAYOUT_MAGIC_U32, 1, *page_bounds)
payload_path.write_bytes(data)
print(
    "patched FDL64 v3: "
    f"text={text_start:#x}-{text_end:#x} "
    f"rodata={rodata_start:#x}-{rodata_end:#x} "
    f"data={data_start:#x}-{data_end:#x} "
    f"bss={bss_start:#x}-{memory_size:#x} file={len(data)} bytes"
)
