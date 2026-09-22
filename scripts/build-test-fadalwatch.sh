#!/usr/bin/env bash
set -Eeuo pipefail

REPO="${FADALWATCH_REPO:-fadalstore/Fadal-watch}"
REF="${FADALWATCH_REF:-main}"
ROOT="${FADALWATCH_SOURCE_DIR:-$HOME/FadalWatch-source}"
REQUIRE_QEMU=0
SKIP_UEFI=0

usage() {
    cat <<'EOF'
Usage: build-test-fadalwatch.sh [options]

Downloads the public FadalWatch source, builds the BIOS kernel, runs the QEMU
smoke test when available, and builds/packages the UEFI artifacts when GNU-EFI
and image tools are installed.

Options:
  --source-dir DIR   install/build source in DIR (default: ~/FadalWatch-source)
  --ref REF          Git branch or tag (default: main)
  --require-qemu     fail instead of skipping when QEMU is unavailable
  --no-uefi          skip the UEFI build/package phase
  -h, --help         show this help
EOF
}

while (($#)); do
    case "$1" in
        --source-dir) ROOT="$2"; shift 2 ;;
        --ref) REF="$2"; shift 2 ;;
        --require-qemu) REQUIRE_QEMU=1; shift ;;
        --no-uefi) SKIP_UEFI=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) echo "error: unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
done

log() { printf '[fadalwatch] %s\n' "$*"; }
warn() { printf '[fadalwatch] warning: %s\n' "$*" >&2; }
die() { printf '[fadalwatch] error: %s\n' "$*" >&2; exit 1; }

need() { command -v "$1" >/dev/null 2>&1 || die "required command not found: $1"; }

fetch() {
    local url="$1" output="$2"
    if command -v curl >/dev/null 2>&1; then
        curl --fail --location --silent --show-error --retry 3 "$url" --output "$output"
    elif command -v wget >/dev/null 2>&1; then
        wget --quiet --tries=3 "$url" --output-document="$output"
    else
        die "curl or wget is required"
    fi
}

need tar
need mktemp
need make
need gcc
need ld
need objcopy

TMP="$(mktemp -d)"
BACKUP=""
cleanup() {
    rm -rf "$TMP"
    if [[ -n "$BACKUP" && -d "$BACKUP" && ! -e "$ROOT" ]]; then
        mv "$BACKUP" "$ROOT"
        warn "restored previous source tree after failure"
    fi
}
trap cleanup EXIT

ARCHIVE="$TMP/fadalwatch.tar.gz"
URL="https://github.com/${REPO}/archive/refs/heads/${REF}.tar.gz"
log "downloading source: ${REPO}@${REF}"
fetch "$URL" "$ARCHIVE"
tar -xzf "$ARCHIVE" -C "$TMP"
SRC="$(find "$TMP" -mindepth 1 -maxdepth 1 -type d -name 'Fadal-watch-*' -print -quit)"
[[ -n "$SRC" && -d "$SRC/kernel" ]] || die "downloaded archive has no kernel directory"

log "building BIOS kernel"
make -C "$SRC/kernel" clean all
BIOS="$SRC/kernel/out/fadal-kernel.img"
[[ -s "$BIOS" ]] || die "BIOS image was not produced"
log "BIOS build passed: $BIOS"

if command -v qemu-system-i386 >/dev/null 2>&1 && command -v rg >/dev/null 2>&1 && command -v timeout >/dev/null 2>&1; then
    log "running BIOS QEMU smoke test"
    make -C "$SRC/kernel" check
    log "BIOS QEMU smoke test passed"
else
    if (( REQUIRE_QEMU )); then
        die "QEMU, ripgrep, and timeout are required by --require-qemu"
    fi
    warn "QEMU smoke test skipped; install qemu-system-i386, ripgrep, and timeout to enable it"
fi

UEFI_IMAGE=""
if (( ! SKIP_UEFI )); then
    if [[ -d /usr/include/efi ]] && [[ -f /usr/lib/crt0-efi-x86_64.o ]] && \
       [[ -f /usr/lib/elf_x86_64_efi.lds ]] && [[ -f /usr/lib/libefi.a ]] && \
       [[ -f /usr/lib/libgnuefi.a ]]; then
        log "building UEFI loader and Fadal64 payload"
        make -C "$SRC/kernel/uefi" clean payload check
        UEFI_IMAGE="$SRC/kernel/uefi/out/fadal-uefi.img"
        log "packaging UEFI image"
        env UEFI_IMAGE="$UEFI_IMAGE" \
            UEFI_BOOTLOADER="$SRC/kernel/uefi/out/BOOTX64.EFI" \
            UEFI_KERNEL="$SRC/kernel/uefi/out/fadal-uefi.krn" \
            "$SRC/tools/create-uefi-image.sh" --force
        [[ -s "$UEFI_IMAGE" ]] || die "UEFI image was not produced"
        log "UEFI build/package passed: $UEFI_IMAGE"
    else
        warn "UEFI phase skipped; GNU-EFI development files are unavailable"
    fi
fi

if [[ -e "$ROOT" ]]; then
    BACKUP="${ROOT}.previous.$(date +%Y%m%d%H%M%S)"
    mv "$ROOT" "$BACKUP"
fi
mv "$SRC" "$ROOT"
BACKUP=""

mkdir -p "$ROOT/artifacts"
cp "$ROOT/kernel/out/fadal-kernel.img" "$ROOT/artifacts/fadal-kernel.img"
if [[ -s "$ROOT/kernel/uefi/out/fadal-uefi.img" ]]; then
    cp "$ROOT/kernel/uefi/out/fadal-uefi.img" "$ROOT/artifacts/fadal-uefi.img"
fi

log "FadalWatch update/build/test complete"
log "source: $ROOT"
log "BIOS image: $ROOT/artifacts/fadal-kernel.img"
if [[ -s "$ROOT/artifacts/fadal-uefi.img" ]]; then
    log "UEFI image: $ROOT/artifacts/fadal-uefi.img"
fi
log "checksums:"
find "$ROOT/artifacts" -maxdepth 1 -type f -print0 | sort -z | xargs -0 sha256sum
