; TrigerOS kush startup stub -- calls kush_main() then exits.
BITS 64
section .text
extern kush_main
global _start
_start:
    cld
    call kush_main
    mov rdi, rax          ; return value -> exit code
    mov rax, 2            ; sys_exit
    int 0x80
    jmp _start
