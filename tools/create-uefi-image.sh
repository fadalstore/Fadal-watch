#!/usr/bin/env bash
set -Eeuo pipefail

usage() {
    cat >&2 <<'EOF'
Usage: create-uefi-image.sh [options]

Create a BIOS-partitioned FAT32 EFI System Partition image without loop mounts.
The image contains EFI/BOOT/BOOTX64.EFI and FADAL.KRN.

Options:
  --image PATH       Output image (default: kernel/out/fadal-uefi.img)
  --bootloader PATH UEFI loader (default: kernel/out/BOOTX64.EFI)
  --kernel PATH      Kernel payload (default: kernel/out/fadal-uefi.krn)
  --size-mib N       Image size in MiB (default: 64)
  --force            Replace an existing output image
  -h, --help         Show this help

The same settings can be supplied through UEFI_IMAGE, UEFI_BOOTLOADER,
UEFI_KERNEL, and UEFI_IMAGE_SIZE_MIB environment variables.
EOF
    exit 2
}

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE="${UEFI_IMAGE:-${ROOT_DIR}/kernel/out/fadal-uefi.img}"
BOOTLOADER="${UEFI_BOOTLOADER:-${ROOT_DIR}/kernel/out/BOOTX64.EFI}"
KERNEL="${UEFI_KERNEL:-${ROOT_DIR}/kernel/out/fadal-uefi.krn}"
SIZE_MIB="${UEFI_IMAGE_SIZE_MIB:-64}"
FORCE=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --image) IMAGE="$2"; shift 2 ;;
        --bootloader) BOOTLOADER="$2"; shift 2 ;;
        --kernel) KERNEL="$2"; shift 2 ;;
        --size-mib) SIZE_MIB="$2"; shift 2 ;;
        --force) FORCE=1; shift ;;
        -h|--help) usage ;;
        *) echo "error: unknown option: $1" >&2; usage ;;
    esac
done

for command_name in sfdisk mkfs.fat mcopy; do
    if ! command -v "$command_name" >/dev/null 2>&1; then
        echo "error: required command not found: $command_name" >&2
        echo "install dosfstools, mtools, and util-linux first" >&2
        exit 1
    fi
done

if [[ ! "$SIZE_MIB" =~ ^[0-9]+$ ]] || (( SIZE_MIB < 8 )); then
    echo "error: --size-mib must be an integer of at least 8" >&2
    exit 1
fi

if [[ ! -f "$BOOTLOADER" ]]; then
    echo "error: UEFI bootloader not found: $BOOTLOADER" >&2
    echo "build or provide a PE/COFF loader with --bootloader PATH" >&2
    exit 1
fi
if [[ ! -f "$KERNEL" ]]; then
    echo "error: UEFI kernel payload not found: $KERNEL" >&2
    echo "build or provide the kernel image with --kernel PATH" >&2
    exit 1
fi

mkdir -p "$(dirname "$IMAGE")"
if [[ -e "$IMAGE" && "$FORCE" -ne 1 ]]; then
    echo "error: output already exists: $IMAGE (use --force to replace it)" >&2
    exit 1
fi

TOTAL_SECTORS=$((SIZE_MIB * 2048))
PARTITION_START=2048
PARTITION_SECTORS=$((TOTAL_SECTORS - PARTITION_START))
if (( PARTITION_SECTORS < 4096 )); then
    echo "error: image is too small for an EFI System Partition" >&2
    exit 1
fi

TMP_IMAGE="${IMAGE}.tmp.$$"
cleanup() { rm -f "$TMP_IMAGE"; }
trap cleanup EXIT

printf '[uefi] creating %s (%s MiB)\n' "$IMAGE" "$SIZE_MIB"
truncate -s "$((SIZE_MIB * 1024 * 1024))" "$TMP_IMAGE"

# MBR type 0xef is the legacy partition-table representation of an EFI
# System Partition and is accepted by both UEFI firmware and OVMF. Keeping
# the partition at the 1 MiB boundary also gives the filesystem safe alignment.
sfdisk --no-reread "$TMP_IMAGE" >/dev/null <<EOF
label: dos
unit: sectors

start=$PARTITION_START, size=$PARTITION_SECTORS, type=ef, bootable
EOF

mkfs.fat -F 32 -n FADAL_ESP --offset "$PARTITION_START" "$TMP_IMAGE" >/dev/null

MTOOLS_IMAGE="${TMP_IMAGE}@@$((PARTITION_START * 512))"
mmd -i "$MTOOLS_IMAGE" ::/EFI
mmd -i "$MTOOLS_IMAGE" ::/EFI/BOOT
mcopy -i "$MTOOLS_IMAGE" "$BOOTLOADER" ::/EFI/BOOT/BOOTX64.EFI
mcopy -i "$MTOOLS_IMAGE" "$KERNEL" ::/FADAL.KRN

mv -f "$TMP_IMAGE" "$IMAGE"
trap - EXIT

printf '[uefi] image ready: %s\n' "$IMAGE"
printf '[uefi] bootloader: EFI/BOOT/BOOTX64.EFI (%s bytes)\n' "$(stat -c '%s' "$BOOTLOADER")"
printf '[uefi] kernel: FADAL.KRN (%s bytes)\n' "$(stat -c '%s' "$KERNEL")"
printf '[uefi] ESP starts at sector %s (%s bytes)\n' "$PARTITION_START" "$((PARTITION_START * 512))"
