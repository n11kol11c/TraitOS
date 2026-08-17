# TrigerOS build system (KumOS-style)
# Requires: clang (freestanding cross-target), lld, nasm, GNU make
# Optional (for `make iso`): xorriso + grub-mkrescue

all: trigeros.bin

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
    src/keyboard.o src/gdt.o src/tss.o src/idt.o src/timer.o src/tmalloc.o src/tpmm.o src/tvmm.o \
    src/tsec.o \
    src/tsys.o \
    src/telf.o \
    src/tfs.o src/ttarfs.o src/tprocfs.o src/tsysfs.o src/tsh.o \
    src/ttask.o src/tipc.o \
    src/ternel.o

# User-mode programs: assembled, linked at VMM_USER_BASE, then embedded into
# the kernel image as raw blobs (ld -r -b binary) so `run <program>` can load
# them into a ring-3 address space at boot.
USER_PROGS = user/hello.elf user/spinner.elf user/kush.elf
USER_TEXT  = 0x8000000000

build/user_blobs.o: $(USER_PROGS)
	@mkdir -p build
	$(LD) -m elf_x86_64 -r -b binary $(USER_PROGS) -o $@

USER_CFLAGS = --target=$(TARGET) -std=gnu11 -ffreestanding -O2 -Wall \
              -fno-stack-protector -fno-builtin -fpie -mcmodel=large \
              -fno-asynchronous-unwind-tables -fcf-protection=none \
              -mno-red-zone -mgeneral-regs-only \
              -mno-sse -mno-sse2 -mno-mmx -mno-80387 \
              -nostdlib -Isrc -Iuser

user/%.o: user/%.c
	@$(CC) $(USER_CFLAGS) -c $< -o $@

user/%.o: user/%.S
	@$(ASM) $(ASMFLAGS) $< -o $@

user/%.elf: user/%.o user/user.ld
	@$(LD) -m elf_x86_64 -z noexecstack -T user/user.ld -o $@ $<

# C user programs: link startup stub + C object
user/kush.elf: user/kush_start.o user/kush.o user/user.ld
	@$(LD) -m elf_x86_64 -pie -z noexecstack -T user/user.ld -o $@ user/kush_start.o user/kush.o

KERN_OBJS += build/user_blobs.o

boot/%.o: boot/%.asm
	@$(ASM) $(ASMFLAGS) $< -o $@

src/%.o: src/%.c
	@$(CC) $(CFLAGS) -c $< -o $@

trigeros.bin: $(KERN_OBJS) linker.ld
	$(LD) $(LDFLAGS) $(KERN_OBJS) -o $@
	@echo "Kernel: $$(ls -lh $@ | awk '{print $$5}')"

RAMFS_IMG = boot/ramfs.img

$(RAMFS_IMG): $(shell find initrd -type f 2>/dev/null)
	@mkdir -p boot
	@tar --format=ustar -cf $(RAMFS_IMG) -C initrd .
	@echo "RAMFS: $$(ls -lh $(RAMFS_IMG) | awk '{print $$5}')"

user-programs: $(USER_PROGS)

iso: trigeros.bin $(RAMFS_IMG)
	@mkdir -p iso/boot/grub
	@cp trigeros.bin iso/boot/trigeros.bin && cp grub.cfg iso/boot/grub/grub.cfg
	@cp $(RAMFS_IMG) iso/boot/ramfs.img
	@$(GRUB_MKR) -o trigeros.iso iso/
	@echo "ISO: $$(ls -lh trigeros.iso | awk '{print $$5}')"

run: iso
	qemu-system-x86_64 -cdrom trigeros.iso -serial stdio -m 256M

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
	$(HOST_CC) -std=gnu11 -O1 -Wall -Isrc tests/elf_smoke.c \
	    -o build/elf_smoke
	./build/elf_smoke

clean:
	@rm -rf build
	@rm -f $(KERN_OBJS) trigeros.bin trigeros.iso $(RAMFS_IMG)
	@rm -f user/hello.o user/hello.elf user/spinner.o user/spinner.elf user/kush.o user/kush.elf user/kush_start.o

.PHONY: all iso run smoke clean user-programs

.PRECIOUS: user/%.o user/%.elf
