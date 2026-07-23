#ifndef ARMONIOS_PROGRAMS_LIBKARM_ARENA_H
#define ARMONIOS_PROGRAMS_LIBKARM_ARENA_H

#include <stddef.h>
#include <stdint.h>

/*
 * Monotonic arena for short-lived userland allocations.
 *
 * The arena owns no hidden global state. Callers may initialize it over an
 * existing buffer or request a private SYS_MMAP-backed region through
 * kli_arena_map(). Individual allocations are not freed; reset discards all
 * allocations at once.
 */
typedef struct {
    uint8_t *base;
    size_t capacity;
    size_t offset;
    size_t mapping_size;
} kli_arena_t;

int kli_arena_init(kli_arena_t *arena, void *buffer, size_t capacity);
void *kli_arena_alloc(kli_arena_t *arena, size_t size);
void *kli_arena_alloc_aligned(kli_arena_t *arena, size_t size,
                              size_t alignment);
size_t kli_arena_remaining(const kli_arena_t *arena);
void kli_arena_reset(kli_arena_t *arena);

/* Return 0 on success or a negative ArmoniOS status. */
long kli_arena_map(kli_arena_t *arena, size_t capacity);
long kli_arena_destroy(kli_arena_t *arena);

#endif
