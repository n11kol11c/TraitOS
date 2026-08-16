/* TrigerOS user-program linker script: everything lands in PML4 slot 1
 * (VMM_USER_BASE), so a single contiguous load at 0x8000000000 with no
 * stray header segment. `run` maps the PT_LOADs into a ring-3 aspace. */
ENTRY(_start)

PHDRS {
    text PT_LOAD FLAGS(5);   /* R + X */
    rodata PT_LOAD FLAGS(4); /* R */
    data  PT_LOAD FLAGS(6);  /* R + W */
}

SECTIONS {
    . = 0x8000000000;

    .text : { *(.text*) } :text
    . = ALIGN(4096);
    .rodata : { *(.rodata*) } :rodata
    . = ALIGN(4096);
    .data : { *(.data*) } :data
    .bss : { *(.bss*) *(COMMON) } :data
}
