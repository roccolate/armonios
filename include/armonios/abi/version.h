#ifndef ARMONIOS_INCLUDE_ARMONIOS_ABI_VERSION_H
#define ARMONIOS_INCLUDE_ARMONIOS_ABI_VERSION_H

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
#define ARMONIOS_ABI_MINOR 1U

/* Keep this macro valid in both C expressions and preprocessor #if checks. */
#define ARMONIOS_ABI_VERSION_ENCODE(major, minor) \
    (((major) & 0xffffU) << 16U | ((minor) & 0xffffU))

#define ARMONIOS_ABI_VERSION \
    ARMONIOS_ABI_VERSION_ENCODE(ARMONIOS_ABI_MAJOR, ARMONIOS_ABI_MINOR)

#if ARMONIOS_ABI_VERSION != 0x00010001U
#error "ABI drift: update the public ABI version intentionally"
#endif

_Static_assert(ARMONIOS_ABI_VERSION == 0x00010001U,
               "ABI drift: update the public ABI version intentionally");

#endif
