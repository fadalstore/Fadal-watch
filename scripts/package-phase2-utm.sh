#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIOS_IMAGE="${1:-$ROOT/kernel/out/fadal-kernel.img}"
UEFI_IMAGE="${2:-$ROOT/kernel/out/fadal-uefi.img}"
OUT_ZIP="${3:-$ROOT/FadalOS-Phase2-UTM-iOS.zip}"
BIOS_SOURCE="$ROOT/utm/FadalWatch-BIOS.utm"
UEFI_SOURCE="$ROOT/utm/FadalWatch-UEFI.utm"
PACKAGE_README="$ROOT/docs/PHASE2_PACKAGE_README.md"

fail() { echo "error: $*" >&2; exit 1; }
[[ -s "$BIOS_IMAGE" ]] || fail "BIOS image not found: $BIOS_IMAGE"
[[ -s "$UEFI_IMAGE" ]] || fail "UEFI image not found: $UEFI_IMAGE"
[[ -f "$BIOS_SOURCE/config.plist" && -f "$UEFI_SOURCE/config.plist" ]] || \
    fail "both source UTM profiles must be present"
[[ -s "$PACKAGE_README" ]] || fail "Phase 2 package README is missing: $PACKAGE_README"
command -v zip >/dev/null 2>&1 || fail "zip is required"
command -v unzip >/dev/null 2>&1 || fail "unzip is required"
command -v sha256sum >/dev/null 2>&1 || fail "sha256sum is required"
command -v realpath >/dev/null 2>&1 || fail "realpath is required"

BIOS_IMAGE="$(realpath "$BIOS_IMAGE")"
UEFI_IMAGE="$(realpath "$UEFI_IMAGE")"
OUT_DIR="$(cd "$(dirname "$OUT_ZIP")" && pwd)"
OUT_NAME="$(basename "$OUT_ZIP")"
OUT_ZIP="$OUT_DIR/$OUT_NAME"
SIDECAR="$OUT_ZIP.sha256"
TEMP_ZIP="$OUT_DIR/.$OUT_NAME.tmp.$$"
STAGE="$(mktemp -d)"
VERIFY="$(mktemp -d)"
trap 'rm -rf "$STAGE" "$VERIFY"; rm -f "$TEMP_ZIP"' EXIT

cp -a "$UEFI_SOURCE" "$BIOS_SOURCE" "$STAGE/"
cp "$UEFI_IMAGE" "$STAGE/FadalWatch-UEFI.utm/Data/fadal-uefi.img"
cp "$BIOS_IMAGE" "$STAGE/FadalWatch-BIOS.utm/Data/fadal-kernel.img"
cp "$PACKAGE_README" "$STAGE/README.md"

(
    cd "$STAGE"
    sha256sum \
        FadalWatch-UEFI.utm/Data/fadal-uefi.img \
        FadalWatch-BIOS.utm/Data/fadal-kernel.img > SHA256SUMS
    # ZIP stores timestamps at two-second resolution. Normalize all entries for
    # stable archive hashes; profile contents and permissions remain unchanged.
    find . -depth -exec touch -h -t 202611150000 {} +
    zip -qr -9 -X "$TEMP_ZIP" \
        README.md SHA256SUMS FadalWatch-UEFI.utm FadalWatch-BIOS.utm
)

unzip -tq "$TEMP_ZIP"
unzip -q "$TEMP_ZIP" -d "$VERIFY"
(
    cd "$VERIFY"
    sha256sum -c SHA256SUMS
)
cmp -s "$BIOS_IMAGE" "$VERIFY/FadalWatch-BIOS.utm/Data/fadal-kernel.img" || \
    fail "the packaged BIOS image differs from the verified input"
cmp -s "$UEFI_IMAGE" "$VERIFY/FadalWatch-UEFI.utm/Data/fadal-uefi.img" || \
    fail "the packaged UEFI image differs from the verified input"

mv "$TEMP_ZIP" "$OUT_ZIP"
(
    cd "$OUT_DIR"
    sha256sum "$OUT_NAME" > "$SIDECAR.tmp.$$"
    mv "$SIDECAR.tmp.$$" "$SIDECAR"
)
echo "Phase 2 UTM package created: $OUT_ZIP"
echo "Archive checksum: $(awk '{print $1}' "$SIDECAR")"
