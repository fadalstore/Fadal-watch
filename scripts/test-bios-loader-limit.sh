#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "usage: $0 <bootable-disk-image> <boot-object> <maximum-sectors> [qemu]" >&2
    exit 2
}

[[ $# -ge 3 && $# -le 4 ]] || usage
image=$1
boot_object=$2
maximum=$3
qemu=${4:-qemu-system-i386}
[[ -s "$image" ]] || { echo "error: BIOS disk image not found: $image" >&2; exit 2; }
[[ -s "$boot_object" ]] || { echo "error: boot object not found: $boot_object" >&2; exit 2; }
[[ "$maximum" =~ ^[1-9][0-9]*$ ]] || { echo "error: maximum sectors must be positive" >&2; exit 2; }
command -v "$qemu" >/dev/null 2>&1 || { echo "error: QEMU not found: $qemu" >&2; exit 2; }
command -v nm >/dev/null 2>&1 || { echo "error: nm is required to locate the boot-sector guard" >&2; exit 2; }
command -v python3 >/dev/null 2>&1 || { echo "error: python3 is required for test-image mutation" >&2; exit 2; }

symbol_hex=$(nm -n "$boot_object" | awk '$3 == "kernel_sector_count" { print $1; exit }')
[[ -n "$symbol_hex" ]] || { echo "error: kernel_sector_count symbol is missing" >&2; exit 2; }
offset=$((16#$symbol_hex))
invalid_count=$((maximum + 1))
(( invalid_count <= 255 )) || { echo "error: test count does not fit the BIOS sector-count byte" >&2; exit 2; }

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
set_sector_count() {
    python3 - "$1" "$offset" "$2" <<'PY'
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
offset = int(sys.argv[2])
value = int(sys.argv[3])
data = bytearray(path.read_bytes())
if offset >= 512 or len(data) < 512:
    raise SystemExit("error: sector-count symbol is outside the boot sector")
data[offset] = value
path.write_bytes(data)
PY
}

cp "$image" "$tmp/at-limit.img"
set_sector_count "$tmp/at-limit.img" "$maximum"
set +e
timeout 5s "$qemu" -accel tcg -machine pc -m 32M -boot c \
    -drive format=raw,file="$tmp/at-limit.img" \
    -debugcon "file:$tmp/at-limit-debugcon.log" -global isa-debugcon.iobase=0xe9 \
    -serial "file:$tmp/at-limit-serial.log" -display none -no-reboot -no-shutdown \
    >/dev/null 2>&1
status=$?
set -e
[[ $status -eq 124 ]] || {
    echo "error: exact-limit BIOS boot did not remain in the kernel (QEMU status $status)" >&2
    exit 1
}
grep -q "FADAL KERNEL ONLINE" "$tmp/at-limit-serial.log" || {
    echo "error: maximum ${maximum}-sector load window did not reach the kernel" >&2
    exit 1
}
echo "PASS: QEMU booted through the maximum ${maximum}-sector BIOS load window."

cp "$image" "$tmp/over-limit.img"
set_sector_count "$tmp/over-limit.img" "$invalid_count"

set +e
timeout 3s "$qemu" -accel tcg -machine pc -m 32M -boot c \
    -drive format=raw,file="$tmp/over-limit.img" \
    -debugcon "file:$tmp/debugcon.log" -global isa-debugcon.iobase=0xe9 \
    -serial "file:$tmp/serial.log" -display none -no-reboot -no-shutdown \
    >/dev/null 2>&1
status=$?
set -e
[[ $status -eq 124 ]] || {
    echo "error: over-limit BIOS boot did not halt as expected (QEMU status $status)" >&2
    exit 1
}
[[ "$(cat "$tmp/debugcon.log" 2>/dev/null || true)" == "L" ]] || {
    echo "error: boot sector did not emit its loader-limit rejection marker" >&2
    exit 1
}
if grep -q "FADAL KERNEL ONLINE" "$tmp/serial.log" 2>/dev/null; then
    echo "error: over-limit kernel reached runtime" >&2
    exit 1
fi

echo "PASS: QEMU rejected embedded kernel sector count ${invalid_count} above the ${maximum}-sector loader ceiling."
