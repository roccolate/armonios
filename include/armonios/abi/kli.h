#ifndef ARMONIOS_INCLUDE_ARMONIOS_ABI_KLI_H
#define ARMONIOS_INCLUDE_ARMONIOS_ABI_KLI_H

#include <stdint.h>

/*
 * KLI1 flat executable-image format.
 *
 * A KLI1 file is a position-independent, little-endian AArch64 image. The
 * header is stored at byte zero and entry offsets are relative to that byte.
 * The loader copies exactly image_size bytes and begins execution at the
 * selected entry offset.
 *
 * KLI1 carries code and read-only data only. Mutable static .data/.bss state is
 * forbidden by the SDK linker contract and must be obtained at runtime through
 * the public memory API. KLI1 contains no relocation table; builders must not
 * leave load-time relocations or absolute pointers that require fixups.
 */
#define ARM_KLI1_MAGIC       0x31494c4bU
#define ARM_KLI1_MAX_ENTRIES 8U
#define ARM_KLI1_HEADER_SIZE 80U

typedef struct {
    uint32_t magic;
    uint16_t header_size;
    uint16_t entry_count;
    uint64_t image_size;
    uint64_t entry_offsets[ARM_KLI1_MAX_ENTRIES];
} arm_kli1_header_t;

_Static_assert(sizeof(arm_kli1_header_t) == ARM_KLI1_HEADER_SIZE,
               "ABI drift: arm_kli1_header_t must remain 80 bytes");
_Static_assert(ARM_KLI1_MAGIC == 0x31494c4bU,
               "ABI drift: KLI1 magic changed");

#endif
