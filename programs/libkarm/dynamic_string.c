#include "libkarm/dynamic_string.h"

#include <stdint.h>

#include "libkarm/errno.h"
#include "libkarm/string.h"

static void string_clear_all(kli_string_t *string) {
    if (string == 0) {
        return;
    }

    string->buffer.data = 0;
    string->buffer.length = 0;
    string->buffer.capacity = 0;
    string->buffer.arena = 0;
}

static int string_storage_is_valid(const kli_string_t *string) {
    const kli_buffer_t *buffer;
    const kli_arena_t *arena;
    uintptr_t arena_base;
    uintptr_t arena_used_end;
    uintptr_t data_base;
    uintptr_t data_end;

    if (string == 0) {
        return 0;
    }

    buffer = &string->buffer;
    arena = buffer->arena;
    if (arena == 0 || arena->base == 0 ||
        arena->offset > arena->capacity || buffer->data == 0 ||
        buffer->capacity == 0 || buffer->length >= buffer->capacity) {
        return 0;
    }

    arena_base = (uintptr_t)arena->base;
    if (arena_base > UINTPTR_MAX - arena->offset) {
        return 0;
    }
    arena_used_end = arena_base + arena->offset;

    data_base = (uintptr_t)buffer->data;
    if (data_base < arena_base || data_base > arena_used_end ||
        data_base > UINTPTR_MAX - buffer->capacity) {
        return 0;
    }
    data_end = data_base + buffer->capacity;
    if (data_end > arena_used_end) {
        return 0;
    }

    return buffer->data[buffer->length] == 0;
}

static int text_has_nul(const char *text, size_t length) {
    for (size_t i = 0; i < length; i++) {
        if (text[i] == '\0') {
            return 1;
        }
    }

    return 0;
}

static long string_reserve_text(kli_string_t *string, size_t text_capacity) {
    long status;

    if (!string_storage_is_valid(string) || text_capacity == SIZE_MAX) {
        return KLI_INVAL;
    }

    status = kli_buffer_reserve(&string->buffer, text_capacity + 1U);
    if (status < 0) {
        return status;
    }

    string->buffer.data[string->buffer.length] = 0;
    return 0;
}

long kli_string_init_capacity(kli_string_t *string, kli_arena_t *arena,
                              size_t initial_capacity) {
    long status;

    if (string == 0) {
        return KLI_INVAL;
    }

    string_clear_all(string);
    if (initial_capacity == SIZE_MAX) {
        return KLI_INVAL;
    }

    status = kli_buffer_init(&string->buffer, arena);
    if (status < 0) {
        return status;
    }

    status = kli_buffer_reserve(&string->buffer, initial_capacity + 1U);
    if (status < 0) {
        string_clear_all(string);
        return status;
    }

    string->buffer.length = 0;
    string->buffer.data[0] = 0;
    return 0;
}

long kli_string_init(kli_string_t *string, kli_arena_t *arena) {
    return kli_string_init_capacity(string, arena, 0);
}

long kli_string_assign_n(kli_string_t *string, const char *text,
                         size_t length) {
    long status;

    if (!string_storage_is_valid(string)) {
        return KLI_INVAL;
    }
    if (length != 0 && text == 0) {
        return KLI_INVAL;
    }
    if (length == SIZE_MAX || (length != 0 && text_has_nul(text, length))) {
        return KLI_INVAL;
    }

    status = string_reserve_text(string, length);
    if (status < 0) {
        return status;
    }

    if (length != 0) {
        memmove(string->buffer.data, text, length);
    }
    string->buffer.length = length;
    string->buffer.data[length] = 0;
    return 0;
}

long kli_string_assign(kli_string_t *string, const char *text) {
    if (text == 0) {
        return KLI_INVAL;
    }

    return kli_string_assign_n(string, text, strlen(text));
}

long kli_string_append_n(kli_string_t *string, const char *text,
                         size_t length) {
    size_t required;
    long status;

    if (!string_storage_is_valid(string)) {
        return KLI_INVAL;
    }
    if (length == 0) {
        return 0;
    }
    if (text == 0 || text_has_nul(text, length) ||
        string->buffer.length > SIZE_MAX - length) {
        return KLI_INVAL;
    }

    required = string->buffer.length + length;
    if (required == SIZE_MAX) {
        return KLI_INVAL;
    }

    status = string_reserve_text(string, required);
    if (status < 0) {
        return status;
    }

    memmove(string->buffer.data + string->buffer.length, text, length);
    string->buffer.length = required;
    string->buffer.data[required] = 0;
    return 0;
}

long kli_string_append(kli_string_t *string, const char *text) {
    if (text == 0) {
        return KLI_INVAL;
    }

    return kli_string_append_n(string, text, strlen(text));
}

long kli_string_append_char(kli_string_t *string, char value) {
    if (value == '\0') {
        return KLI_INVAL;
    }

    return kli_string_append_n(string, &value, 1);
}

void kli_string_clear(kli_string_t *string) {
    if (!string_storage_is_valid(string)) {
        return;
    }

    string->buffer.length = 0;
    string->buffer.data[0] = 0;
}

const char *kli_string_cstr(const kli_string_t *string) {
    if (!string_storage_is_valid(string)) {
        return 0;
    }

    return (const char *)string->buffer.data;
}

size_t kli_string_length(const kli_string_t *string) {
    if (!string_storage_is_valid(string)) {
        return 0;
    }

    return string->buffer.length;
}

size_t kli_string_capacity(const kli_string_t *string) {
    if (!string_storage_is_valid(string)) {
        return 0;
    }

    return string->buffer.capacity - 1U;
}
