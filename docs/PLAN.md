# TraitOS — Development Plan

## Vision

TraitOS is a privacy- and security-oriented operating system that **runs
entirely from RAM**:

- Boots from a USB stick (or ISO) using GRUB2.
- Kernel, drivers, and root filesystem are loaded into RAM during boot.
- After boot, the OS **never writes to the boot media**: no swap, no
  persistent mounts, no writes. Unmount the stick and the machine holds no
  storage at all.
- Power off = complete wipe.

The UI is a **terminal (TUI)** for now; a framebuffer console is a later
milestone.

## Locked design decisions

| Decision          | Choice                                    | Why |
| ----------------- | ----------------------------------------- | --- |
| Architecture      | x86_64 only                               | scope, tooling |
| Boot protocol     | GRUB2 via Multiboot2                      | BIOS + UEFI from one image |
| Firmware          | BIOS **and** UEFI (one ISO)               | broadest reach |
| Language          | C11 (clang, freestanding) + NASM asm      | auditable, no deps |
| Kernel placement  | loaded at 1 MiB, identity-mapped          | simplest correct start |
| Toolchain         | clang `--target=x86_64-none-elf`, ld.lld, nasm | no GCC cross build needed |

The target later adds a higher-half mapping (`0xFFFFFFFF80000000`) for the
kernel, keeping the 1 MiB identity map only for the boot handoff.

## Structure heritage

The project layout, build scripts, and module conventions mirror
[KumOS](https://github.com/todorw/KumOS) (GPL-3.0): a flat `src/` of
self-contained `.c/.h` modules, `boot/` assembly, top-level
`linker.ld`/`grub.cfg`/`build.sh`, and a `user/` userspace directory.
KumOS is 32-bit (Multiboot1); TraitOS is x86_64 (Multiboot2), so the boot
chain is written from scratch. TraitOS source is original MIT code — the
KumOS API shapes and conventions are adapted, not copied verbatim.

## Directory map (current)

```
boot/                    boot assembly (boot.asm, gdt.asm), ramfs.img initrd
src/                     flat kernel modules
  ternel.c, tstring.*, vga.*, serial.*
  teyboard.*, gdt.*, idt.*, timer.*, tmalloc.*, tpmm.*, tvmm.*
  tfs.*, ttarfs.*, tprocfs.*, tsysfs.*, tsh.*    (filesystem + shell core)
  tsys.* (syscalls), telf.* (user ELF loader), tss.* (ring-3 stacks)
initrd/                  root filesystem staging (tar'd into boot/ramfs.img)
tests/                   host-side smoke tests (fs, shell, sec, ttask, ipc, elf)
user/                    userspace programs (hello.S, spinner.S) + user.ld
linker.ld                kernel link script (loads at 1 MiB, higher-half VMA)
grub.cfg                 GRUB2 menu config (multiboot2 + module2 initrd)
Makefile                 build system (KumOS-style KERN_OBJS)
build.sh                 deps/build/iso/run/usb/clean wrapper
```

## Milestones

### M0 — Foundation (this commit)
- [x] Project structure, Makefile, build.sh, toolchain, CI
- [x] Multiboot2 header + 32→64-bit long-mode switch (identity map 1 GiB)
- [x] VGA text driver, serial (COM1) logging, `tprintf`/`tlog`
- [x] ISO build (`grub-mkrescue`) and USB writer (dd of hybrid ISO)
- [ ] Boot to a clean TUI console (console banner, halting loop)

### M1 — Interrupts & input
- [x] 64-bit IDT gates for all 32 exceptions + 16 IRQs (boot/isr_stubs.asm)
- [x] 8259A PIC remap to vectors 32-47, EOI dispatch, `irq_register()`
- [x] PIT timer at 100 Hz (`timer_ticks`, uptime logging)
- [x] PS/2 keyboard driver (IRQ1): set-1 scancodes, shift/caps, ring buffer
- [ ] PIC→APIC, TSS/IST (double-fault stack), exception page-fault details

### M2 — Memory management
- [x] Parse Multiboot2 memory map and framebuffer info
- [x] Bitmap **PMM** with page alloc/free (`src/tpmm.c`), first-fit scan
- [x] `vmm` paging: map/unmap 4 KiB pages, tables resolved via the
      higher-half physmap window (`src/tvmm.c`)
- [x] Heap (`tmalloc`) on top of PMM: first-fit free list + bump grow, living
      in a dedicated higher-half virtual region mapped through the VMM (`src/tmalloc.c`)
- [x] Higher-half kernel: linked at `0xFFFFFFFF80000000`, loaded low by GRUB,
      boot.asm maps 1 GiB (identity + higher half) before entering `ternel_main`
- [x] Per-process address spaces (`vmm_aspace_*`): independent PML4 trees that
      clone the kernel view, switch via CR3, with user slots 1..255 free

### M3 — TUI shell
- [x] Kernel command line: `help`, `clear`, `uptime`, `ver`, `info`, `alloc`,
      `echo`, `die` (panic demo) — line editing + backspace in `src/ternel.c`
- [x] Command history with up/down-arrow recall, `!n` / `!!` re-run,
      `#` comments, `"quoted"` arguments, `\` escapes, `$VAR` expansion and
      `set`/`env` — extracted to `src/tsh.c`, host-tested by `make smoke`
- [x] Line editor: left/right arrows, Home/End, Del, mid-line insert
- [x] `whoami`, `halt`, `reboot` (8042 keyboard-controller reset)
- [ ] History persistence across sessions, globbing, pipelines, tab-completion
- [ ] Framebuffer console with simple fonts (optional)

### M4 — RAM filesystem
- [x] GRUB loads an initrd as a Multiboot2 module (`module2 /boot/ramfs.img`)
- [x] ramfs VFS core: node tree, `mkdir -p`, `touch`, `write`, recursive `rm`
      (`src/tfs.c`), virtual nodes with generators for procfs/sysfs
- [x] ustar tar unpacker + initrd loader that maps the module's pages into a
      scratch window, unpacks, and unmaps (`src/ttarfs.c`)
- [x] procfs (`/proc/uptime`, `/version`, `/meminfo`, `/heapinfo`) and sysfs
      (`/sys/memory`, `/frames`, `/kernel`)
- [x] shell commands: `ls`, `cat`, `mkdir`, `touch`, `rm`, `write`, `mount`
- [x] `make` builds `boot/ramfs.img` from `initrd/` (ustar); host-side smoke
      test unpacks the real image and walks/verifies the tree
- [ ] Confirm: boot media never mounted writable

### M5 — Security hardening
- [x] NX pages: EFER.NXE + NX on the identity map, higher-half physmap,
      kernel heap, initrd scratch and demo mappings (`tsec`)
- [x] W^X kernel image: `.text` read-only+executable, `.rodata` read-only,
      everything else non-executable — 4 KiB split of the physmap window,
      pure PTE math in `tsec_core.h`, verified by `make smoke`
- [x] SMEP + SMAP (CR4), enabled at boot, reported by `sec`
- [x] Physical KASLR: image relocated to a randomized 2 MiB slot (RDTSC
      entropy, initrd + used frames excluded); live page tables moved out of
      the overridden window first; the original copy stays reserved
- [x] RAM scrub on `halt`/`reboot` + `scrub` command (free frames < 1 GiB)
- [x] Kernel stack guard page: the identity map's first 2 MiB leaf is split
      to 4 KiB and the page below `.boot.bss` stack is left non-present, so
      a stack overflow faults instead of corrupting the boot page tables
- [ ] Kernel-image *virtual* KASLR (randomize the virtual base) — needs
      `-fPIC` + relocations; deferred to the user-mode milestone
- [ ] Stack canaries / guard pages for the future kernel heap
- [ ] Kernel KASLR via the GRUB `multiboot2` relocatable tag (later)

### M6 — Processes
#### M6a — Preemptive multitasking (done)
- [x] Kernel task table + states (`FREE/READY/RUNNING/EXITED`), idle task wraps
      the boot/main context (`src/ttask.c`, `src/ttask.h`)
- [x] Assembly context switch (`boot/task_switch.asm`): saves callee-saved
      regs + rsp, restores the next task's, resumes via `ret`
- [x] New tasks entered through a iretq frame (`ttask_entry_iret`): the
      initial stack holds the saved block, a return address, and the iretq
      frame; the C entry runs `fn(arg)` then `ttask_exit()`
- [x] PIT-driven preemption: IRQ0 → `ttask_tick()` picks the next task and
      switches mid-handler; PIC EOI is sent before dispatch so the PIC never
      stays blocked across a switch (bug fix)
- [x] Cooperative `ttask_yield()`, round-robin + priority pick (pure math in
      `ttask_core.h`, host-tested by `make smoke`)
- [x] CPU-time accounting (`ticks` per task) + `tasks`/`spawn`/`burst`/`yield`
      shell commands
- [x] Task stacks from `tmalloc` (8 KiB each); exited slots are reused and
      their stacks freed
- [x] Bug fixes (v0.9.1): shared kernel half of the page tables is guarded
      against user-space mapping/unmapping (`tvmm.c`), and `tmalloc`/`tfree`
      + the PMM bitmap allocators run under an IF-critical section so timer
      preemption cannot corrupt them
- [ ] Kernel stack guard pages *per task* (own stacks are on the heap now)
- [ ] Scheduler priority tuning (e.g. idle boost, timeslice scaling)

#### M6b — IPC (done, v0.10.0)
- [x] Blocking state (`TTASK_BLOCKED`) + `ttask_block()`/`ttask_wake()`/
      `ttask_self()`; pick already skips non-READY slots (host-tested)
- [x] Pure IPC core in `tipc_core.h`: bounded message ring, FIFO wait
      queues, recursive mutex — all host-tested by `make smoke`
      (`tests/ipc_smoke.c`, 108 checks)
- [x] Kernel wrappers in `tipc.c`: `tipc_send`/`tipc_recv` block when the
      ring is full/empty, `tipc_mutex_lock`/`unlock` with FIFO wakeups;
      every primitive runs under a cli/sti critical section and
      registration+block is atomic, so no wake is ever lost
- [x] Shell demos: `ipc` (100 messages delivered in order with stamped
      sender ids, blocking send+recv) and `mutex` (3 tasks × 5000
      increments must land on exactly 15000)
- [ ] Semaphore (counting) on top of the mutex primitives

#### M6c — User mode + syscalls (done, v0.11.0)
- [x] Full GDT with user segments + a TSS; `ltr` after load (`src/gdt.c`,
      `src/tss.c`); per-task ring-3 kernel stacks via `tss_set_rsp0()`
- [x] `int 0x80` syscall gate: DPL-3 IDT vector 128, isr stub + C table in
      `src/tsys.c` — `write`/`exit`/`getpid`/`yield`/`sleep`; ring-0
      callers rejected; SMAP user-buffer reads under `stac`/`clac`
- [x] Ring-3 tasks (`ttask_create_user`): user-mode iretq frame, per-task
      address spaces with CR3 switch + RSP0 swap on every pick, ring-3
      faults kill the task instead of panicking
- [x] Pure ELF64 ET_EXEC parser in `telf_core.h` (header/segment math,
      overflow/overlap/out-of-bounds/W^X checks) — host-tested
      (`tests/elf_smoke.c`, 28 checks)
- [x] User ELF loader (`src/telf.c`): `tpmm_alloc_low_contig` frames filled
      through the physmap window, mapped into a fresh address space
- [x] `user/` programs: `hello.S`, `spinner.S` built with `user/user.ld`
      (page-aligned sections, single region at VMM_USER_BASE), embedded in
      the kernel via `ld -r -b binary`; `run` spawns them
- [ ] Deferred: ring-3 syscalls for IPC (mailboxes/mutex from M6b)
- [ ] Deferred: a userspace `kush`-style shell split out of the kernel

## Security model (honest scoping)

TraitOS defends against **software** tampering and data persistence:

- Nothing is written to the boot media after boot → no forensic trail on the
  stick; all ephemeral state lives in RAM.
- Defense in depth: NX, SMEP/SMAP, W^X, physical KASLR, RAM scrub on
  shutdown, minimal attack surface.

TraitOS does **not** defend against:

- **Cold-boot / RAM-dump attacks** (memory persists after power loss for a
  while, especially with DRAM remanence).
- **Evil-maid / hardware tampering** (no TPM chain of trust yet).
- Firmware (UEFI/BIOS) compromise.

These are documented here so the security claims stay honest.

## Toolchain notes

```make
clang --target=x86_64-none-elf -mcmodel=kernel -mno-red-zone
      -mgeneral-regs-only -mno-sse -mno-sse2 -fno-pic -ffreestanding
ld.lld -m elf_x86_64 -T linker.ld
nasm -f elf64
```

- `-mgeneral-regs-only` keeps compiler-generated code free of SSE/MMX so
  interrupt handling can assume clean FPU state.
- ISO: `grub-mkrescue` (needs xorriso + GRUB modules). USB: the ISO is a
  hybrid image, so `dd`ing it to a USB stick boots in BIOS mode; UEFI USB
  layout (ESP + `x86_64-efi` core) is a later milestone.
- macOS uses `x86_64-elf-grub-mkrescue`; Linux uses `grub-mkrescue`; the
  Makefile and build.sh probe both.

## Testing

- CI builds `traitos.bin` + `traitos.iso` on every push, then runs the
  host-side filesystem smoke test (`make smoke`).
- Host-side smoke test (`tests/fs_smoke.c`, `make smoke`) compiles the fs
  modules against the host libc with stubs for kernel-only symbols, unpacks
  the real `boot/ramfs.img`, and verifies the tree — **no emulator required**.
- QEMU (`./build.sh run`) and real-hardware USB boot (`./build.sh usb`) are
  optional visual runs; USB is the primary deployment target.
- Serial output (`-serial stdio`) for kernel logs via `tlog`.
- Later: in-kernel self-tests (`kunit`-style) for pmm/vmm/string.

## Notes / open items

- `lld` may be keg-only on macOS; `Makefile` probes PATH then
  `/opt/homebrew/opt/lld/bin/ld.lld`.
- `user/` mirrors KumOS's userspace dir; populated at M6.
- `iso/` is generated and gitignored, matching KumOS's workflow.
