#ifndef ARMONIOS_PROGRAMS_LIBKARM_FILE_H
#define ARMONIOS_PROGRAMS_LIBKARM_FILE_H

#include <stddef.h>

#include "libkarm/arena.h"
#include "libkarm/buffer.h"
#include "libkarm/dynamic_string.h"

/* Return 0 on success or a negative ArmoniOS status. */
long kli_fd_write_all(int fd, const void *data, size_t size);

/*
 * Read one regular file into a new arena-backed destination.
 *
 * On success `out` owns allocations from `arena`. On failure `out` is cleared
 * and the arena offset is restored to its value at function entry.
 */
long kli_file_read_all(const char *path, kli_arena_t *arena,
                       kli_buffer_t *out);
long kli_file_read_text(const char *path, kli_arena_t *arena,
                        kli_string_t *out);

#endif
