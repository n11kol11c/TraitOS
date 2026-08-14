<div align="center">

# TraitOS

### A from-scratch, RAM-resident x86_64 operating system

Boots from a USB stick, loads entirely into RAM, and **vanishes on power-off**.
No disk writes. No forensic trail. Just you and the kernel.

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-x86__64-blue.svg)]()
[![Language](https://img.shields.io/badge/Language-C11%20+%20NASM-lightgrey.svg)]()
[![Boot](https://img.shields.io/badge/Boot-GRUB2%20Multiboot2-green.svg)]()
[![Memory](https://img.shields.io/badge/Memory-Higher--Half%20kernel-brightgreen.svg)]()
[![RAM-resident](https://img.shields.io/badge/RAM--resident-Yes-brightgreen.svg)]()
[![Build](https://github.com/n11kol11c/TraitOS/actions/workflows/ci.yml/badge.svg)](https://github.com/n11kol11c/TraitOS/actions/workflows/ci.yml)

**Runs on real hardware (BIOS + UEFI) and in emulators — no libc, no runtime, no dependencies.**

</div>

---

## What is TraitOS?

TraitOS is a hobby operating system written **from scratch** in C11 and x86_64
assembly. It is built around one uncompromising idea: **nothing written back to
the boot media, ever.**

- The kernel, drivers, and (eventually) the root filesystem live in RAM.
- After boot the OS never mounts the boot media, never swaps, never writes —
  unmount the stick and the machine holds no storage at all.
- Power off = complete wipe. That is the whole point.

It boots through **GRUB2 / Multiboot2** on both **BIOS and UEFI**, switches to
long mode from assembly, and hands control to a higher-half C kernel with its
own physical memory manager, paging, per-process address spaces, and a heap —
all surfaced through an interactive TUI shell.

> **Status:** early but real. M0 (boot + console), M1 (interrupts + keyboard),
> and M2 (memory management) are complete. See [Roadmap](#roadmap).

---

## Highlights

- **Multiboot2 + GRUB2** — boots the same ISO on BIOS and UEFI hardware.
- **Higher-half kernel** — C code linked at `0xFFFFFFFF80000000`, loaded low by
  GRUB, mapped into place by assembly before the first C instruction runs.
- **Bitmap physical memory manager** — first-fit allocation, 4 GiB ceiling,
  low-memory allocation for page tables (`tpmm_alloc_low`).
- **Paging with per-process address spaces** — independent PML4 trees that share
  the kernel view; CR3 switching with TLB invalidation (`vmm_aspace_*`).
- **Real heap** — first-fit free list + bump allocation on top of the PMM
  (`tmalloc`), with magic-block corruption checks.
- **Interrupts & input** — IDT for 32 exceptions + 16 IRQs, PIC remap, 100 Hz
  PIT, PS/2 keyboard with shift/caps and a ring buffer.
- **Interactive shell** — `help`, `info`, `alloc`, `paging`, `heap`, `aspace`,
  and a `die` command that deliberately page-faults to show the exception path.
- **CI on every push** — GitHub Actions builds `traitos.bin` + `traitos.iso`.

---

## Usage showcase

Booting `traitos.iso` in QEMU or on hardware drops you into a shell:

```
GRUB 2.06 ── "TraitOS (RAM-resident)"

===============================================
 TraitOS v0.5.0 - RAM-resident, amnesic OS
===============================================

 Type 'help' for a list of commands.
   > help
 commands:
   help      list available commands
   clear     clear the screen
   uptime    seconds since boot
   ver       kernel version
   info      system + memory info
   alloc     allocate and free 8 physical pages
   paging    map a page at high vaddr, poke it, unmap
   heap      exercise the kernel heap (tmalloc/tfree)
   echo      print the rest of the line
   die       divide by zero (panic demo)
   aspace    demo per-process address spaces (page tables)

   > info
 arch      : x86_64
 boot      : GRUB2 / Multiboot2 (BIOS + UEFI)
 storage   : none - runs entirely from RAM
 memory    : 255 MiB total, 250 MiB available
 frames    : 63706 free / 1402 used
 heap      : 64 KiB mapped, 0 KiB used, 0 blocks

   > aspace
 2 address spaces, same virtual page mapped in both
  in space A: wrote 0x11111111, read back 0x11111111
  in space B: wrote 0x22222222, read back 0x22222222
  in space A: still 0x11111111 (per-process isolation works)
 spaces torn down, frames returned

   > uptime
 uptime: 12s (1247 ticks)
```

`die` intentionally divides by zero — the IDT catches it, prints an exception
report, and the machine halts. It is the closest thing to a panic demo an OS
can ship.
