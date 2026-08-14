/* TraitOS Kernel Linker Script */
ENTRY(_start)

PHYS_BASE = 0x100000;
VIRT_BASE = 0xFFFFFFFF80000000;

SECTIONS {
    . = PHYS_BASE;

    .multiboot ALIGN(8) : {
        KEEP(*(.multiboot))
    }

    .boot ALIGN(8) : {
        *(.boot)
    }

    .boot.bss ALIGN(4096) (NOLOAD) : {
        *(.boot.bss)
    }

    /* physical end of the low boot area; higher-half sections load after it */
    _boot_phys_end = ALIGN(4096);

    . = VIRT_BASE + _boot_phys_end;

    .text ALIGN(4096) : AT(_boot_phys_end) {
        *(.text*)
    }

    .rodata ALIGN(4096) : AT(ALIGN(LOADADDR(.text) + SIZEOF(.text), 4096)) {
        *(.rodata*)
    }

    .data ALIGN(4096) : AT(ALIGN(LOADADDR(.rodata) + SIZEOF(.rodata), 4096)) {
        *(.data*)
    }

    .bss ALIGN(4096) : AT(ALIGN(LOADADDR(.data) + SIZEOF(.data), 4096)) {
        *(COMMON)
        *(.bss*)
    }

    /DISCARD/ : {
        *(.note.gnu.property)
        *(.comment)
        *(.eh_frame*)
    }

    . = ALIGN(4096);
    _kernel_virtual_end = .;
    _kernel_virtual_start = VIRT_BASE + _boot_phys_end;
    _kernel_phys_start = PHYS_BASE;
    _kernel_phys_end = LOADADDR(.bss) + SIZEOF(.bss);
}
