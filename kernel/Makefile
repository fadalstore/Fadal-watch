CC ?= gcc
AS ?= as
LD ?= ld
OBJCOPY ?= objcopy
QEMU ?= qemu-system-i386

OUT := out
BOOT_SECTORS := 48
KERNEL_BYTES := $(shell expr $(BOOT_SECTORS) \* 512)

CFLAGS := -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-builtin \
	-mno-sse -mno-sse2 -mno-mmx -msoft-float -fno-tree-vectorize \
	-Wall -Wextra -Werror -O2
LDFLAGS := -m elf_i386 -T linker.ld -nostdlib

.PHONY: all clean check run

all: $(OUT)/fadal-kernel.img

$(OUT):
	mkdir -p $(OUT)

$(OUT)/boot.o: boot.S | $(OUT)
	$(AS) --32 -o $@ $<

$(OUT)/kernel.o: kernel.c | $(OUT)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OUT)/interrupts.o: interrupts.S | $(OUT)
	$(AS) --32 -o $@ $<

$(OUT)/kernel.elf: $(OUT)/kernel.o $(OUT)/interrupts.o linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OUT)/kernel.o $(OUT)/interrupts.o

$(OUT)/kernel.bin: $(OUT)/kernel.elf
	$(OBJCOPY) -O binary $< $@
	@test "$$(stat -c%s $@)" -le "$(KERNEL_BYTES)" || \
		(echo "kernel is larger than $(KERNEL_BYTES) bytes" >&2; exit 1)

$(OUT)/boot.bin: $(OUT)/boot.o
	$(OBJCOPY) -O binary --only-section=.text $< $@
	@test "$$(stat -c%s $@)" -eq 512 || \
		(echo "boot sector must be exactly 512 bytes" >&2; exit 1)

$(OUT)/fadal-kernel.img: $(OUT)/boot.bin $(OUT)/kernel.bin
	dd if=/dev/zero of=$@ bs=512 count=$$((1 + $(BOOT_SECTORS))) status=none
	dd if=$(OUT)/boot.bin of=$@ conv=notrunc status=none
	dd if=$(OUT)/kernel.bin of=$@ bs=512 seek=1 conv=notrunc status=none

check: $(OUT)/fadal-kernel.img
	@command -v $(QEMU) >/dev/null || \
		(echo "$(QEMU) is required for the boot smoke test" >&2; exit 1)
	@timeout 5s $(QEMU) -accel tcg -drive format=raw,file=$(OUT)/fadal-kernel.img \
		-serial stdio -display none -no-reboot -no-shutdown > $(OUT)/smoke.log 2>&1 || \
		test $$? -eq 124
	@grep -q "FADAL KERNEL ONLINE" $(OUT)/smoke.log
	@grep -q "memory: page allocation self-test passed" $(OUT)/smoke.log
	@echo "Fadal Kernel boot smoke test passed"

run: $(OUT)/fadal-kernel.img
	$(QEMU) -accel tcg -drive format=raw,file=$(OUT)/fadal-kernel.img \
		-serial stdio -display none -no-reboot -no-shutdown

clean:
	rm -rf $(OUT)