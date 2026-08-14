#ifndef TELF_H
#define TELF_H

#include "telf_core.h"
#include "tvmm.h"

#include <stdint.h>
#include <stddef.h>

/* Load a parsed ELF plan into a fresh user address space `as`: allocate
 * low physical frames, map them at the segment vaddrs (user + W/NX from
 * p_flags), and copy the file bytes / zero the bss tail through the
 * physmap window. Returns TELF_OK or a TELF_E_* error. */
int telf_load(vmm_aspace_t *as, const uint8_t *blob, size_t size,
              telf_plan_t *plan);

#endif
