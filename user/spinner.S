; TraitOS user-mode program: "spinner" - a live counter task
; Prints its pid and a running count, sleeping between lines. Running two of
; these (plus the kernel demos) shows ring-3 tasks preempted and scheduled
; side by side with kernel threads.

BITS 64

section .text

global _start
_start:
    xor ebx, ebx                ; count

.loop:
    mov rax, 1                  ; write("user ")
    mov rdi, 1
    lea rsi, [rel s_user]
    mov rdx, 5
    int 0x80

    mov rax, 3                  ; getpid()
    int 0x80
    mov rdi, rax
    call print_dec

    mov rax, 1                  ; write(": ")
    mov rdi, 1
    lea rsi, [rel s_sep]
    mov rdx, 2
    int 0x80

    mov rdi, rbx                ; write(count)
    call print_dec

    mov rax, 1                  ; write("\n")
    mov rdi, 1
    lea rsi, [rel s_nl]
    mov rdx, 1
    int 0x80

    mov rax, 5                  ; sleep(25 ticks = 250 ms)
    mov rdi, 25
    int 0x80

    inc ebx
    jmp .loop

; rdi = value; prints its decimal form to fd 1 (digits built on the ring-3
; stack, which the kernel validates and reads with SMAP disabled)
print_dec:
    mov rax, rdi
    mov rcx, 10
    mov r8, rsp                 ; remember the top of the digit buffer

.push:
    xor edx, edx
    div rcx
    add dl, '0'
    dec rsp
    mov [rsp], dl
    test rax, rax
    jnz .push

    mov rsi, rsp                ; digits -> write()
    mov rdx, r8
    sub rdx, rsp
    mov rax, 1
    mov rdi, 1
    int 0x80
    mov rsp, r8
    ret

section .rodata

s_user: db "user "
s_sep:  db ": "
s_nl:   db 10
