#ifndef ARMONIOS_PROGRAMS_LIBKARM_BUFFER_H
#define ARMONIOS_PROGRAMS_LIBKARM_BUFFER_H

#include <stddef.h>
#include <stdint.h>

#include "libkarm/arena.h"

/*
 * Growable binary buffer backed by a caller-owned monotonic arena.
 *
 * Growth allocates a new block and leaves the old block in the arena. This is
 * intentional: the arena releases storage as a group and libkarm does not
 * pretend individual buffer growth can be freed.
 */
typedef struct {
    uint8_t *data;
    size_t length;
    size_t capacity;
    kli_arena_t *arena;
} kli_buffer_t;

long kli_buffer_init(kli_buffer_t *buffer, kli_arena_t *arena);
long kli_buffer_init_capacity(kli_buffer_t *buffer, kli_arena_t *arena,
                              size_t initial_capacity);
long kli_buffer_reserve(kli_buffer_t *buffer, size_t minimum_capacity);
long kli_buffer_append(kli_buffer_t *buffer, const void *data, size_t size);
long kli_buffer_append_byte(kli_buffer_t *buffer, uint8_t value);
void kli_buffer_clear(kli_buffer_t *buffer);
size_t kli_buffer_remaining(const kli_buffer_t *buffer);

#endif
