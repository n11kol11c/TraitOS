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
own physical memory manager, paging, per-process address spaces, a heap, and a
RAM filesystem — all surfaced through an interactive TUI shell.

> **Status:** early but real. M0 (boot + console), M1 (interrupts + keyboard),
> M2 (memory management), M3 (TUI shell), M4 (RAM filesystem), M5 (security
> hardening), and M6a (preemptive multitasking) are complete; M6 (IPC, user
> mode) is next. See [Roadmap](#roadmap).

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
- **RAM filesystem** — GRUB loads an initrd as a Multiboot2 module; a ustar
  unpacker builds a ramfs VFS on boot (`tfs`), with `procfs` and `sysfs`
  virtual trees layered on top.
- **Preemptive multitasking (M6a)** — a PIT-driven round-robin kernel
  scheduler with per-task stacks and CPU-time accounting. `spawn`/`burst`
  create tasks, `yield` hands over the CPU cooperatively, `tasks` lists
  everyone. IPC and user mode are next (M6b/M6c).
- **Concurrency hardening (v0.9.1)** — the heap, the physical-memory bitmap,
  and the VMM address-space API were audited for the new scheduler: PML4
  slots 0 and 256..511 (the shared kernel view) can no longer be mapped or
  unmapped from a user address space, and `tmalloc`/`tfree` plus the PMM
  allocators run under an interrupt-critical section so preemption can't
  tear their free lists apart.
- **CPU hardening (M5)** — NX pages (EFER.NXE) with the heap/stack/data
  mapped non-executable, a W^X split of the kernel image (`.text` is
  read-only+executable, `.rodata` read-only, everything else NX), SMEP +
  SMAP in CR4, **physical KASLR** (the image relocates to a randomized 2 MiB
  slot at boot), a guard page below the kernel stack, and RAM scrubbing on
  `halt`/`reboot` so nothing survives in memory. `sec` verifies all of it at
  runtime.
- **Interrupts & input** — IDT for 32 exceptions + 16 IRQs, PIC remap, 100 Hz
  PIT, PS/2 keyboard with shift/caps and a ring buffer.
- **Interactive shell** — `help`, `info`, `alloc`, `paging`, `heap`, `aspace`,
  filesystem commands (`ls`, `cat`, `mkdir`, `touch`, `rm`, `write`, `mount`),
  a real line editor (left/right arrows, Home/End, Del, mid-line insert),
  history with up/down-arrow recall and `!n` / `!!` re-run, `"quoted"`
  arguments, `$VAR` expansion (`set`/`env`), `#` comments, plus
  `whoami`/`halt`/`reboot` and a `die` command that deliberately page-faults
  to show the exception path.
- **CI on every push** — GitHub Actions builds `traitos.bin` + `traitos.iso`
  and runs the QEMU-free host smoke tests.

---

## Usage showcase

Booting `traitos.iso` in QEMU or on hardware drops you into a shell:

```
GRUB 2.06 ── "TraitOS (RAM-resident)"

===============================================
 TraitOS v0.9.1 - RAM-resident, amnesic OS
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
   hist      show command history (up/down arrows recall)
   env       show environment variables
   set       set an environment variable (KEY=VALUE)
   whoami    print the current user (from $USER)
   halt      halt the CPU (power-off not implemented)
   reboot    reboot via the keyboard controller (8042)
   sec       show + verify CPU hardening (NX, W^X, SMEP/SMAP)
   scrub     zero every free physical frame (amnesia)
   tasks     list scheduler tasks
   spawn     spawn a demo task
   burst     spawn N demo tasks
   yield     yield the CPU to another task
   ls        list a directory (default /)
   cat       print a file (ramfs, procfs, sysfs)
   mkdir     create a directory
   touch     create an empty file
   rm        remove a file or directory
   write     write text into a file
   mount     list mounted filesystems

   > info
 arch      : x86_64
 boot      : GRUB2 / Multiboot2 (BIOS + UEFI)
 storage   : none - runs entirely from RAM
 memory    : 255 MiB total, 250 MiB available
 frames    : 63706 free / 1402 used
 heap      : 64 KiB mapped, 0 KiB used, 0 blocks

   > ls /
 drw- 0  README.txt
 drw- 0  var/
 drw- 0  etc/
 drw- 0  usr/
 drw- 0  sys/
 drw- 0  proc/

   > cat /etc/hostname
 hostname=traitos

   > cat /proc/uptime
 uptime: 3 seconds (334 ticks)

   > mkdir /tmp
   > write /tmp/hello.txt hello from ram
 ok (15 bytes)
   > cat /tmp/hello.txt
 hello from ram

   > mount
  procfs  ->  proc
  sysfs   ->  sys

   > whoami
 root

   > echo "hello, $USER from $HOSTNAME"
 hello, root from traitos

   > !1
   > whoami
 root

   > sec
 EFER   : 0xd01 NXE LME LMA
 CR4    : 0x1006b0 PAE SMEP SMAP
 CR0    : 0x80000013 PE PG
 kaslr  : on (image @ 0x4000000)
 W^X    : enforced
 NX     : enforced
 stack  : guarded
 heap   : mapped non-executable

    > aspace
 2 address spaces, same virtual page mapped in both
  in space A: wrote 0x11111111, read back 0x11111111
  in space B: wrote 0x22222222, read back 0x22222222
  in space A: still 0x11111111 (per-process isolation works)
 spaces torn down, frames returned

    > burst 3
 spawned 3 tasks

    > tasks
  id  name      state    prio  ticks
  0   idle      ready      0    1492
  1   d0        ready      1    1207
  2   d1        ready      1    1215
  3   d2        ready      1    1203
      (d0-d2 spin in the background; every preemption tick is visible here)

    > uptime
 uptime: 12s (1247 ticks)
```

`die` intentionally divides by zero — the IDT catches it, prints an exception
report, and the machine halts. `halt` and `reboot` first scrub every free
physical frame, so nothing is left behind in RAM; `reboot` then pulses the
8042 keyboard controller to restart the machine.

---

## How it works

### Boot flow

Everything before the first line of C is assembly. TraitOS never uses a boot
stub beyond what GRUB2 already provides.

```
 firmware (BIOS/UEFI)
      │
      ▼
 ┌──────────────────────────────────────────────────────────────────┐
 │ GRUB2 reads the Multiboot2 header (first 8 KiB of traitos.bin),  │
 │ loads every PT_LOAD segment at its physical address,             │
 │ zero-fills BSS, loads the initrd as a module, and jumps to the   │
 │ ELF entry point.                                                 │
 └──────────────────────────────────────────────────────────────────┘
      │  e_entry = _start @ 0x100030  (32-bit protected mode, paging OFF)
      ▼
 boot/boot.asm
   • checks the Multiboot2 magic (0x36D76289)
   • zeroes the boot page tables (.boot.bss is NOLOAD — GRUB did not fill it)
   • maps 1 GiB twice: identity (PML4[0]) + higher-half (PML4[511])
   • enables PAE, sets EFER.LME, loads CR3, flips CR0.PG
      │
      ▼
 long_mode (64-bit)
   • reloads all data segments from the boot GDT
   • `mov rax, ternel_main; call rax`  ── the first higher-half C code
      │
      ▼
 src/ternel.c  ternel_main(mbi)
   • VGA → serial → GDT → IDT/PIC → PIT → keyboard → PMM → VMM → heap
   • mounts procfs/sysfs, unpacks the initrd module into ramfs, and
     enters the shell loop
```

### Memory model

The kernel is a **higher-half** kernel: the C code is linked far away from the
low physical addresses it is loaded at, and a 1 GiB physmap window maps every
physical page of low memory into the higher half.

```
  VIRTUAL ADDRESS (per address space)              PHYSICAL
 ─────────────────────────────────────────      ─────────────────────
  0xFFFFFFFFC0000000   heap base (grows up)
  0xFFFFFFFF8010F000   kernel .data/.bss      ──►  loaded by GRUB at
  0xFFFFFFFF8010E000   kernel .rodata                physical 1 MiB
  0xFFFFFFFF8010A000   kernel .text
  0xFFFFFFFF80000000   physmap window base      ─►  phys 0x00000000
 ─────────────────────────────────────────      ─────────────────────
  0x0000008000000000   first user slot (PML4[1])    (per-process)
  0x0000000000000000   identity map (shared slot 0) ─► phys 0
```

Key rules:

- **PML4 slots 0 and 256..511** are the *kernel view*: slot 0 identity-maps low
  memory (the boot-time stack, VGA, MMIO and the heap live there), slots
  256..511 map the higher half (kernel image + physmap window).
- **PML4 slots 1..255** are free for *user space*. Every process gets its own
  PML4 (`vmm_aspace_create`) with the kernel view cloned in; user pages are
  mapped into the untouched slots, so one process can never see another's
  memory — the `aspace` shell command proves it live.
- **Page tables live in low memory** (`tpmm_alloc_low`), so the kernel can
  always reach them through the physmap window (`VMM_PHYS_TO_VIRT`) even when
  CR3 points at a freshly created address space.

### Module map

| Module                       | Role                                                        |
| ---------------------------- | ----------------------------------------------------------- |
| `boot/boot.asm`              | Multiboot2 header, boot page tables, long-mode switch, entry |
| `boot/gdt.asm`               | Boot-time GDT (low) + `gdt_reload` (higher half)             |
| `boot/isr_stubs.asm`         | 48 assembly interrupt entry stubs                            |
| `src/ternel.c`               | Entry point, console printf, line editor, command table    |
| `src/ttask.{c,h}`            | Preemptive round-robin scheduler (M6a)                     |
| `src/tsh.{c,h}`              | Host-testable shell: env vars, tokenizer, history, `!n`    |
| `src/tfs.{c,h}`              | Ramfs VFS: node tree, `mkdir -p`, `write`, rm, virtual nodes |
| `src/ttarfs.c`               | ustar unpacker + Multiboot2 initrd module loader             |
| `src/tprocfs.{c,h}`          | `/proc`: uptime, version, meminfo, heapinfo                  |
| `src/tsysfs.{c,h}`           | `/sys`: memory, frames, kernel                               |
| `src/tvmm.{c,h}`             | Paging + per-process address spaces (`vmm_aspace_*`)         |
| `src/tpmm.{c,h}`             | Bitmap physical memory manager, first-fit + `_low`           |
| `src/tmalloc.{c,h}`          | Kernel heap: first-fit free list + bump growth               |
| `src/idt.{c,h}`              | IDT gates, PIC remap, IRQ dispatch, exception reports        |
| `src/timer.{c,h}`            | PIT at 100 Hz, `timer_ticks()`, scheduler tick               |
| `boot/task_switch.asm`       | Context switch (`ttask_switch_ctx`) + iretq entry trampoline |
| `src/teyboard.{c,h}`         | PS/2 keyboard: set-1 scancodes, shift/caps, ring buffer      |
| `src/vga.{c,h}`              | VGA text-mode driver (CP437 glyphs)                          |
| `src/serial.{c,h}`           | COM1 UART + `tlog()` for serial logs                         |
| `src/gdt.{c,h}`              | GDT reload glue                                              |
| `src/tstring.{c,h}`          | Dependency-free libc subset (`tstr*`/`tmem*`/`titoa`/`tsprintf`) |
| `initrd/`                    | Root filesystem staging; `make` tars it into `boot/ramfs.img` |
| `linker.ld`                  | Higher-half link script (VMA/LMA split for GRUB)             |
| `grub.cfg`                   | GRUB2 menu entry                                             |
| `Makefile` / `build.sh`      | Build system + dependency/ISO/USB wrapper                    |

### Design principles

- **No dependencies.** No libc, no toolchain runtimes — every `strlen`, every
  `memcpy` is hand-written in `tstring.c`. The toolchain is stock clang + nasm +
  ld.lld with `-ffreestanding`.
- **Flat and auditable.** One module per concern in `src/`, t-prefixed symbols,
  no magic frameworks. If you can read C, you can read this kernel.
- **Amnesic by construction.** No storage driver is even reachable after boot;
  the only I/O is VGA, serial, keyboard, PIT and the PIC.

---

## Getting started

### Prerequisites

| Tool         | macOS                                    | Debian/Ubuntu                                |
| ------------ | ---------------------------------------- | -------------------------------------------- |
| Compiler     | `clang` (Xcode Command Line Tools)       | `clang`                                      |
| Linker       | `brew install lld`                       | `apt install lld`                            |
| Assembler    | `brew install nasm`                      | `apt install nasm`                           |
| ISO tooling  | `brew install xorriso mtools x86_64-elf-grub` | `apt install xorriso mtools grub-pc-bin grub-common grub-efi-amd64-bin grub-efi-ia32-bin` |
| Emulator *   | `brew install qemu`                      | `apt install qemu-system-x86`                |

\* Only needed to run the image; **not** required to build it.

### Quick start

```sh
./build.sh deps       # installs the toolchain for your OS (macOS/Linux)
./build.sh all        # build kernel + bootable ISO → traitos.iso
./build.sh smoke      # verify the filesystem host-side, no emulator needed
./build.sh run        # optionally boot traitos.iso in QEMU (-serial stdio, 256M)
```

That is it. Thirty seconds later you are in the TraitOS shell.

### Manual build

```sh
make                 # just the kernel → traitos.bin
make iso             # + initrd + bootable ISO → traitos.iso
make smoke           # host-side filesystem verification, no emulator
```

`make iso` also builds the initrd (`boot/ramfs.img`) from `initrd/` via a
ustar archive and tells GRUB to load it with a `module2` line in `grub.cfg`.

### Verify without an emulator

`make smoke` compiles the filesystem modules (`tfs`, `ttarfs`, `tprocfs`,
`tsysfs`), the shell module (`tsh`) and the scheduler core (`ttask_core.h`)
against the host libc with small stubs for kernel-only symbols, then runs four
checks: it unpacks the real `boot/ramfs.img` and verifies the tree (walk, cat,
`write`/`rm`/`mkdir -p`), it exercises the tokenizer (`"quotes"`, `\` escapes,
`$VAR` expansion, `#` comments) plus history (`push`/dedupe/`!n`), it verifies
the M5 security core — for every one of the 512 window pages, with and without
a KASLR relocation, the same pure helper the kernel uses must produce W^X
correct PTEs (code RO+X, rodata RO, data NX, no page writable *and*
executable) — and it checks the scheduler's round-robin/priority pick over
every task-table state (empty, single, wrap-around, priorities, exited). CI
runs it on every push, so the RAM filesystem, shell, security math, and
scheduling math are verified on your machine **and** in GitHub Actions with
zero emulators involved.

### Run in an emulator (optional)

Not required to build or verify the project — the smoke test covers the
filesystem without one. If you have QEMU anyway:

```sh
./build.sh run
# equivalent:
qemu-system-x86_64 -cdrom traitos.iso -serial stdio -m 256M
```

Add `-no-reboot` if you want to inspect the screen after `die`.

### Boot on real hardware (USB)

**This completely wipes the target device.** Double-check the device name.

```sh
./build.sh usb /dev/disk2     # macOS  (the ISO is a hybrid image)
./build.sh usb /dev/sdb       # Linux
```

If you omit the device, `build.sh` lists your disks and prompts. The ISO is a
hybrid GRUB2 image, so it boots in BIOS mode everywhere and in UEFI mode via
the bundled `x86_64-efi` modules.

### `build.sh` reference

| Command                  | What it does                                        |
| ------------------------ | --------------------------------------------------- |
| `./build.sh deps`        | Install the toolchain (Homebrew / apt / dnf / pacman) |
| `./build.sh build`       | Build `traitos.bin`                                 |
| `./build.sh iso`         | Build `traitos.bin` + `traitos.iso`                 |
| `./build.sh smoke`       | Run the host-side filesystem smoke test (no emulator) |
| `./build.sh run`         | Build and boot in QEMU (optional)                   |
| `./build.sh usb [dev]`   | Write `traitos.iso` to a USB stick (erases it)      |
| `./build.sh clean`       | Remove build artifacts                              |
| `./build.sh all`         | Default: build kernel + ISO                         |

### Continuous integration

Pushing to GitHub triggers the `build` workflow (`.github/workflows/ci.yml`):
it installs nasm/lld/grub tooling on Ubuntu, runs `make all`, runs `make iso`,
runs the filesystem smoke test (`make smoke`), and uploads `traitos.iso` as an
artifact. If the badge at the top is green, the latest push builds clean and
the RAM filesystem is verified — no emulator anywhere in the pipeline.

---

## The command line

| Command    | Description                                            |
| ---------- | ------------------------------------------------------ |
| `help`     | list available commands                                 |
| `clear`    | clear the screen                                        |
| `uptime`   | seconds since boot (100 Hz PIT)                         |
| `ver`      | kernel version                                          |
| `info`     | system + memory stats (PMM, heap)                       |
| `alloc`    | allocate and free 8 physical frames                     |
| `paging`   | map a frame at a high virtual address, poke it, unmap   |
| `heap`     | exercise `tmalloc`/`tfree`                              |
| `aspace`   | create 2 address spaces, show same-VA isolation, teardown |
| `hist`     | show command history (up/down arrows recall lines)        |
| `env`      | show environment variables                                |
| `set`      | set an environment variable (`KEY=VALUE`)                 |
| `whoami`   | print the current user (from `$USER`)                     |
| `halt`     | halt the CPU (power-off not implemented)                  |
| `reboot`   | reboot via the 8042 keyboard controller                   |
| `sec`      | show + verify CPU hardening (NX, W^X, SMEP/SMAP, KASLR, guard) |
| `scrub`    | zero every free physical frame (amnesia)                  |
| `tasks`    | list scheduler tasks (name, state, priority, CPU ticks)   |
| `spawn`    | spawn a demo task                                         |
| `burst`    | spawn N demo tasks (default 4)                            |
| `yield`    | yield the CPU to another task                             |
| `ls`       | list a directory (default `/`)                            |
| `cat`      | print a file (ramfs, procfs, sysfs)                       |
| `mkdir`    | create a directory (parents are created on demand)        |
| `touch`    | create an empty file                                      |
| `rm`       | remove a file or directory (recursively)                  |
| `write`    | write text into an existing file                          |
| `mount`    | list mounted filesystems (`procfs`, `sysfs`)              |
| `echo`     | print the rest of the line                                |
| `die`      | divide by zero — demonstrates the exception handler     |

## Roadmap

| Milestone | Status | Scope |
| --------- | ------ | ----- |
| **M0** | Done | Boot, long-mode switch, VGA/serial console, ISO + USB |
| **M1** | Done | IDT, PIC, PIT, PS/2 keyboard |
| **M2** | Done | Memory map, bitmap PMM, paging, heap, higher half, address spaces |
| **M3** | Done | Shell: line editing, history + `!n`, quoting, `$VAR`, env, `reboot` |
| **M4** | Done | RAM filesystem: initrd → ramfs VFS, procfs/sysfs, fs commands |
| **M5** | Done | Hardening: NX, W^X, SMEP/SMAP, physical KASLR, RAM scrub |
| **M6a** | Done | Preemptive multitasking: round-robin scheduler, context switch, `tasks`/`spawn`/`burst`/`yield` |
| **M6b** | Next | IPC between tasks (mailboxes/queues) |
| **M6c** | Later | User mode: TSS, syscalls, user ELF loader |
| **M7** | Later | Multitasking polish: priorities, scheduler tuning, perf counters |

Full per-item checklist: [`docs/PLAN.md`](docs/PLAN.md).

---

## Project layout

```
boot/                  boot-time assembly (boot.asm, gdt.asm, isr_stubs.asm, task_switch.asm)
boot/ramfs.img         initrd, generated by `make` (ustar) — gitignored
initrd/                root filesystem staging (tar'd into the initrd)
src/                   kernel sources (flat, self-contained modules)
  ternel.c             entry point + console + line editor + command table
  tstring.{c,h}        libc-string subset (t-prefixed) + tsprintf
  tsh.{c,h}            shell: env vars, tokenizer, history + !n (host-tested)
  vga.{c,h}            VGA text-mode driver
  serial.{c,h}         COM1 UART + tlog()
  teyboard.{c,h}       PS/2 keyboard
  gdt.{c,h}            GDT reload glue
  idt.{c,h}            IDT + PIC remap, IRQ dispatch
  timer.{c,h}          PIT timer + scheduler tick
  ttask.{c,h}          preemptive round-robin scheduler (M6a)
  ttask_core.h         pure round-robin/priority pick math (host-tested)
  tmalloc.{c,h}        kernel heap
  tpmm.{c,h}           bitmap physical memory manager
  tvmm.{c,h}           paging + per-process address spaces
  tsec.{c,h}           M5 hardening: NX, W^X, SMEP/SMAP, KASLR, guard, scrub
  tsec_core.h          pure W^X/KASLR PTE math (host-tested)
  tfs.{c,h}            ramfs VFS core
  ttarfs.c             ustar tar unpacker + initrd loader
  tprocfs.{c,h}        /proc virtual filesystem
  tsysfs.{c,h}         /sys virtual filesystem
tests/                 host-side smoke tests (make smoke, no emulator)
user/                  userspace programs (planned, M6)
docs/PLAN.md           detailed architecture + roadmap
linker.ld              higher-half kernel link script
grub.cfg               GRUB2 menu config (multiboot2 + module2 initrd)
Makefile               build system
build.sh               deps/build/iso/run/usb/clean wrapper
.github/workflows/     CI (builds kernel + ISO on every push)
```

## Security posture (honest)

TraitOS defends against **software** tampering and data persistence: no writes
to the boot media, no swap, no dump, NX pages, SMEP/SMAP, a W^X kernel image,
physical KASLR, a stack guard page, and RAM scrubbing on shutdown so nothing
survives in memory.

It does **not** defend against cold-boot/RAM-dump attacks, hardware
(evil-maid) tampering, or firmware compromise — because no pure-software OS can
do that. Kernel-image *virtual* KASLR (randomizing the kernel's virtual base)
is also not implemented: it needs position-independent code + relocations,
deferred to the user-mode milestone. See [`docs/PLAN.md`](docs/PLAN.md) for the
full threat model.

> **Disclaimer:** this is an educational project, not production security
> software. Do not store secrets on it.

## License

[MIT](LICENSE). Project structure and conventions modeled after
[KumOS](https://github.com/todorw/KumOS) (GPL-3.0) by a friend — TraitOS is
x86_64, all source is original, and is licensed MIT.

