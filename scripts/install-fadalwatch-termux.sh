#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

REPO="${FADALWATCH_REPO:-fadalstore/Fadal-watch}"
REF="${FADALWATCH_REF:-main}"
PREFIX_DIR="${PREFIX:-/data/data/com.termux/files/usr}"
ROOT="${FADALWATCH_DIR:-${PREFIX_DIR}/opt/fadalwatch}"
BIN_DIR="${PREFIX_DIR}/bin"
BASE_URL="https://raw.githubusercontent.com/${REPO}/${REF}"
BIOS_SHA256="285939762ccffc6d8eb49568bcd4dddc8ca90bd14679cff8424c1e3d064e269c"

fail() { printf 'FadalWatch Termux installer: %s\n' "$1" >&2; exit 1; }

if [ "${PREFIX:-}" = "" ] || [ "${TERMUX_VERSION:-}" = "" ]; then
    fail "this installer must be run inside Termux"
fi
command -v curl >/dev/null 2>&1 || fail "curl is required; run: pkg install curl"
command -v sha256sum >/dev/null 2>&1 || fail "coreutils is required; run: pkg install coreutils"

mkdir -p "$ROOT/images" "$ROOT/bin" "$BIN_DIR"
printf 'Installing FadalWatch for Termux in %s\n' "$ROOT"

curl --fail --location --silent --show-error --retry 3 \
    "$BASE_URL/install/fadal-kernel.img" \
    --output "$ROOT/images/fadal-kernel.img"
actual_sha256="$(sha256sum "$ROOT/images/fadal-kernel.img" | awk '{print $1}')"
[ "$actual_sha256" = "$BIOS_SHA256" ] || fail "BIOS image checksum mismatch"

curl --fail --location --silent --show-error --retry 3 \
    "$BASE_URL/scripts/fadalwatch-termux" \
    --output "$ROOT/bin/fadalwatch"
chmod 755 "$ROOT/bin/fadalwatch"
ln -sfn "$ROOT/bin/fadalwatch" "$BIN_DIR/fadalwatch"

cat > "$ROOT/INSTALL-MANIFEST" <<EOF
FadalWatch Termux installation
Repository: $REPO
Ref: $REF
Installed: $(date -u +%Y-%m-%dT%H:%M:%SZ)
BIOS image SHA-256: $actual_sha256
EOF

printf '\nFadalWatch Termux installation complete.\n'
printf 'Run: fadalwatch info\n'
printf 'Boot the BIOS image: fadalwatch run\n'
printf 'Install build tools: fadalwatch tools\n'
printf 'Location: %s\n' "$ROOT"
