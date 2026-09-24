# FadalOS Build Guide

This guide defines the supported build paths. Run commands from the repository root unless a command changes directory explicitly.

## Fast path

On a prepared Linux or Termux environment:

```sh
scripts/bootstrap-fadalwatch.sh
```

The script builds the BIOS image, runs the canonical smoke test, copies the result to `artifacts/fadal-kernel.img`, and writes `artifacts/SHA256SUMS`.

To build without starting QEMU:

```sh
scripts/bootstrap-fadalwatch.sh --no-check
```

To run the complete repository validator:

```sh
scripts/validate-repo.sh
```

## BIOS kernel

The BIOS path is the primary reproducible build.

```sh
make -C kernel clean all
make -C kernel check
make -C kernel run
make -C kernel run-vga
```

The image is written to `kernel/out/fadal-kernel.img`. `check` must remain the required pre-push test because it verifies the serial boot trace, FAT12 persistence, VFS, scheduler, syscalls, network path, and independence guard.

The default compiler variables can be overridden when a toolchain uses different names:

```sh
make -C kernel CC=clang AS=as LD=ld OBJCOPY=objcopy
```

## UEFI payload

The UEFI path requires GNU-EFI development files and an x86_64 linker environment.

```sh
make -C kernel/uefi clean check
make -C kernel/uefi payload
rm -f kernel/out/fadal-uefi.img
make -C kernel/uefi image \
  UEFI_KERNEL="$PWD/kernel/uefi/out/fadal-uefi.krn"
```

The resulting image is `kernel/out/fadal-uefi.img`. The linker may report an RWX load-segment warning because the current payload is a small freestanding flat image. That warning is known; a future memory-layout milestone should split code and writable data after the loader contract is formalized.

## Termux

The supported Android workflow installs the verified BIOS artifact and a launcher:

```sh
pkg update
pkg install curl coreutils
curl -fsSL https://raw.githubusercontent.com/fadalstore/Fadal-watch/main/scripts/install-fadalwatch-termux.sh | bash
fadalwatch tools
fadalwatch build check
fadalwatch run
```

`fadalwatch build` operates on `$FADALWATCH_SOURCE` when set, or `$HOME/Fadal-watch` by default. The guest remains an x86 BIOS image and is emulated by QEMU on Android.

## Artifacts

Build output belongs in `kernel/out` and `kernel/uefi/out`. Release-like copies belong in `artifacts`. Generated output should not be hand-edited or committed unless it is an explicitly versioned distribution artifact. Use the repository scripts to regenerate images.

## Failure diagnosis

If `make -C kernel check` fails, inspect `kernel/out/smoke.log` first. If the boot banner is absent, inspect the compiler, linker, boot sector size, and QEMU command. If only an individual trace is absent, locate the corresponding self-test in `kernel/kernel.c` and compare its ABI or initialization assumptions. For UEFI faults, first verify the payload header, loader allocation, GOP descriptor, and entry offset before changing renderer code.

## References

[1]: https://github.com/fadalstore/Fadal-watch "FadalWatch source repository"
[2]: https://www.qemu.org/docs/master/ "QEMU documentation"
