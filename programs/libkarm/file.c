#include "libkarm/file.h"

#include <stdint.h>

#include "libkarm/errno.h"
#include "libkarm/syscall.h"

#define KLI_FILE_IO_CHUNK 256U

static int file_arena_is_valid(const kli_arena_t *arena) {
    return arena != 0 && arena->base != 0 && arena->offset <= arena->capacity;
}

static void file_buffer_clear(kli_buffer_t *buffer) {
    if (buffer == 0) {
        return;
    }

    buffer->data = 0;
    buffer->length = 0;
    buffer->capacity = 0;
    buffer->arena = 0;
}

static void file_string_clear(kli_string_t *string) {
    if (string == 0) {
        return;
    }

    file_buffer_clear(&string->buffer);
}

static long file_regular_size(const char *path, size_t *size_out) {
    arm_stat_v2_t stat = {0};
    size_t size;
    long status;

    if (path == 0 || size_out == 0) {
        return KLI_INVAL;
    }

    stat.version = ARM_VFS_METADATA_VERSION;
    stat.struct_size = sizeof(stat);
    status = kli_stat_v2(path, &stat);
    if (status < 0) {
        return status;
    }
    if (stat.type != ARM_FILE_TYPE_REGULAR) {
        return KLI_INVAL;
    }

    size = (size_t)stat.size;
    if ((uint64_t)size != stat.size) {
        return KLI_NOMEM;
    }

    *size_out = size;
    return 0;
}

long kli_fd_write_all(int fd, const void *data, size_t size) {
    const uint8_t *bytes = (const uint8_t *)data;
    size_t offset = 0;

    if (size == 0) {
        return 0;
    }
    if (data == 0) {
        return KLI_INVAL;
    }

    while (offset < size) {
        size_t remaining = size - offset;
        long written = kli_write(fd, bytes + offset, remaining);

        if (written < 0) {
            return written;
        }
        if (written == 0) {
            return KLI_AGAIN;
        }
        if ((uint64_t)written > (uint64_t)remaining) {
            return KLI_INVAL;
        }

        offset += (size_t)written;
    }

    return 0;
}

long kli_file_read_all(const char *path, kli_arena_t *arena,
                       kli_buffer_t *out) {
    kli_buffer_t temporary;
    uint8_t chunk[KLI_FILE_IO_CHUNK];
    size_t arena_offset;
    size_t expected_size;
    long fd;
    long result;

    if (out == 0) {
        return KLI_INVAL;
    }
    file_buffer_clear(out);
    if (path == 0 || !file_arena_is_valid(arena)) {
        return KLI_INVAL;
    }

    arena_offset = arena->offset;
    result = file_regular_size(path, &expected_size);
    if (result < 0) {
        return result;
    }

    result = kli_buffer_init_capacity(&temporary, arena, expected_size);
    if (result < 0) {
        arena->offset = arena_offset;
        return result;
    }

    fd = kli_open(path, ARM_O_RDONLY);
    if (fd < 0) {
        arena->offset = arena_offset;
        return fd;
    }

    for (;;) {
        long count = kli_read((int)fd, chunk, sizeof(chunk));

        if (count < 0) {
            result = count;
            break;
        }
        if (count == 0) {
            result = 0;
            break;
        }
        if ((uint64_t)count > (uint64_t)sizeof(chunk)) {
            result = KLI_INVAL;
            break;
        }

        result = kli_buffer_append(&temporary, chunk, (size_t)count);
        if (result < 0) {
            break;
        }
    }

    {
        long close_status = kli_close((int)fd);
        if (result == 0 && close_status < 0) {
            result = close_status;
        }
    }

    if (result < 0) {
        arena->offset = arena_offset;
        return result;
    }

    *out = temporary;
    return 0;
}

long kli_file_read_text(const char *path, kli_arena_t *arena,
                        kli_string_t *out) {
    kli_string_t temporary;
    char chunk[KLI_FILE_IO_CHUNK];
    size_t arena_offset;
    size_t expected_size;
    long fd;
    long result;

    if (out == 0) {
        return KLI_INVAL;
    }
    file_string_clear(out);
    if (path == 0 || !file_arena_is_valid(arena)) {
        return KLI_INVAL;
    }

    arena_offset = arena->offset;
    result = file_regular_size(path, &expected_size);
    if (result < 0) {
        return result;
    }

    result = kli_string_init_capacity(&temporary, arena, expected_size);
    if (result < 0) {
        arena->offset = arena_offset;
        return result;
    }

    fd = kli_open(path, ARM_O_RDONLY);
    if (fd < 0) {
        arena->offset = arena_offset;
        return fd;
    }

    for (;;) {
        long count = kli_read((int)fd, chunk, sizeof(chunk));

        if (count < 0) {
            result = count;
            break;
        }
        if (count == 0) {
            result = 0;
            break;
        }
        if ((uint64_t)count > (uint64_t)sizeof(chunk)) {
            result = KLI_INVAL;
            break;
        }

        result = kli_string_append_n(&temporary, chunk, (size_t)count);
        if (result < 0) {
            break;
        }
    }

    {
        long close_status = kli_close((int)fd);
        if (result == 0 && close_status < 0) {
            result = close_status;
        }
    }

    if (result < 0) {
        arena->offset = arena_offset;
        return result;
    }

    *out = temporary;
    return 0;
}
