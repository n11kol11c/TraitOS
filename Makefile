# TraitOS build system (KumOS-style)
# Requires: clang (freestanding cross-target), lld, nasm, GNU make
# Optional (for `make iso`): xorriso + grub-mkrescue

CC       = clang
TARGET   = x86_64-none-elf
CFLAGS   = --target=$(TARGET) -std=gnu11 -ffreestanding -O2 -Wall -Wextra \
           -fno-stack-protector -fno-builtin -fno-pic -fno-pie \
           -fno-asynchronous-unwind-tables -fcf-protection=none \
           -mcmodel=kernel -mno-red-zone -mgeneral-regs-only \
           -mno-sse -mno-sse2 -mno-mmx -mno-80387 \
           -nostdlib -Isrc

ASM      = nasm
ASMFLAGS = -f elf64

LD := $(shell command -v ld.lld 2>/dev/null)
ifeq ($(LD),)
LD := $(shell ls /opt/homebrew/opt/lld/bin/ld.lld 2>/dev/null)
endif
ifeq ($(LD),)
$(error ld.lld not found. macOS: `brew install lld` | Linux: `apt install lld`)
endif

LDFLAGS  = -m elf_x86_64 -T linker.ld -z noexecstack

GRUB_MKR = $(shell command -v x86_64-elf-grub-mkrescue 2>/dev/null || command -v grub-mkrescue 2>/dev/null || command -v grub2-mkrescue 2>/dev/null)

KERN_OBJS = \
    boot/boot.o boot/gdt.o boot/isr_stubs.o boot/task_switch.o \
    src/tstring.o src/vga.o src/serial.o \
    src/teyboard.o src/gdt.o src/idt.o src/timer.o src/tmalloc.o src/tpmm.o src/tvmm.o \
    src/tsec.o \
    src/tfs.o src/ttarfs.o src/tprocfs.o src/tsysfs.o src/tsh.o \
    src/ttask.o src/tipc.o \
    src/ternel.o

USER_PROGS =

all: traitos.bin

boot/%.o: boot/%.asm
	@$(ASM) $(ASMFLAGS) $< -o $@

src/%.o: src/%.c
	@$(CC) $(CFLAGS) -c $< -o $@

traitos.bin: $(KERN_OBJS) linker.ld
	$(LD) $(LDFLAGS) $(KERN_OBJS) -o $@
	@echo "Kernel: $$(ls -lh $@ | awk '{print $$5}')"

RAMFS_IMG = boot/ramfs.img

$(RAMFS_IMG): $(shell find initrd -type f 2>/dev/null)
	@mkdir -p boot
	@tar --format=ustar -cf $(RAMFS_IMG) -C initrd .
	@echo "RAMFS: $$(ls -lh $(RAMFS_IMG) | awk '{print $$5}')"

user-programs: $(USER_PROGS)

iso: traitos.bin $(RAMFS_IMG)
	@mkdir -p iso/boot/grub
	@cp traitos.bin iso/boot/traitos.bin && cp grub.cfg iso/boot/grub/grub.cfg
	@cp $(RAMFS_IMG) iso/boot/ramfs.img
	@$(GRUB_MKR) -o traitos.iso iso/
	@echo "ISO: $$(ls -lh traitos.iso | awk '{print $$5}')"

run: iso
	qemu-system-x86_64 -cdrom traitos.iso -serial stdio -m 256M

HOST_CC ?= cc

smoke: $(RAMFS_IMG)
	@mkdir -p build
	$(HOST_CC) -std=gnu11 -O1 -Wall -Isrc tests/fs_smoke.c \
	    src/tfs.c src/ttarfs.c src/tstring.c src/tprocfs.c src/tsysfs.c \
	    -o build/fs_smoke
	./build/fs_smoke
	$(HOST_CC) -std=gnu11 -O1 -Wall -Isrc tests/shell_smoke.c \
	    src/tsh.c src/tstring.c -o build/shell_smoke
	./build/shell_smoke
	$(HOST_CC) -std=gnu11 -O1 -Wall -Isrc tests/tsec_smoke.c \
	    -o build/tsec_smoke
	./build/tsec_smoke
	$(HOST_CC) -std=gnu11 -O1 -Wall -Isrc tests/ttask_smoke.c \
	    -o build/ttask_smoke
	./build/ttask_smoke
	$(HOST_CC) -std=gnu11 -O1 -Wall -Isrc tests/ipc_smoke.c \
	    -o build/ipc_smoke
	./build/ipc_smoke

clean:
	@rm -rf build
	@rm -f $(KERN_OBJS) traitos.bin traitos.iso $(RAMFS_IMG)

.PHONY: all iso run smoke clean user-programs
