# user/ — userspace programs (planned)

This mirrors KumOS's `user/` layout. Ring-3 user-mode programs (a `kush`-style
shell, demo ELFs) land with the user-mode milestone (M6c). ELF binaries and
their embedded blobs will live here.

Until then the interactive shell lives in-kernel (`src/ternel.c` line editor
+ command table, `src/tsh.c` tokenizer/env/history), so the TUI shell you see
today is ring-0. M6a (already in) adds a preemptive round-robin kernel
scheduler on top of that shell task; M6c splits the shell into a ring-3
process.
