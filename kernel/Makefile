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
	-Wall -Wextra -Werror -O2 -nostdinc
LDFLAGS := -m elf_i386 -T linker.ld -nostdlib

.PHONY: all clean check independence run run-vga

all: $(OUT)/fadal-kernel.img

$(OUT):
	mkdir -p $(OUT)

$(OUT)/boot.o: boot.S | $(OUT)
	$(AS) --32 -o $@ $<

$(OUT)/kernel.o: kernel.c fat12.h ata.h slab.h vfs.h | $(OUT)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OUT)/fat12.o: fat12.c fat12.h | $(OUT)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OUT)/ata.o: ata.c ata.h | $(OUT)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OUT)/slab.o: slab.c slab.h | $(OUT)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OUT)/vfs.o: vfs.c vfs.h fat12.h | $(OUT)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OUT)/fsh.o: fsh.c | $(OUT)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OUT)/fsh.elf: $(OUT)/fsh.o fsh.ld
	$(LD) -m elf_i386 -T fsh.ld -nostdlib -o $@ $(OUT)/fsh.o

$(OUT)/fsh.bin: $(OUT)/fsh.elf
	$(OBJCOPY) -O binary $< $@

$(OUT)/fsh_blob.o: $(OUT)/fsh.bin
	$(OBJCOPY) -I binary -O elf32-i386 -B i386 $< $@

$(OUT)/interrupts.o: interrupts.S | $(OUT)
	$(AS) --32 -o $@ $<

$(OUT)/kernel.elf: $(OUT)/kernel.o $(OUT)/fat12.o $(OUT)/ata.o $(OUT)/slab.o $(OUT)/vfs.o $(OUT)/fsh_blob.o $(OUT)/interrupts.o linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OUT)/kernel.o $(OUT)/fat12.o $(OUT)/ata.o $(OUT)/slab.o $(OUT)/vfs.o $(OUT)/fsh_blob.o $(OUT)/interrupts.o

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

independence: $(OUT)/kernel.elf
	@test -z "$$(nm -u $(OUT)/kernel.elf)" || \
		(echo "kernel has unresolved host/runtime symbols" >&2; nm -u $(OUT)/kernel.elf; exit 1)
	@! rg -n '#include[[:space:]]*<' . --glob '*.c' --glob '*.h' --glob '!out/**'
	@echo "Fadal independence guard passed: freestanding, no host headers, no unresolved symbols"

check: independence $(OUT)/fadal-kernel.img
	@command -v $(QEMU) >/dev/null || \
		(echo "$(QEMU) is required for the boot smoke test" >&2; exit 1)
	@timeout 5s $(QEMU) -accel tcg -boot c -drive format=raw,file=$(OUT)/fadal-kernel.img \
		-serial file:$(OUT)/smoke.log -display none -no-reboot -no-shutdown >/dev/null 2>&1 || \
		test $$? -eq 124
		@grep -q "FADAL KERNEL ONLINE" $(OUT)/smoke.log
		@grep -q "memory: BIOS E820 map accepted" $(OUT)/smoke.log
		@grep -q "memory: 16 MiB identity paging online" $(OUT)/smoke.log
		@grep -q "memory: slab caches online (process, fat12_dirent)" $(OUT)/smoke.log
		@grep -q "memory: slab allocation/reuse self-test passed" $(OUT)/smoke.log
		@grep -q "memory: process slab exhausted cleanly at 8 objects" $(OUT)/smoke.log
		@grep -q "memory: heap stress passed (30 blocks; allocator limit verified)" $(OUT)/smoke.log
		@grep -Eq "filesystem: FAT12 formatted; KERNEL.TXT written|vfs: FAT12 backend mounted; root directory recognized" $(OUT)/smoke.log
		@grep -q "vfs: FAT12 backend mounted; root directory recognized (2 entries)" $(OUT)/smoke.log
			@grep -Eq "filesystem: allocated clusters 3; free clusters 56|filesystem: mounted KERNEL.TXT verified" $(OUT)/smoke.log
		@grep -Eq "disk: ATA LBA28 persisted FAT12 volume|disk: FAT12 remount read-only verification passed" $(OUT)/smoke.log
		@grep -Eq "filesystem: ATA read-back verified KERNEL.TXT|filesystem: mounted KERNEL.TXT verified" $(OUT)/smoke.log
	@grep -q "memory: dynamic heap self-test passed (2 pages)" $(OUT)/smoke.log
	@grep -q "memory: page allocation self-test passed" $(OUT)/smoke.log
	@grep -q "timer: PIT IRQ0 online at 100 Hz" $(OUT)/smoke.log
	@grep -q "syscalls: int 0x80 ABI gate online" $(OUT)/smoke.log
		@grep -q "process: dynamic PID allocator online; 2 processes ready" $(OUT)/smoke.log
			@grep -q "keyboard: PS/2 IRQ1 interactive shell online" $(OUT)/smoke.log
			@grep -q "userspace: FSH.BIN loaded from FAT12; ring-3 entry ready" $(OUT)/smoke.log
			@grep -q "shell: FSH disk executable selected" $(OUT)/smoke.log
				@grep -q "Fadal TTY ready; FSH.BIN owns user shell" $(OUT)/smoke.log
				@grep -q "fsh: disk executable online" $(OUT)/smoke.log
				@grep -q "commands: help mount ls cat KERNEL.TXT exit" $(OUT)/smoke.log
	@grep -q "syscall: ring-3 entry" $(OUT)/smoke.log
	@grep -q "syscall: get_ticks dispatch" $(OUT)/smoke.log
		@grep -q "syscall: get_pid dispatch" $(OUT)/smoke.log
			@grep -q "syscall: read dispatch; blocking TTY queue read" $(OUT)/smoke.log
		@grep -q "syscall: write dispatch; user buffer validated" $(OUT)/smoke.log
		@grep -q "syscall: open dispatch; VFS path checked" $(OUT)/smoke.log
		@grep -q "syscall: exec dispatch; process created" $(OUT)/smoke.log
	@dd if=$(OUT)/fadal-kernel.img bs=512 skip=$(FAT12_DATA_LBA) count=1 status=none | grep -a -q "FADAL FAT12 write"
	@echo "Fadal Kernel boot smoke test passed"

run: $(OUT)/fadal-kernel.img
	$(QEMU) -accel tcg -boot c -drive format=raw,file=$(OUT)/fadal-kernel.img \
		-serial stdio -display none -no-reboot -no-shutdown

run-vga: $(OUT)/fadal-kernel.img
	$(QEMU) -accel tcg -boot c -drive format=raw,file=$(OUT)/fadal-kernel.img \
		-serial stdio -no-reboot -no-shutdown

clean:
	rm -rf $(OUT)
