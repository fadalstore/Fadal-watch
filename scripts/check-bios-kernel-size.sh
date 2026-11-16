#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "usage: $0 <kernel-image> <maximum-bytes>" >&2
    exit 2
}

[[ $# -eq 2 ]] || usage
image=$1
maximum=$2
[[ -f "$image" ]] || { echo "error: BIOS kernel image not found: $image" >&2; exit 2; }
[[ "$maximum" =~ ^[1-9][0-9]*$ ]] || { echo "error: maximum must be a positive byte count" >&2; exit 2; }

actual=$(wc -c < "$image")
if (( actual > maximum )); then
    echo "error: BIOS kernel image is ${actual} bytes; maximum is ${maximum} bytes" >&2
    exit 1
fi

echo "Fadal BIOS kernel image size accepted: ${actual}/${maximum} bytes"
