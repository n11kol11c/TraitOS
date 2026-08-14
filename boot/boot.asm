; TraitOS — boot entry (x86_64, Multiboot2)
; Mirrors KumOS's boot/ layout: boot.asm holds the entry + long-mode switch,
; gdt.asm holds the boot-time GDT.

BITS 32

GDT_CODE equ 0x08
GDT_DATA equ 0x10

section .multiboot
align 8
mb_header_start:
    dd 0xE85250D6                        ; magic: Multiboot2
    dd 0                                 ; architecture: i386 (valid for x86_64)
    dd mb_header_end - mb_header_start
    dd 0x100000000 - (0xE85250D6 + 0 + (mb_header_end - mb_header_start))

    ; framebuffer tag: let GRUB pick a linear framebuffer
    dw 5                                 ; type = framebuffer
    dw 0                                 ; flags
    dd 20                                ; size
    dd 0                                 ; width  (0 = preferred)
    dd 0                                 ; height
    dd 0                                 ; depth

    ; end tag
    dw 0
    dw 0
    dd 8
mb_header_end:

section .bss
align 4096
p4_table:
    resb 4096
p3_table:
    resb 4096
p2_table:
    resb 4096
stack_bottom:
    resb 16384
stack_top:

section .text
global _start
extern gdt64_ptr
extern kernel_main

_start:
    mov esp, stack_top

    ; eax = Multiboot2 magic, ebx = pointer to boot info
    cmp eax, 0x36d76289
    jne .halt

    call setup_paging
    lgdt [gdt64_ptr]
    jmp GDT_CODE:long_mode

.halt:
    cli
    hlt
    jmp .halt

; Identity-map the first 1 GiB with 2 MiB pages, enable PAE and long mode.
setup_paging:
    mov eax, p3_table
    or eax, 0x3                          ; present | writable
    mov [p4_table], eax

    mov eax, p2_table
    or eax, 0x3
    mov [p3_table], eax

    mov ecx, 0
.set_entry:
    mov eax, ecx
    shl eax, 21                          ; frame address = index * 2 MiB
    or eax, 0x83                         ; present | writable | 2 MiB page
    mov [p2_table + ecx * 8], eax
    inc ecx
    cmp ecx, 512
    jne .set_entry

    ; PAE on
    mov eax, cr4
    or eax, 0x20
    mov cr4, eax

    ; EFER.LME = long mode enable
    mov ecx, 0xC0000080
    rdmsr
    or eax, 0x100
    wrmsr

    ; load page table base
    mov eax, p4_table
    mov cr3, eax

    ; paging on + protected mode on
    mov eax, cr0
    or eax, 0x80000000
    or eax, 0x1
    mov cr0, eax
    ret

BITS 64
section .text
long_mode:
    mov ax, GDT_DATA
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov rax, stack_top
    mov rsp, rax

    mov rdi, rbx                         ; rdi = multiboot2 info pointer

    call kernel_main

.hang:
    cli
    hlt
    jmp .hang
