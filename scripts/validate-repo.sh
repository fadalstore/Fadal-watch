#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NO_QEMU=0
UEFI=0

usage() {
    cat <<'EOF'
Usage: scripts/validate-repo.sh [--no-qemu] [--uefi]

Run repository hygiene checks and the canonical freestanding build.
EOF
}

while (($#)); do
    case "$1" in
        --no-qemu) NO_QEMU=1; shift ;;
        --uefi) UEFI=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) echo "error: unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
done

bash -n "$ROOT"/scripts/*.sh
[ -f "$ROOT/README.md" ]
[ -f "$ROOT/AI_HANDOFF.md" ]
[ -d "$ROOT/kernel" ]

if (( NO_QEMU )); then
    make -C "$ROOT/kernel" clean all
else
    make -C "$ROOT/kernel" check
fi

if (( UEFI )); then
    make -C "$ROOT/kernel/uefi" clean check payload
fi

printf 'FadalWatch repository validation passed.\n'
