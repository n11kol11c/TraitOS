; TraitOS — boot entry (x86_64, Multiboot2, higher-half kernel)
; Mirrors KumOS's boot/ layout: boot.asm holds the entry + long-mode switch,
; gdt.asm holds the boot-time GDT.
;
; The C kernel is linked at KERNEL_VBASE (higher half). This file runs at its
; physical load address (1 MiB, identity-mapped) and, once paging is on, maps
; [KERNEL_VBASE, KERNEL_VBASE + 1 GiB) onto physical 0..1 GiB before entering
; the higher-half kernel via ternel_main().

BITS 32

GDT_CODE equ 0x08
GDT_DATA equ 0x10

KERNEL_VBASE equ 0xFFFFFFFF80000000

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

section .boot.bss nobits
align 4096
p4_table:
    resb 4096
p3_table:
    resb 4096
p2_table:
    resb 4096
p3_high:
    resb 4096
p2_high:
    resb 4096
global stack_bottom
stack_bottom:
    resb 16384
global stack_top
stack_top:

section .boot
global _start
extern gdt64_ptr
extern ternel_main

_start:
    mov esp, stack_top

    ; eax = Multiboot2 magic, ebx = pointer to boot info
    cmp eax, 0x36d76289
    jne .halt

    call zero_boot_tables
    call setup_paging
    lgdt [gdt64_ptr]
    jmp GDT_CODE:long_mode

.halt:
    cli
    hlt
    jmp .halt

; .boot.bss is a NOLOAD section, so its memory is not guaranteed zeroed.
; Clear all five page tables before use (5 * 1024 dwords).
zero_boot_tables:
    mov ecx, 0
    mov edi, p4_table
.ztab:
    mov dword [edi + ecx * 4], 0
    inc ecx
    cmp ecx, 5120
    jne .ztab
    ret

; Identity-map the first 1 GiB (2 MiB pages) and map the higher-half region
; [KERNEL_VBASE, KERNEL_VBASE + 1 GiB) onto the same physical 0..1 GiB.
; PML4[511] PDPT[510] PD[0..511] => VIRT 0xFFFFFFFF80000000 + i*2M -> phys i*2M.
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

    ; higher half
    mov eax, p3_high
    or eax, 0x3
    mov [p4_table + 511 * 8], eax

    mov eax, p2_high
    or eax, 0x3
    mov [p3_high + 510 * 8], eax

    mov ecx, 0
.set_high:
    mov eax, ecx
    shl eax, 21
    or eax, 0x83
    mov [p2_high + ecx * 8], eax
    inc ecx
    cmp ecx, 512
    jne .set_high

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
section .boot
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

    mov rax, ternel_main                 ; jump to the higher-half kernel
    call rax

.hang:
    cli
    hlt
    jmp .hang
