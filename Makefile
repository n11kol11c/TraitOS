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
    boot/boot.o boot/gdt.o boot/isr_stubs.o \
    src/kstring.o src/vga.o src/serial.o \
    src/keyboard.o src/gdt.o src/idt.o src/timer.o src/kmalloc.o \
    src/kernel.o

USER_PROGS =

all: traitos.bin

boot/%.o: boot/%.asm
	@$(ASM) $(ASMFLAGS) $< -o $@

src/%.o: src/%.c
	@$(CC) $(CFLAGS) -c $< -o $@

traitos.bin: $(KERN_OBJS)
	$(LD) $(LDFLAGS) $(KERN_OBJS) -o $@
	@echo "Kernel: $$(ls -lh $@ | awk '{print $$5}')"

user-programs: $(USER_PROGS)

iso: traitos.bin
	@mkdir -p iso/boot/grub
	@cp traitos.bin iso/boot/traitos.bin && cp grub.cfg iso/boot/grub/grub.cfg
	@$(GRUB_MKR) -o traitos.iso iso/
	@echo "ISO: $$(ls -lh traitos.iso | awk '{print $$5}')"

run: iso
	qemu-system-x86_64 -cdrom traitos.iso -serial stdio -m 256M

clean:
	@rm -f $(KERN_OBJS) traitos.bin traitos.iso

.PHONY: all iso run clean user-programs
