#ifndef ARMONIOS_INCLUDE_ARMONIOS_ABI_VERSION_H
#define ARMONIOS_INCLUDE_ARMONIOS_ABI_VERSION_H

/*
 * Public ArmoniOS ABI revision.
 *
 * The project remains pre-release. Additive ABI work may land while this value
 * stays at 1.0; the first official release will establish the compatibility
 * baseline. After that point, MAJOR changes are incompatible and MINOR changes
 * are append-only additions that preserve existing numbers, layouts, and
 * semantics.
 *
 * This is a compile-time contract identifier, not a runtime capability query.
 * Applications must not infer feature availability from the version alone. A
 * capability/query syscall can be added later without changing this header's
 * compatibility rules.
 */
#define ARMONIOS_ABI_MAJOR 1U
#define ARMONIOS_ABI_MINOR 0U

/* Keep this macro valid in both C expressions and preprocessor #if checks. */
#define ARMONIOS_ABI_VERSION_ENCODE(major, minor) \
    (((major) & 0xffffU) << 16U | ((minor) & 0xffffU))

#define ARMONIOS_ABI_VERSION \
    ARMONIOS_ABI_VERSION_ENCODE(ARMONIOS_ABI_MAJOR, ARMONIOS_ABI_MINOR)

#if ARMONIOS_ABI_VERSION != 0x00010000U
#error "ABI drift: update the public ABI version intentionally"
#endif

_Static_assert(ARMONIOS_ABI_VERSION == 0x00010000U,
               "ABI drift: update the public ABI version intentionally");

#endif
