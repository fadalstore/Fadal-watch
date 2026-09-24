#!/usr/bin/env bash
set -Eeuo pipefail

REPO="${FADALWATCH_REPO:-fadalstore/Fadal-watch}"
REF="${FADALWATCH_REF:-main}"
ROOT="${FADALWATCH_SOURCE_DIR:-$PWD}"
RUN_CHECK=1
BUILD_UEFI=0

usage() {
    cat <<'EOF'
Usage: scripts/bootstrap-fadalwatch.sh [options]

Prepare and validate an existing FadalWatch checkout.

Options:
  --source-dir DIR  repository root (default: current directory)
  --ref REF         record the expected branch/tag (default: main)
  --no-check        build without the QEMU smoke test
  --uefi            build the UEFI loader, payload, and image when GNU-EFI exists
  -h, --help        show this help
EOF
}

while (($#)); do
    case "$1" in
        --source-dir) ROOT="$2"; shift 2 ;;
        --ref) REF="$2"; shift 2 ;;
        --no-check) RUN_CHECK=0; shift ;;
        --uefi) BUILD_UEFI=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) printf 'error: unknown option: %s\n' "$1" >&2; usage >&2; exit 2 ;;
    esac
done

ROOT="$(cd "$ROOT" && pwd)"
[ -d "$ROOT/kernel" ] || { echo "error: no kernel directory in $ROOT" >&2; exit 1; }
command -v make >/dev/null 2>&1 || { echo 'error: make is required' >&2; exit 1; }
command -v gcc >/dev/null 2>&1 || command -v clang >/dev/null 2>&1 || {
    echo 'error: gcc or clang is required' >&2; exit 1;
}
command -v ld >/dev/null 2>&1 || { echo 'error: ld is required' >&2; exit 1; }
command -v objcopy >/dev/null 2>&1 || { echo 'error: objcopy is required' >&2; exit 1; }

printf '[bootstrap] repository: %s\n' "$ROOT"
printf '[bootstrap] requested ref: %s\n' "$REF"

if command -v rg >/dev/null 2>&1; then
    make -C "$ROOT/kernel" independence
else
    printf '[bootstrap] ripgrep unavailable; independence target will be checked by make when possible\n'
fi

if (( RUN_CHECK )); then
    make -C "$ROOT/kernel" check
else
    make -C "$ROOT/kernel" clean all
fi

if (( BUILD_UEFI )); then
    if [ -d "$ROOT/kernel/uefi" ] && [ -f /usr/include/efi/efi.h ] && \
       [ -f /usr/lib/crt0-efi-x86_64.o ] && [ -f /usr/lib/elf_x86_64_efi.lds ]; then
        printf '[bootstrap] building UEFI loader and payload\n'
        make -C "$ROOT/kernel/uefi" clean check payload
        rm -f "$ROOT/kernel/out/fadal-uefi.img"
        make -C "$ROOT/kernel/uefi" image \
            UEFI_KERNEL="$ROOT/kernel/uefi/out/fadal-uefi.krn"
        cp "$ROOT/kernel/out/fadal-uefi.img" "$ROOT/artifacts/fadal-uefi.img"
    else
        echo 'error: GNU-EFI development files are unavailable for --uefi' >&2
        exit 1
    fi
fi

mkdir -p "$ROOT/artifacts"
cp "$ROOT/kernel/out/fadal-kernel.img" "$ROOT/artifacts/fadal-kernel.img"
sha256sum "$ROOT/artifacts/fadal-kernel.img" > "$ROOT/artifacts/SHA256SUMS"
if [ -s "$ROOT/artifacts/fadal-uefi.img" ]; then
    sha256sum "$ROOT/artifacts/fadal-uefi.img" >> "$ROOT/artifacts/SHA256SUMS"
fi
printf '[bootstrap] BIOS artifact: %s\n' "$ROOT/artifacts/fadal-kernel.img"
printf '[bootstrap] checksum file: %s\n' "$ROOT/artifacts/SHA256SUMS"
printf '[bootstrap] complete\n'
