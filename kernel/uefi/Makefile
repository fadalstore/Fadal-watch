# Build a PE/COFF x86_64 UEFI application with GNU-EFI.
# Expected source files in this directory:
#   boot.c   - efi_main() and the UEFI-to-kernel handoff
#   boot.ld  - GNU-EFI linker script, usually elf_x86_64_efi.lds

CC ?= gcc
LD ?= ld
OBJCOPY ?= objcopy
OBJDUMP ?= objdump
FILE ?= file

OUT ?= out
BOOT_C ?= boot.c
BOOT_SO := $(OUT)/boot.so
BOOT_OBJ := $(OUT)/boot.o
BOOT_EFI ?= $(OUT)/BOOTX64.EFI

# Override these when the distribution installs GNU-EFI elsewhere.
EFI_INC ?= /usr/include/efi
EFI_ARCH_INC ?= $(EFI_INC)/x86_64
EFI_CRT ?= $(firstword $(wildcard \
	/usr/lib/crt0-efi-x86_64.o \
	/usr/lib64/crt0-efi-x86_64.o \
	/usr/lib/x86_64-linux-gnu/crt0-efi-x86_64.o))
EFI_LDSCRIPT ?= $(firstword $(wildcard \
	/usr/lib/elf_x86_64_efi.lds \
	/usr/lib64/elf_x86_64_efi.lds \
	/usr/lib/x86_64-linux-gnu/elf_x86_64_efi.lds))
EFI_LIBDIR ?= $(if $(EFI_CRT),$(dir $(EFI_CRT)),/usr/lib)
BOOT_LD ?= $(EFI_LDSCRIPT)

# GNU-EFI requires the Microsoft x64 ABI for UEFI callbacks, a short wchar_t,
# no red zone, and accumulated outgoing arguments.
CFLAGS ?= -O2
CFLAGS += -ffreestanding -fno-stack-protector -fno-pie \
	-fno-asynchronous-unwind-tables -fno-unwind-tables \
	-fshort-wchar -mno-red-zone -maccumulate-outgoing-args -m64 \
	-Wall -Wextra -Werror \
	-I$(EFI_INC) -I$(EFI_ARCH_INC) \
	-DEFI_FUNCTION_WRAPPER

LDFLAGS ?=
LDFLAGS += -nostdlib -znocombreloc -shared -Bsymbolic

OBJCOPY_FLAGS := \
	-j .text -j .stext -j .data -j .rodata -j .sdata \
	-j .dynamic -j .dynsym -j .rel -j .rela -j .reloc \
	-j .eh_frame -j .bss --subsystem=10 \
	--target=efi-app-x86_64

.PHONY: all check check-toolchain image clean help

all: $(BOOT_EFI)

$(OUT):
	mkdir -p $@

check-toolchain:
	@test -d "$(EFI_INC)" || \
		(echo "error: GNU-EFI headers not found at $(EFI_INC)" >&2; \
		echo "install gnu-efi or override EFI_INC" >&2; exit 1)
	@test -n "$(EFI_CRT)" && test -f "$(EFI_CRT)" || \
		(echo "error: crt0-efi-x86_64.o not found" >&2; \
		echo "override EFI_CRT with its installed path" >&2; exit 1)
	@test -n "$(EFI_LDSCRIPT)" && test -f "$(EFI_LDSCRIPT)" || \
		(echo "error: elf_x86_64_efi.lds not found" >&2; \
		echo "override EFI_LDSCRIPT with its installed path" >&2; exit 1)
	@test -f "$(EFI_LIBDIR)/libefi.a" || \
		(echo "error: libefi.a not found in $(EFI_LIBDIR)" >&2; exit 1)
	@test -f "$(EFI_LIBDIR)/libgnuefi.a" || \
		(echo "error: libgnuefi.a not found in $(EFI_LIBDIR)" >&2; exit 1)
	@command -v $(CC) >/dev/null || \
		(echo "error: compiler not found: $(CC)" >&2; exit 1)
	@command -v $(LD) >/dev/null || \
		(echo "error: linker not found: $(LD)" >&2; exit 1)
	@command -v $(OBJCOPY) >/dev/null || \
		(echo "error: objcopy not found: $(OBJCOPY)" >&2; exit 1)

$(BOOT_OBJ): $(BOOT_C) | check-toolchain $(OUT)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BOOT_SO): $(BOOT_OBJ) $(BOOT_LD) check-toolchain | $(OUT)
	$(LD) $(LDFLAGS) -T "$(BOOT_LD)" \
		-L"$(EFI_LIBDIR)" -o $@ "$(EFI_CRT)" $< -lefi -lgnuefi

$(BOOT_EFI): $(BOOT_SO) | $(OUT)
	$(OBJCOPY) $(OBJCOPY_FLAGS) $< $@
	@test "$$($(FILE) -b $@ | grep -E -i 'PE32\\+.*x86-64|EFI application')" != "" || \
		(echo "error: $@ is not a PE32+ x86_64 EFI application" >&2; exit 1)

check: $(BOOT_EFI)
	@$(OBJDUMP) -f $(BOOT_EFI) | grep -E -q 'pei-x86-64|pe-x86-64' || \
		(echo "error: $(BOOT_EFI) has the wrong object format" >&2; exit 1)
	@echo "Fadal UEFI loader build passed: $(BOOT_EFI)"

image: $(BOOT_EFI)
	@test -n "$(UEFI_KERNEL)" || \
		(echo "usage: make image UEFI_KERNEL=path/to/fadal-uefi.krn" >&2; exit 1)
	UEFI_BOOTLOADER="$(BOOT_EFI)" \
	UEFI_KERNEL="$(UEFI_KERNEL)" \
		../../tools/create-uefi-image.sh

help:
	@echo "make                 build BOOTX64.EFI"
	@echo "make check           build and validate PE/COFF format"
	@echo "make image UEFI_KERNEL=...  build and package an ESP image"
	@echo "EFI_INC=... EFI_CRT=... EFI_LDSCRIPT=... EFI_LIBDIR=... override GNU-EFI paths"

clean:
	rm -rf $(OUT)
