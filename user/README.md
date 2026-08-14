# user/ — userspace programs (planned)

This mirrors KumOS's `user/` layout. Ring-3 user-mode programs (a `kush`-style
shell, demo ELFs) land with the processes/ELF-loader milestone (M6). ELF
binaries and their embedded blobs will live here.

Until then the interactive shell lives in-kernel (`src/ternel.c` line editor
+ command table, `src/tsh.c` tokenizer/env/history), so the TUI shell you see
today is ring-0. M6 splits it into a ring-3 process.
