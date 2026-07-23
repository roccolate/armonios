#include "libkarm/arena.h"

#include <stdint.h>

#include "libkarm/errno.h"
#include "libkarm/syscall.h"

static void mapped_arena_clear(kli_arena_t *arena) {
    if (arena == 0) {
        return;
    }

    arena->base = 0;
    arena->capacity = 0;
    arena->offset = 0;
    arena->mapping_size = 0;
}

long kli_arena_map(kli_arena_t *arena, size_t capacity) {
    long address;

    if (arena == 0 || capacity == 0) {
        return KLI_INVAL;
    }

    mapped_arena_clear(arena);
    address = kli_mmap(0, capacity, 0);
    if (address < 0) {
        return address;
    }

    if (kli_arena_init(arena, (void *)(uintptr_t)address, capacity) != 0) {
        (void)kli_munmap((uintptr_t)address, capacity);
        return KLI_INVAL;
    }

    arena->mapping_size = capacity;
    return 0;
}

long kli_arena_destroy(kli_arena_t *arena) {
    long status;

    if (arena == 0 || arena->base == 0 || arena->mapping_size == 0) {
        return KLI_INVAL;
    }

    status = kli_munmap((uintptr_t)arena->base, arena->mapping_size);
    if (status < 0) {
        return status;
    }

    mapped_arena_clear(arena);
    return 0;
}
