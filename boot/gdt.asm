; TraitOS — boot-time GDT + segment reload helper
; Mirrors KumOS's gdt_flush.asm: a separate asm module for GDT work.

BITS 32

section .rodata
align 8
global gdt64
gdt64:
    dq 0x0000000000000000                ; null descriptor
gdt64_code: equ $ - gdt64                ; 0x08
    dq 0x0020980000000000                ; 64-bit code (L=1)
gdt64_data: equ $ - gdt64                ; 0x10
    dq 0x0000920000000000                ; data

global gdt64_ptr
gdt64_ptr:
    dw gdt64_ptr - gdt64 - 1             ; limit
    dq gdt64                             ; base

BITS 64
section .text
; void gdt_reload(void) — reload data segments after an lgdt.
global gdt_reload
gdt_reload:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret
