# user/ — ring-3 programs

`hello.S` and `spinner.S` are the first ring-3 programs. `make` assembles
each with NASM, links it with `user/user.ld` (everything lands in a single
region at `0x8000000000` — PML4 slot 1, the user area — with page-aligned
text/rodata segments and no stray header load), and embeds both ELFs into
`build/user_blobs.o` with `ld -r -b binary`. The kernel addresses them
through `_binary_user_<name>_elf_start` and runs them with `run <name>`.

Programs never call the BIOS, the VGA, or the keyboard. They talk to the
kernel through the `int 0x80` gate in `src/tsys.c`:

| syscall | rax | args (rdi, rsi, rdx)  |
| ------- | --- | --------------------- |
| write   | 1   | fd, buf, len          |
| exit    | 2   | code                  |
| getpid  | 3   | —                     |
| yield   | 4   | —                     |
| sleep   | 5   | ticks (100 Hz)        |

- `hello.S` — writes `hello from user mode!\n` and exits.
- `spinner.S` — loops: `write` a line, `getpid`, decimal-prints a counter
  (digits built on the ring-3 stack), `sleep` 25 ticks.

The ring-3 shell remains a future milestone (M6c "deferred"); today the TUI
shell still lives in the kernel (`src/ternel.c` line editor + command table,
`src/tsh.c` tokenizer/env/history).
