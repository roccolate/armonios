#include "libkarm/buffer.h"

#include <stdint.h>

#include "libkarm/dynamic_string.h"
#include "libkarm/errno.h"
#include "libkarm/string.h"

#define KLI_BUFFER_MIN_CAPACITY 64U

static void buffer_clear_all(kli_buffer_t *buffer) {
    if (buffer == 0) {
        return;
    }

    buffer->data = 0;
    buffer->length = 0;
    buffer->capacity = 0;
    buffer->arena = 0;
}

static int buffer_is_valid(const kli_buffer_t *buffer) {
    if (buffer == 0 || buffer->arena == 0 || buffer->arena->base == 0 ||
        buffer->arena->offset > buffer->arena->capacity ||
        buffer->length > buffer->capacity) {
        return 0;
    }
    if ((buffer->capacity == 0 && buffer->data != 0) ||
        (buffer->capacity != 0 && buffer->data == 0)) {
        return 0;
    }

    return 1;
}

long kli_buffer_init(kli_buffer_t *buffer, kli_arena_t *arena) {
    if (buffer == 0) {
        return KLI_INVAL;
    }

    buffer_clear_all(buffer);
    if (arena == 0 || arena->base == 0 || arena->offset > arena->capacity) {
        return KLI_INVAL;
    }

    buffer->arena = arena;
    return 0;
}

long kli_buffer_init_capacity(kli_buffer_t *buffer, kli_arena_t *arena,
                              size_t initial_capacity) {
    long status = kli_buffer_init(buffer, arena);

    if (status < 0 || initial_capacity == 0) {
        return status;
    }

    return kli_buffer_reserve(buffer, initial_capacity);
}

long kli_buffer_reserve(kli_buffer_t *buffer, size_t minimum_capacity) {
    size_t new_capacity;
    uint8_t *new_data;

    if (!buffer_is_valid(buffer)) {
        return KLI_INVAL;
    }
    if (minimum_capacity <= buffer->capacity) {
        return 0;
    }

    new_capacity = buffer->capacity;
    if (new_capacity < KLI_BUFFER_MIN_CAPACITY) {
        new_capacity = KLI_BUFFER_MIN_CAPACITY;
    }

    while (new_capacity < minimum_capacity) {
        if (new_capacity > SIZE_MAX / 2U) {
            new_capacity = minimum_capacity;
            break;
        }
        new_capacity *= 2U;
    }
    if (new_capacity < minimum_capacity) {
        return KLI_NOMEM;
    }

    new_data = (uint8_t *)kli_arena_alloc(buffer->arena, new_capacity);
    if (new_data == 0) {
        return KLI_NOMEM;
    }

    if (buffer->length != 0) {
        memmove(new_data, buffer->data, buffer->length);
    }
    buffer->data = new_data;
    buffer->capacity = new_capacity;
    return 0;
}

long kli_buffer_append(kli_buffer_t *buffer, const void *data, size_t size) {
    size_t required;
    long status;

    if (!buffer_is_valid(buffer)) {
        return KLI_INVAL;
    }
    if (size == 0) {
        return 0;
    }
    if (data == 0 || buffer->length > SIZE_MAX - size) {
        return KLI_INVAL;
    }

    required = buffer->length + size;
    status = kli_buffer_reserve(buffer, required);
    if (status < 0) {
        return status;
    }

    memmove(buffer->data + buffer->length, data, size);
    buffer->length = required;
    return 0;
}

long kli_buffer_append_byte(kli_buffer_t *buffer, uint8_t value) {
    return kli_buffer_append(buffer, &value, 1);
}

void kli_buffer_clear(kli_buffer_t *buffer) {
    if (!buffer_is_valid(buffer)) {
        return;
    }

    buffer->length = 0;
}

size_t kli_buffer_remaining(const kli_buffer_t *buffer) {
    if (!buffer_is_valid(buffer)) {
        return 0;
    }

    return buffer->capacity - buffer->length;
}

static void string_clear_all(kli_string_t *string) {
    if (string == 0) {
        return;
    }

    buffer_clear_all(&string->buffer);
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
    if (!buffer_is_valid(buffer) || buffer->data == 0 ||
        buffer->capacity == 0 || buffer->length >= buffer->capacity) {
        return 0;
    }

    arena = buffer->arena;
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
