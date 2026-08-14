#ifndef TSEC_H
#define TSEC_H

#include <stdint.h>

/* CPU feature control (x86-64). Call once at boot, after tpmm_init/vmm_init. */
void sec_enable_nxe(void);       /* EFER.NXE: makes the NX page bit active */
void sec_enable_smep_smap(void); /* CR4.SMEP + CR4.SMAP */
void sec_harden(void);           /* NXE + SMEP/SMAP + NX sweep + W^X split */

/* KASLR: relocate the loaded kernel image to a randomized physical slot.
 * Returns 1 when relocated, 0 when it stayed at its load address. */
int sec_kaslr_relocate(uintptr_t mbi);

/* RAM sanitization: zero every free physical frame below 1 GiB.
 * Returns the number of frames scrubbed. */
uint32_t sec_scrub_ram(void);

/* Introspection for the `sec` command. */
uint64_t sec_read_efer(void);
uint64_t sec_read_cr4(void);
uint64_t sec_read_cr0(void);
uintptr_t sec_kernel_phys_base(void);   /* 0 = not relocated (image at 1 MiB) */
int      sec_wx_enforced(void);
int      sec_nx_enforced(void);

#endif
