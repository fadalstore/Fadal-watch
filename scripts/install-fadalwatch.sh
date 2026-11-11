#!/usr/bin/env bash
set -euo pipefail

REPO="${FADALWATCH_REPO:-fadalstore/Fadal-watch}"
REF="${FADALWATCH_REF:-main}"
DEST="${1:-${FADALWATCH_DIR:-$HOME/FadalWatch}}"
BASE_URL="https://raw.githubusercontent.com/${REPO}/${REF}"

mkdir -p "$DEST"

fetch() {
    local path="$1" output="$2"
    local token="${FADALWATCH_TOKEN:-${GH_TOKEN:-}}"
    if [ -n "$token" ] && command -v curl >/dev/null 2>&1; then
        curl --fail --location --silent --show-error --retry 3 \
            -H "Authorization: Bearer ${token}" \
            -H 'Accept: application/vnd.github.raw' \
            "https://api.github.com/repos/${REPO}/contents/${path}?ref=${REF}" \
            --output "$output"
    elif [ -n "$token" ] && command -v wget >/dev/null 2>&1; then
        wget --quiet --tries=3 \
            --header="Authorization: Bearer ${token}" \
            --header='Accept: application/vnd.github.raw' \
            "https://api.github.com/repos/${REPO}/contents/${path}?ref=${REF}" \
            --output-document="$output"
    elif command -v curl >/dev/null 2>&1; then
        local url="${BASE_URL}/${path}"
        curl --fail --location --silent --show-error --retry 3 "$url" --output "$output"
    elif command -v wget >/dev/null 2>&1; then
        local url="${BASE_URL}/${path}"
        wget --quiet --tries=3 "$url" --output-document="$output"
    elif command -v gh >/dev/null 2>&1 && gh auth status >/dev/null 2>&1; then
        gh api -H 'Accept: application/vnd.github.raw' \
            "repos/${REPO}/contents/${path}?ref=${REF}" > "$output"
    else
        echo "error: curl or wget is required" >&2
        exit 1
    fi
}

printf 'Downloading FadalWatch into %s\n' "$DEST"
fetch "FadalWatch-UTM-iOS.zip" "$DEST/FadalWatch-UTM-iOS.zip"
fetch "FadalWatch-iOS-source.zip" "$DEST/FadalWatch-iOS-source.zip"
fetch "install/fadal-kernel.img" "$DEST/fadal-kernel.img"

expected_utm="bd1b433309cce3671f9a206f56629db8d7db8268a0167a378696db705c362fab"
expected_source="ed59a510bb3119b15e981b24a65034fceea3bb41387cc11262276d15ece773ff"
expected_bios="4b8cdedd7420303e53bf2d3970275ad88c891f0523cde614b4c31ce470f9b6b3"

if command -v sha256sum >/dev/null 2>&1; then
    actual_utm="$(sha256sum "$DEST/FadalWatch-UTM-iOS.zip" | awk '{print $1}')"
    actual_source="$(sha256sum "$DEST/FadalWatch-iOS-source.zip" | awk '{print $1}')"
    actual_bios="$(sha256sum "$DEST/fadal-kernel.img" | awk '{print $1}')"
else
    actual_utm="$(shasum -a 256 "$DEST/FadalWatch-UTM-iOS.zip" | awk '{print $1}')"
    actual_source="$(shasum -a 256 "$DEST/FadalWatch-iOS-source.zip" | awk '{print $1}')"
    actual_bios="$(shasum -a 256 "$DEST/fadal-kernel.img" | awk '{print $1}')"
fi

[[ "$actual_utm" == "$expected_utm" ]] || { echo "error: UTM archive checksum mismatch" >&2; exit 1; }
[[ "$actual_source" == "$expected_source" ]] || { echo "error: iOS source checksum mismatch" >&2; exit 1; }
[[ "$actual_bios" == "$expected_bios" ]] || { echo "error: BIOS image checksum mismatch" >&2; exit 1; }

printf '\nFadalWatch download complete.\n'
printf 'UTM package: %s\n' "$DEST/FadalWatch-UTM-iOS.zip"
printf 'iOS source:  %s\n' "$DEST/FadalWatch-iOS-source.zip"
printf 'BIOS image:  %s\n' "$DEST/fadal-kernel.img"
printf '\nTo import on iOS, extract FadalWatch-UTM-iOS.zip and open FadalWatch-UEFI.utm for the desktop or FadalWatch-BIOS.utm for the 32-bit kernel in UTM using Emulate.\n'
