; TraitOS user-mode program: "hello from user mode!"
; Assembled with nasm, linked at VMM_USER_BASE (0x8000000000).
;
; Syscall ABI (int 0x80): rax = number, rdi/rsi/rdx = args, rax = result.
;   1 = write(fd, buf, len)   2 = exit()   3 = getpid()
;   4 = yield()               5 = sleep(ticks)

BITS 64

section .text

global _start
_start:
    mov rax, 1                  ; sys_write
    mov rdi, 1                  ; fd = 1 (console)
    lea rsi, [rel msg]          ; buffer
    mov rdx, [rel msg_len]      ; length
    int 0x80

    mov rax, 2                  ; sys_exit
    int 0x80
    jmp _start                  ; unreachable safety net

section .rodata

msg:     db "hello from user mode!", 10
msg_len: dq $ - msg
