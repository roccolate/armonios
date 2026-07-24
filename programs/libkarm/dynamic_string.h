#ifndef ARMONIOS_PROGRAMS_LIBKARM_DYNAMIC_STRING_H
#define ARMONIOS_PROGRAMS_LIBKARM_DYNAMIC_STRING_H

#include <stddef.h>

#include "libkarm/arena.h"
#include "libkarm/buffer.h"

/*
 * Null-terminated text stored in a caller-owned monotonic arena.
 *
 * `buffer.length` is the number of text bytes and excludes the trailing NUL.
 * The byte at `buffer.data[buffer.length]` is always NUL for a valid string.
 * Text is treated as bytes: UTF-8 may be stored, but libkarm does not normalize
 * or decode it. Embedded NUL bytes are rejected.
 */
typedef struct {
    kli_buffer_t buffer;
} kli_string_t;

long kli_string_init(kli_string_t *string, kli_arena_t *arena);
long kli_string_init_capacity(kli_string_t *string, kli_arena_t *arena,
                              size_t initial_capacity);
long kli_string_assign(kli_string_t *string, const char *text);
long kli_string_assign_n(kli_string_t *string, const char *text, size_t length);
long kli_string_append(kli_string_t *string, const char *text);
long kli_string_append_n(kli_string_t *string, const char *text, size_t length);
long kli_string_append_char(kli_string_t *string, char value);
void kli_string_clear(kli_string_t *string);
const char *kli_string_cstr(const kli_string_t *string);
size_t kli_string_length(const kli_string_t *string);
size_t kli_string_capacity(const kli_string_t *string);

#endif
