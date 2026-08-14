#include "gdt.h"

/* The boot-time GDT lives in boot/gdt.asm. gdt_init() will install a full
   C-defined GDT (TSS, user-mode segments) with the interrupts milestone. */
void gdt_init(void)
{
    gdt_reload();
}
