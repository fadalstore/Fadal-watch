#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIOS_IMAGE="${1:-$ROOT/kernel/out/fadal-kernel.img}"
BIOS_BUNDLE="$ROOT/utm/FadalWatch-BIOS.utm"
UEFI_BUNDLE="$ROOT/utm/FadalWatch-UEFI.utm"
UTM_ZIP="$ROOT/FadalWatch-UTM-iOS.zip"
ALIAS_ZIP="$ROOT/FadalOS-Phase1-UTM.zip"
INSTALLER="$ROOT/scripts/install-fadalwatch.sh"
MANIFEST="$ROOT/artifacts/SHA256SUMS"
TEMP_ZIP="$ROOT/.FadalWatch-UTM-iOS.zip.tmp.$$"

fail() { echo "error: $*" >&2; exit 1; }
[[ -s "$BIOS_IMAGE" ]] || fail "BIOS kernel image not found: $BIOS_IMAGE"
[[ -f "$UEFI_BUNDLE/config.plist" && -s "$UEFI_BUNDLE/Data/fadal-uefi.img" ]] || fail "UEFI bundle is incomplete"
[[ -f "$BIOS_BUNDLE/config.plist" ]] || fail "BIOS UTM config is missing"
command -v zip >/dev/null 2>&1 || fail "zip is required"
command -v sha256sum >/dev/null 2>&1 || fail "sha256sum is required"

trap 'rm -f "$TEMP_ZIP"' EXIT
mkdir -p "$ROOT/artifacts" "$ROOT/install" "$BIOS_BUNDLE/Data"
cp "$BIOS_IMAGE" "$ROOT/artifacts/fadal-kernel.img"
cp "$BIOS_IMAGE" "$ROOT/install/fadal-kernel.img"
cp "$BIOS_IMAGE" "$BIOS_BUNDLE/Data/fadal-kernel.img"

(cd "$ROOT/utm" && zip -qr -9 -X "$TEMP_ZIP" FadalWatch-UEFI.utm FadalWatch-BIOS.utm)
mv "$TEMP_ZIP" "$UTM_ZIP"
cp "$UTM_ZIP" "$ALIAS_ZIP"

UTM_HASH="$(sha256sum "$UTM_ZIP" | awk '{print $1}')"
BIOS_HASH="$(sha256sum "$BIOS_IMAGE" | awk '{print $1}')"
python3 - "$INSTALLER" "$UTM_HASH" "$BIOS_HASH" <<'PY'
from pathlib import Path
import re
import sys

path = Path(sys.argv[1])
text = path.read_text()
for key, value in (("expected_utm", sys.argv[2]), ("expected_bios", sys.argv[3])):
    text, count = re.subn(rf'(?m)^{key}="[0-9a-f]{{64}}"$', f'{key}="{value}"', text)
    if count != 1:
        raise SystemExit(f"expected exactly one {key} checksum in {path}, found {count}")
path.write_text(text)
PY

(cd "$ROOT" && sha256sum \
    FadalWatch-UTM-iOS.zip \
    FadalOS-Phase1-UTM.zip \
    artifacts/fadal-kernel.img \
    install/fadal-kernel.img \
    utm/FadalWatch-BIOS.utm/config.plist \
    utm/FadalWatch-BIOS.utm/Data/fadal-kernel.img \
    utm/FadalWatch-UEFI.utm/Data/fadal-uefi.img > "$MANIFEST")

cmp -s "$UTM_ZIP" "$ALIAS_ZIP" || fail "UTM archive aliases differ"
cmp -s "$BIOS_IMAGE" "$BIOS_BUNDLE/Data/fadal-kernel.img" || fail "BIOS bundle image copy differs"
echo "UTM packages refreshed. BIOS image SHA-256: $BIOS_HASH"
echo "UEFI profile: $UEFI_BUNDLE"
echo "BIOS profile: $BIOS_BUNDLE"
echo "UTM archive:  $UTM_ZIP"
