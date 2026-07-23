#ifndef ARMONIOS_INCLUDE_ARMONIOS_ABI_VERSION_H
#define ARMONIOS_INCLUDE_ARMONIOS_ABI_VERSION_H

#include <stdint.h>

/*
 * Public ArmoniOS ABI revision.
 *
 * MAJOR changes are reserved for deliberately incompatible public ABI changes.
 * MINOR changes are append-only: new syscalls, flags, structures, or fields may
 * be added without changing existing numbers, layouts, or semantics.
 *
 * This is a compile-time contract identifier, not a runtime capability query.
 * Applications must not infer that every future feature exists from the minor
 * value alone. A capability/query syscall can be added later without changing
 * this header's compatibility rules.
 */
#define ARMONIOS_ABI_MAJOR 1U
#define ARMONIOS_ABI_MINOR 0U

#define ARMONIOS_ABI_VERSION_ENCODE(major, minor) \
    ((((uint32_t)(major)) << 16U) | ((uint32_t)(minor) & 0xffffU))

#define ARMONIOS_ABI_VERSION \
    ARMONIOS_ABI_VERSION_ENCODE(ARMONIOS_ABI_MAJOR, ARMONIOS_ABI_MINOR)

_Static_assert(ARMONIOS_ABI_VERSION == 0x00010000U,
               "ABI drift: update the public ABI version intentionally");

#endif
