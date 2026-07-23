#include "libkarm/arena.h"

#include <stdint.h>

static void arena_clear(kli_arena_t *arena) {
    if (arena == 0) {
        return;
    }

    arena->base = 0;
    arena->capacity = 0;
    arena->offset = 0;
    arena->mapping_size = 0;
}

int kli_arena_init(kli_arena_t *arena, void *buffer, size_t capacity) {
    if (arena == 0) {
        return -1;
    }

    arena_clear(arena);
    if (buffer == 0 || capacity == 0) {
        return -1;
    }

    arena->base = (uint8_t *)buffer;
    arena->capacity = capacity;
    return 0;
}

void *kli_arena_alloc_aligned(kli_arena_t *arena, size_t size,
                              size_t alignment) {
    uintptr_t base;
    uintptr_t current;
    uintptr_t aligned;
    size_t padding;
    size_t remaining;

    if (arena == 0 || arena->base == 0 || size == 0 || alignment == 0 ||
        (alignment & (alignment - 1U)) != 0 ||
        arena->offset > arena->capacity) {
        return 0;
    }

    base = (uintptr_t)arena->base;
    if (base > UINTPTR_MAX - arena->offset) {
        return 0;
    }
    current = base + arena->offset;

    if (current > UINTPTR_MAX - (alignment - 1U)) {
        return 0;
    }
    aligned = (current + alignment - 1U) & ~(uintptr_t)(alignment - 1U);
    padding = (size_t)(aligned - current);
    remaining = arena->capacity - arena->offset;

    if (padding > remaining || size > remaining - padding) {
        return 0;
    }

    arena->offset += padding + size;
    return (void *)aligned;
}

void *kli_arena_alloc(kli_arena_t *arena, size_t size) {
    return kli_arena_alloc_aligned(arena, size, sizeof(uintptr_t));
}

size_t kli_arena_remaining(const kli_arena_t *arena) {
    if (arena == 0 || arena->base == 0 || arena->offset > arena->capacity) {
        return 0;
    }

    return arena->capacity - arena->offset;
}

void kli_arena_reset(kli_arena_t *arena) {
    if (arena == 0 || arena->base == 0) {
        return;
    }

    arena->offset = 0;
}
