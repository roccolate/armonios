#include "libkarm/buffer.h"

#include <stdint.h>

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
