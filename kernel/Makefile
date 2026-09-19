CC ?= gcc
AS ?= as
LD ?= ld
OBJCOPY ?= objcopy
QEMU ?= qemu-system-i386

OUT := out
BOOT_SECTORS := 112
FAT12_LBA := 113
FAT12_SECTORS := 64
FAT12_DATA_LBA := 117
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

$(OUT)/kernel.o: kernel.c fat12.h ata.h | $(OUT)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OUT)/fat12.o: fat12.c fat12.h | $(OUT)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OUT)/ata.o: ata.c ata.h | $(OUT)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OUT)/interrupts.o: interrupts.S | $(OUT)
	$(AS) --32 -o $@ $<

$(OUT)/kernel.elf: $(OUT)/kernel.o $(OUT)/fat12.o $(OUT)/ata.o $(OUT)/interrupts.o linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OUT)/kernel.o $(OUT)/fat12.o $(OUT)/ata.o $(OUT)/interrupts.o

$(OUT)/kernel.bin: $(OUT)/kernel.elf
	$(OBJCOPY) -O binary $< $@
	@test "$$(stat -c%s $@)" -le "$(KERNEL_BYTES)" || \
		(echo "kernel is larger than $(KERNEL_BYTES) bytes" >&2; exit 1)

$(OUT)/boot.bin: $(OUT)/boot.o
	$(OBJCOPY) -O binary --only-section=.text $< $@
	@test "$$(stat -c%s $@)" -eq 512 || \
		(echo "boot sector must be exactly 512 bytes" >&2; exit 1)

$(OUT)/fadal-kernel.img: $(OUT)/boot.bin $(OUT)/kernel.bin
	dd if=/dev/zero of=$@ bs=512 count=$$((1 + $(BOOT_SECTORS) + $(FAT12_SECTORS))) status=none
	dd if=$(OUT)/boot.bin of=$@ conv=notrunc status=none
	dd if=$(OUT)/kernel.bin of=$@ bs=512 seek=1 conv=notrunc status=none

check: $(OUT)/fadal-kernel.img
	@command -v $(QEMU) >/dev/null || \
		(echo "$(QEMU) is required for the boot smoke test" >&2; exit 1)
	@timeout 5s $(QEMU) -accel tcg -boot c -drive format=raw,file=$(OUT)/fadal-kernel.img \
		-serial file:$(OUT)/smoke.log -display none -no-reboot -no-shutdown >/dev/null 2>&1 || \
		test $$? -eq 124
	@grep -q "FADAL KERNEL ONLINE" $(OUT)/smoke.log
	@grep -q "memory: BIOS E820 map accepted" $(OUT)/smoke.log
	@grep -q "filesystem: FAT12 formatted; KERNEL.TXT written" $(OUT)/smoke.log
	@grep -q "filesystem: allocated clusters 1; free clusters 59" $(OUT)/smoke.log
	@grep -q "disk: ATA LBA28 persisted FAT12 volume" $(OUT)/smoke.log
	@grep -q "filesystem: ATA read-back verified KERNEL.TXT" $(OUT)/smoke.log
	@grep -q "memory: dynamic heap self-test passed (2 pages)" $(OUT)/smoke.log
	@grep -q "memory: page allocation self-test passed" $(OUT)/smoke.log
	@grep -q "timer: PIT IRQ0 online at 100 Hz" $(OUT)/smoke.log
	@grep -q "syscalls: int 0x80 ABI gate online" $(OUT)/smoke.log
	@grep -q "process: dynamic PID allocator online; 2 processes ready" $(OUT)/smoke.log
	@grep -q "syscall: ring-3 entry" $(OUT)/smoke.log
	@grep -q "syscall: get_ticks dispatch" $(OUT)/smoke.log
	@grep -q "syscall: get_pid dispatch" $(OUT)/smoke.log
	@grep -q "syscall: exit dispatch; process marked exited" $(OUT)/smoke.log
	@dd if=$(OUT)/fadal-kernel.img bs=512 skip=$(FAT12_DATA_LBA) count=1 status=none | grep -a -q "FADAL FAT12 write"
	@echo "Fadal Kernel boot smoke test passed"

run: $(OUT)/fadal-kernel.img
	$(QEMU) -accel tcg -boot c -drive format=raw,file=$(OUT)/fadal-kernel.img \
		-serial stdio -display none -no-reboot -no-shutdown

clean:
	rm -rf $(OUT)
