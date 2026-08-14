#ifndef GDT_H
#define GDT_H

void gdt_init(void);
void gdt_reload(void); /* defined in boot/gdt.asm */

#endif
