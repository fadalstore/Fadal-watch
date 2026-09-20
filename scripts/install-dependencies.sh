#!/usr/bin/env bash
set -Eeuo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SUDO=""
if [[ "${EUID}" -ne 0 ]]; then
    SUDO="sudo"
fi

install_debian() {
    ${SUDO} apt-get update
    DEBIAN_FRONTEND=noninteractive ${SUDO} apt-get install -y \
        build-essential gcc-multilib binutils make qemu-system-x86 git \
        dosfstools mtools util-linux fdisk
}

install_fedora() {
    ${SUDO} dnf install -y \
        gcc glibc-devel.i686 binutils make qemu-system-x86 git \
        dosfstools mtools util-linux
}

install_arch() {
    ${SUDO} pacman -Sy --needed --noconfirm \
        base-devel gcc lib32-glibc binutils make qemu-desktop git \
        dosfstools mtools util-linux
}

if [[ "${FADAL_SKIP_INSTALL:-0}" != "1" ]]; then
    if command -v apt-get >/dev/null 2>&1; then
        echo "[fadal] installing Debian/Ubuntu build dependencies"
        install_debian
    elif command -v dnf >/dev/null 2>&1; then
        echo "[fadal] installing Fedora/RHEL build dependencies"
        install_fedora
    elif command -v pacman >/dev/null 2>&1; then
        echo "[fadal] installing Arch Linux build dependencies"
        install_arch
    else
        echo "error: unsupported distribution; install the dependencies listed in README.md" >&2
        exit 1
    fi
else
    echo "[fadal] package installation skipped (FADAL_SKIP_INSTALL=1)"
fi

required=(cc ld as objcopy make qemu-system-i386 git sfdisk mkfs.fat mcopy)
for command_name in "${required[@]}"; do
    if ! command -v "${command_name}" >/dev/null 2>&1; then
        echo "error: required command not found: ${command_name}" >&2
        exit 1
    fi
done

if ! cc -m32 -x c -c /dev/null -o /tmp/fadal-m32-check.o >/dev/null 2>&1; then
    echo "error: the compiler cannot produce 32-bit objects; install gcc-multilib or glibc-devel.i686" >&2
    rm -f /tmp/fadal-m32-check.o
    exit 1
fi
rm -f /tmp/fadal-m32-check.o

echo "[fadal] dependencies ready; running freestanding build and QEMU smoke test"
make -C "${ROOT_DIR}/kernel" clean
make -C "${ROOT_DIR}/kernel" check

echo "[fadal] Fadal-watch dependencies installed and kernel smoke test passed"
