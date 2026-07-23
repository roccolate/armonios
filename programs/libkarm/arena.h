#ifndef ARMONIOS_PROGRAMS_LIBKARM_ARENA_H
#define ARMONIOS_PROGRAMS_LIBKARM_ARENA_H

#include <stddef.h>
#include <stdint.h>

#include "libkarm/errno.h"
#include "libkarm/syscall.h"

#define KLI_ARENA_ALIGNMENT 16U

typedef struct {
    uintptr_t base;
    size_t capacity;
    size_t offset;
} kli_arena_t;

_Static_assert(sizeof(kli_arena_t) == 24,
               "libkarm ABI drift: kli_arena_t");

static inline long kli_arena_init(kli_arena_t *arena, size_t capacity) {
    long mapped;

    if (arena == 0 || capacity == 0) {
        return KLI_INVAL;
    }

    mapped = kli_mmap(0, capacity, 0);
    if (mapped < 0) {
        arena->base = 0;
        arena->capacity = 0;
        arena->offset = 0;
        return mapped;
    }

    arena->base = (uintptr_t)mapped;
    arena->capacity = capacity;
    arena->offset = 0;
    return 0;
}

static inline void *kli_arena_alloc(kli_arena_t *arena, size_t size) {
    size_t aligned;

    if (arena == 0 || arena->base == 0 || size == 0) {
        return 0;
    }

    if (arena->offset > SIZE_MAX - (KLI_ARENA_ALIGNMENT - 1U)) {
        return 0;
    }
    aligned = (arena->offset + (KLI_ARENA_ALIGNMENT - 1U)) &
              ~(size_t)(KLI_ARENA_ALIGNMENT - 1U);

    if (aligned > arena->capacity || size > arena->capacity - aligned) {
        return 0;
    }

    arena->offset = aligned + size;
    return (void *)(arena->base + aligned);
}

static inline void *kli_arena_alloc_zero(kli_arena_t *arena, size_t size) {
    uint8_t *memory = (uint8_t *)kli_arena_alloc(arena, size);

    if (memory == 0) {
        return 0;
    }

    for (size_t i = 0; i < size; i++) {
        memory[i] = 0;
    }
    return memory;
}

static inline size_t kli_arena_remaining(const kli_arena_t *arena) {
    if (arena == 0 || arena->base == 0 || arena->offset > arena->capacity) {
        return 0;
    }
    return arena->capacity - arena->offset;
}

static inline void kli_arena_reset(kli_arena_t *arena) {
    if (arena != 0 && arena->base != 0) {
        arena->offset = 0;
    }
}

static inline long kli_arena_destroy(kli_arena_t *arena) {
    long status;

    if (arena == 0) {
        return KLI_INVAL;
    }
    if (arena->base == 0) {
        arena->capacity = 0;
        arena->offset = 0;
        return 0;
    }

    status = kli_munmap(arena->base, arena->capacity);
    if (status < 0) {
        return status;
    }

    arena->base = 0;
    arena->capacity = 0;
    arena->offset = 0;
    return 0;
}

#endif
