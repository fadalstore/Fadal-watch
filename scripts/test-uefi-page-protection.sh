#!/usr/bin/env bash
set -u

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE="${ROOT_DIR}/kernel/uefi/out/page-protection.img"
QEMU_BIN="${QEMU:-qemu-system-x86_64}"
OVMF_CODE="${OVMF_CODE:-/usr/share/OVMF/OVMF_CODE_4M.fd}"
OVMF_VARS="${OVMF_VARS:-/usr/share/OVMF/OVMF_VARS_4M.fd}"
TIMEOUT_SECONDS="${QEMU_TIMEOUT_SECONDS:-30}"

usage() {
    cat <<'EOF'
Usage: scripts/test-uefi-page-protection.sh [options]

Run the isolated EFI page-protection test under QEMU/OVMF.

Options:
  --image PATH       Raw UEFI test image
  --qemu PATH        QEMU x86_64 executable
  --ovmf-code PATH   OVMF code firmware image
  --ovmf-vars PATH   OVMF variables template
  --timeout SECONDS Guest timeout (default: 30)
EOF
}

while (($#)); do
    case "$1" in
        --image) IMAGE="$2"; shift 2 ;;
        --qemu) QEMU_BIN="$2"; shift 2 ;;
        --ovmf-code) OVMF_CODE="$2"; shift 2 ;;
        --ovmf-vars) OVMF_VARS="$2"; shift 2 ;;
        --timeout) TIMEOUT_SECONDS="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "error: unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
done

if [[ ! "$TIMEOUT_SECONDS" =~ ^[0-9]+$ ]] || (( TIMEOUT_SECONDS < 1 )); then
    echo 'error: --timeout must be a positive integer' >&2
    exit 2
fi
for path in "$IMAGE" "$OVMF_CODE" "$OVMF_VARS"; do
    if [[ ! -f "$path" ]]; then
        echo "error: required file not found: $path" >&2
        exit 2
    fi
done
if ! command -v "$QEMU_BIN" >/dev/null 2>&1; then
    echo "error: QEMU executable not found: $QEMU_BIN" >&2
    exit 2
fi
if ! command -v timeout >/dev/null 2>&1; then
    echo 'error: timeout command is required' >&2
    exit 2
fi

TEMP_DIR="$(mktemp -d)" || exit 2
trap 'rm -rf "$TEMP_DIR"' EXIT
VARS_COPY="${TEMP_DIR}/OVMF_VARS.fd"
SERIAL_LOG="${TEMP_DIR}/serial.log"
cp "$OVMF_VARS" "$VARS_COPY" || exit 2

printf '[pf-test] QEMU: %s\n' "$QEMU_BIN"
printf '[pf-test] image: %s\n' "$IMAGE"
printf '[pf-test] serial log: %s\n' "$SERIAL_LOG"
set +e
timeout --foreground "${TIMEOUT_SECONDS}s" "$QEMU_BIN" \
    -machine q35 \
    -m 256M \
    -drive "if=pflash,format=raw,readonly=on,file=${OVMF_CODE}" \
    -drive "if=pflash,format=raw,file=${VARS_COPY}" \
    -drive "format=raw,file=${IMAGE}" \
    -device isa-debug-exit,iobase=0xf4,iosize=0x04 \
    -display none \
    -monitor none \
    -serial "file:${SERIAL_LOG}" \
    -no-reboot
QEMU_STATUS=$?
set -e

if [[ -f "$SERIAL_LOG" ]]; then
    cat "$SERIAL_LOG"
fi

case "$QEMU_STATUS" in
    33)
        echo '[pf-test] PASS: NX and read-only #PFs were caught and recovered'
        exit 0
        ;;
    35)
        echo '[pf-test] FAIL: test reported a protection failure' >&2
        exit 1
        ;;
    37)
        echo '[pf-test] SKIP: CPU NX, firmware IDT, or four-level identity mapping is unsupported' >&2
        exit 77
        ;;
    124)
        echo '[pf-test] ERROR: QEMU timed out; guest did not report a result' >&2
        exit 2
        ;;
    *)
        echo "[pf-test] ERROR: unexpected QEMU exit status ${QEMU_STATUS}" >&2
        exit 2
        ;;
esac
