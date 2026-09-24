#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NO_QEMU=0
UEFI=0
REQUIRE_QEMU=0

usage() {
    cat <<'EOF'
Usage: scripts/validate-repo.sh [--no-qemu] [--require-qemu] [--uefi]

Run repository hygiene checks and the canonical freestanding build.
EOF
}

while (($#)); do
    case "$1" in
        --no-qemu) NO_QEMU=1; shift ;;
        --require-qemu) REQUIRE_QEMU=1; shift ;;
        --uefi) UEFI=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) echo "error: unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
done

bash -n "$ROOT"/scripts/*.sh
[ -f "$ROOT/README.md" ]
[ -f "$ROOT/AI_HANDOFF.md" ]
[ -d "$ROOT/kernel" ]

QEMU_BIN=""
if command -v qemu-system-i386 >/dev/null 2>&1; then QEMU_BIN="qemu-system-i386"; fi
if [ -z "$QEMU_BIN" ] && command -v qemu-system-x86_64 >/dev/null 2>&1; then QEMU_BIN="qemu-system-x86_64"; fi

if (( NO_QEMU )); then
    make -C "$ROOT/kernel" independence
    make -C "$ROOT/kernel" clean all
elif [ -n "$QEMU_BIN" ]; then
    make -C "$ROOT/kernel" QEMU="$QEMU_BIN" check
elif (( REQUIRE_QEMU )); then
    echo 'error: QEMU is required; run: fadalwatch tools' >&2
    exit 1
else
    printf 'warning: QEMU is unavailable; running compile and independence checks only.\n'
    printf 'Install it with: pkg install qemu-system-x86-64 ripgrep\n'
    make -C "$ROOT/kernel" independence
    make -C "$ROOT/kernel" clean all
fi

if (( UEFI )); then
    make -C "$ROOT/kernel/uefi" clean check payload
fi

printf 'FadalWatch repository validation passed.\n'
