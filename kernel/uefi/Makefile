# Build a PE/COFF x86_64 UEFI application with GNU-EFI.
# Expected source files in this directory:
#   boot.c   - efi_main() and the UEFI-to-kernel handoff
#   boot.ld  - GNU-EFI linker script, usually elf_x86_64_efi.lds

CC ?= gcc
LD ?= ld
OBJCOPY ?= objcopy
OBJDUMP ?= objdump
NM ?= nm
FILE ?= file

OUT ?= out
BOOT_C ?= boot.c
BOOT_SO := $(OUT)/boot.so
BOOT_OBJ := $(OUT)/boot.o
PAGE_TABLES_OBJ := $(OUT)/page_tables.o
BOOT_EFI ?= $(OUT)/BOOTX64.EFI
PAYLOAD_C ?= payload.c
PAYLOAD_LD ?= payload.ld
PAYLOAD_OBJ := $(OUT)/payload.o
PAYLOAD_ELF := $(OUT)/fadal64.elf
PAYLOAD_BIN ?= $(OUT)/fadal-uefi.krn
PATCH_HEADER ?= ../../tools/patch-fadal64-header.py
PAYLOAD_MAX_BYTES ?= 67108864
PF_TEST_C ?= page_protection_test.c
PF_TEST_OBJ := $(OUT)/page_protection_test.o
PF_TEST_SO := $(OUT)/page_protection_test.so
PF_TEST_EFI ?= $(OUT)/PFTESTX64.EFI
PF_TEST_IMAGE ?= $(OUT)/page-protection.img
FOCUS_TEST_SCRIPT ?= ../../scripts/test-uefi-keyboard-focus.py
FOCUS_TEST_IMAGE ?= ../out/fadal-uefi.img
FOCUS_TEST_SCREEN ?= $(OUT)/keyboard-focus.ppm
TERMINAL_SCROLL_SCREEN ?= $(OUT)/terminal-scrollback.ppm
TERMINAL_TAIL_SCREEN ?= $(OUT)/terminal-tail.ppm
WINDOW_CONTROLS_SCREEN ?= $(OUT)/window-controls.ppm
QEMU ?= qemu-system-x86_64
OVMF_CODE ?= /usr/share/OVMF/OVMF_CODE_4M.fd
OVMF_VARS ?= /usr/share/OVMF/OVMF_VARS_4M.fd

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
CFLAGS += -ffreestanding -fPIC -fno-stack-protector \
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

.PHONY: all check check-toolchain payload image page-protection-efi \
	test-page-protection test-keyboard-focus test-terminal-input \
	test-window-controls clean help

all: $(BOOT_EFI)

payload: $(PAYLOAD_BIN)

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

$(BOOT_OBJ): $(BOOT_C) page_tables.h | check-toolchain $(OUT)
	$(CC) $(CFLAGS) -c -o $@ $<

$(PAGE_TABLES_OBJ): page_tables.c page_tables.h | check-toolchain $(OUT)
	$(CC) $(CFLAGS) -c -o $@ page_tables.c

$(BOOT_SO): $(BOOT_OBJ) $(PAGE_TABLES_OBJ) $(BOOT_LD) check-toolchain | $(OUT)
	$(LD) $(LDFLAGS) -T "$(BOOT_LD)" \
		-L"$(EFI_LIBDIR)" -o $@ "$(EFI_CRT)" $(BOOT_OBJ) $(PAGE_TABLES_OBJ) -lefi -lgnuefi

$(BOOT_EFI): $(BOOT_SO) | $(OUT)
	$(OBJCOPY) $(OBJCOPY_FLAGS) $< $@
	@test "$$($(FILE) -b $@ | grep -E -i 'PE32\\+.*x86-64|EFI application')" != "" || \
		(echo "error: $@ is not a PE32+ x86_64 EFI application" >&2; exit 1)

$(PAYLOAD_OBJ): $(PAYLOAD_C) | $(OUT)
	$(CC) -O2 -ffreestanding -fPIC -fno-stack-protector -fno-asynchronous-unwind-tables \
		-fno-unwind-tables -mno-red-zone -m64 -Wall -Wextra -Werror -c -o $@ $<

$(PAYLOAD_ELF): $(PAYLOAD_OBJ) $(PAYLOAD_LD) | $(OUT)
	$(LD) -nostdlib -z max-page-size=0x1000 -z common-page-size=0x1000 \
		-T $(PAYLOAD_LD) -o $@ $(PAYLOAD_OBJ)

$(PAYLOAD_BIN): $(PAYLOAD_ELF) $(PATCH_HEADER)
	$(OBJCOPY) -O binary $< $@
	NM="$(NM)" python3 $(PATCH_HEADER) $@ $(PAYLOAD_ELF)
	@test "$$(stat -c '%s' $@)" -le "$(PAYLOAD_MAX_BYTES)"

$(PF_TEST_OBJ): $(PF_TEST_C) page_tables.h | check-toolchain $(OUT)
	$(CC) $(CFLAGS) -c -o $@ $<

$(PF_TEST_SO): $(PF_TEST_OBJ) $(PAGE_TABLES_OBJ) $(BOOT_LD) check-toolchain | $(OUT)
	$(LD) $(LDFLAGS) -T "$(BOOT_LD)" \
		-L"$(EFI_LIBDIR)" -o $@ "$(EFI_CRT)" $(PF_TEST_OBJ) $(PAGE_TABLES_OBJ) -lefi -lgnuefi

$(PF_TEST_EFI): $(PF_TEST_SO) | $(OUT)
	$(OBJCOPY) $(OBJCOPY_FLAGS) $< $@
	@test "$$($(FILE) -b $@ | grep -E -i 'PE32\\+.*x86-64|EFI application')" != "" || \
		(echo "error: $@ is not a PE32+ x86_64 EFI application" >&2; exit 1)

page-protection-efi: $(PF_TEST_EFI)

test-page-protection: $(PF_TEST_EFI) | $(OUT)
	@printf 'test-only payload; ignored by page-protection EFI app\n' > "$(OUT)/unused-test-payload.krn"
	@rm -f "$(PF_TEST_IMAGE)"
	UEFI_IMAGE="$(abspath $(PF_TEST_IMAGE))" \
	UEFI_BOOTLOADER="$(abspath $(PF_TEST_EFI))" \
	UEFI_KERNEL="$(abspath $(OUT)/unused-test-payload.krn)" \
		../../tools/create-uefi-image.sh
	../../scripts/test-uefi-page-protection.sh \
		--image "$(abspath $(PF_TEST_IMAGE))" --qemu "$(QEMU)" \
		--ovmf-code "$(OVMF_CODE)" --ovmf-vars "$(OVMF_VARS)"

test-keyboard-focus: check payload
	@rm -f "$(FOCUS_TEST_IMAGE)"
	UEFI_IMAGE="$(abspath $(FOCUS_TEST_IMAGE))" \
		$(MAKE) image UEFI_KERNEL="$(abspath $(PAYLOAD_BIN))"
	python3 "$(FOCUS_TEST_SCRIPT)" --image "$(abspath $(FOCUS_TEST_IMAGE))" \
		--qemu "$(QEMU)" --ovmf-code "$(OVMF_CODE)" \
		--ovmf-vars "$(OVMF_VARS)" \
		--screenshot "$(abspath $(FOCUS_TEST_SCREEN))"

test-terminal-input: check payload
	@rm -f "$(FOCUS_TEST_IMAGE)"
	UEFI_IMAGE="$(abspath $(FOCUS_TEST_IMAGE))" \
		$(MAKE) image UEFI_KERNEL="$(abspath $(PAYLOAD_BIN))"
	python3 "$(FOCUS_TEST_SCRIPT)" --image "$(abspath $(FOCUS_TEST_IMAGE))" \
		--qemu "$(QEMU)" --ovmf-code "$(OVMF_CODE)" \
		--ovmf-vars "$(OVMF_VARS)" --terminal-input \
		--screenshot "$(abspath $(FOCUS_TEST_SCREEN))" \
		--scroll-screenshot "$(abspath $(TERMINAL_SCROLL_SCREEN))" \
			--tail-screenshot "$(abspath $(TERMINAL_TAIL_SCREEN))"

test-window-controls: check payload
	@rm -f "$(FOCUS_TEST_IMAGE)"
	UEFI_IMAGE="$(abspath $(FOCUS_TEST_IMAGE))" \
		$(MAKE) image UEFI_KERNEL="$(abspath $(PAYLOAD_BIN))"
	python3 "$(FOCUS_TEST_SCRIPT)" --image "$(abspath $(FOCUS_TEST_IMAGE))" \
		--qemu "$(QEMU)" --ovmf-code "$(OVMF_CODE)" \
		--ovmf-vars "$(OVMF_VARS)" --window-controls \
		--screenshot "$(abspath $(FOCUS_TEST_SCREEN))" \
		--window-screenshot "$(abspath $(WINDOW_CONTROLS_SCREEN))"

check: $(BOOT_EFI)
	@$(OBJDUMP) -f $(BOOT_EFI) | grep -E -q 'pei-x86-64|pe-x86-64' || \
		(echo "error: $(BOOT_EFI) has the wrong object format" >&2; exit 1)
	@echo "Fadal UEFI loader build passed: $(BOOT_EFI)"

image: $(BOOT_EFI) $(PAYLOAD_BIN)
	@test -n "$(UEFI_KERNEL)" || \
		(echo "usage: make image UEFI_KERNEL=path/to/fadal-uefi.krn" >&2; exit 1)
	UEFI_BOOTLOADER="$(BOOT_EFI)" \
	UEFI_KERNEL="$(UEFI_KERNEL)" \
		../../tools/create-uefi-image.sh

help:
	@echo "make                 build BOOTX64.EFI"
	@echo "make check           build and validate PE/COFF format"
	@echo "make image UEFI_KERNEL=...  build and package an ESP image"
	@echo "make page-protection-efi   build isolated EFI page-protection test"
	@echo "make test-page-protection  run test under QEMU/OVMF + isa-debug-exit"
	@echo "make test-keyboard-focus  test UEFI keyboard focus routing in QEMU/OVMF"
	@echo "make test-terminal-input  test line editing and scrollback in QEMU/OVMF"
	@echo "make test-window-controls test terminal window controls with QEMU/OVMF mouse"
	@echo "EFI_INC=... EFI_CRT=... EFI_LDSCRIPT=... EFI_LIBDIR=... override GNU-EFI paths"

clean:
	rm -rf $(OUT)
