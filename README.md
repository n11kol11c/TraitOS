# TraitOS

A from-scratch **x86_64** hobby operating system that boots from a USB drive,
loads entirely into **RAM**, and never writes to persistent storage — power off
and it is gone. Built with C and x86_64 assembly, boots via **GRUB2
(Multiboot2)** on both **BIOS and UEFI**, and ships a **TUI** for now.

> **Status:** interactive console with a command line (`help`, `clear`,
> `uptime`, `info`, `alloc`, `paging`, `heap`, `die`...) and M2 mostly done:
> Multiboot2 memory map, bitmap PMM, `vmm` paging (map/unmap), and a real
> kernel heap (`tmalloc`).
> **Disclaimer:** educational project, *not* production security software.

## Goals

- Boot from USB with GRUB2 / Multiboot2 on BIOS **and** UEFI.
- **RAM-resident and amnesic**: after boot, nothing is ever written back to the
  boot media (no swap, no persistent mounts, no writes).
- **TUI-first**: a terminal UI; no GUI for now.
- Small, auditable, dependency-free codebase: C + assembly only.

## Building

### Requirements

| Tool            | macOS                                     | Debian/Ubuntu                          |
| --------------- | ----------------------------------------- | -------------------------------------- |
| Cross C compiler | `clang` (Xcode CLT)                       | `clang`                                |
| ELF linker      | `brew install lld`                        | `apt install lld`                      |
| Assembler       | `brew install nasm`                       | `apt install nasm`                     |
| ISO tooling     | `brew install xorriso mtools x86_64-elf-grub` | `apt install xorriso mtools grub-pc-bin grub-common grub-efi-amd64-bin grub-efi-ia32-bin` |

`./build.sh deps` installs them automatically on macOS (Homebrew) or
Debian/Ubuntu/Fedora/Arch.

### Build

```sh
make            # -> traitos.bin
make iso        # -> traitos.iso  (BIOS + UEFI bootable)
./build.sh      # banner + build + ISO
```

### Run (emulator, optional)

Requires QEMU:

```sh
./build.sh run
# or: qemu-system-x86_64 -cdrom traitos.iso -serial stdio -m 256M
```

### Boot from a real USB stick

**Warning: this completely wipes the target device.** Double-check the device
identifier first.

```sh
./build.sh usb /dev/disk2     # macOS
./build.sh usb /dev/sdb       # Linux
```

## Project layout

```
boot/                  boot-time assembly
  boot.asm             Multiboot2 header, paging, long-mode switch, entry
  gdt.asm              boot-time GDT + segment reload helper
src/                   kernel sources (flat modules)
  ternel.c             entry point + console tprintf
  tstring.{c,h}        libc-string subset (t-prefixed)
  vga.{c,h}            VGA text-mode driver (incl. CP437 box-drawing glyphs)
  serial.{c,h}         COM1 UART + tlog()
  teyboard.{c,h}       PS/2 keyboard
  gdt.{c,h}            GDT reload glue
  idt.{c,h}            IDT + PIC remap, IRQ dispatch
  timer.{c,h}          PIT timer
  tmalloc.{c,h}        heap (stub)
user/                  userspace programs (planned, M6)
linker.ld              kernel link script (loads at 1 MiB)
grub.cfg               GRUB2 menu config
Makefile               build system
build.sh               deps/build/iso/run/usb/clean wrapper
docs/PLAN.md           detailed architecture + roadmap
.github/workflows/     CI (builds the ISO on every push)
```

## Roadmap

1. Boot + console (this milestone)
2. Interrupts: IDT, PIC/APIC, PIT, PS/2 keyboard
3. Memory management: Multiboot2 memory map, bitmap PMM, paging, heap
4. TUI: input, line editor, command interpreter
5. RAM rootfs: initrd in RAM (tarfs), no persistent mounts
6. Security hardening: NX, SMEP/SMAP, W^X, KASLR, strict syscall surface
7. Processes: scheduler, IPC, user mode

## Acknowledgments

Project structure and conventions modeled after
[KumOS](https://github.com/todorw/KumOS) (GPL-3.0), a 32-bit OS by a friend.
TraitOS is x86_64 and all source code is original, written for this project
under the MIT License.

## License

[MIT](LICENSE)
