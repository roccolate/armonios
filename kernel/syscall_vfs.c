#include "kernel/syscall_internal.h"

#include <stdint.h>

#include "input/input.h"
#include "kernel/syscall_helpers.h"
#include "kernel/vfs.h"
#include "uart/pl011.h"

#define SYSCALL_VFS_IO_CHUNK 256U

static uint64_t syscall_vfs_chunk_size(uint64_t remaining) {
    return remaining > SYSCALL_VFS_IO_CHUNK ? SYSCALL_VFS_IO_CHUNK : remaining;
}

int64_t sys_write(process_t *process, uint64_t fd, uint64_t buf,
                  uint64_t len) {
    uint8_t kernel_buffer[SYSCALL_VFS_IO_CHUNK];
    uint64_t total = 0;
    int64_t status = sys_user_buf_in(process, buf, len);

    if (status != 0) {
        return status;
    }

    if (fd >= (uint64_t)FD_FILE_BASE) {
        int vfs_fd = (int64_t)fd - FD_FILE_BASE;

        /* Preserve the existing NULL/zero-length file-write rejection. */
        if (len == 0) {
            uint64_t bytes_written = 0;

            if (buf == 0 ||
                vfs_write_fd(vfs_fd, kernel_buffer, 0, &bytes_written) != 0) {
                return ERR_BADF;
            }
            return (int64_t)bytes_written;
        }

        while (total < len) {
            uint64_t requested = syscall_vfs_chunk_size(len - total);
            uint64_t bytes_written = 0;

            status = sys_copy_from_user(process, kernel_buffer, buf + total,
                                        requested);
            if (status != 0) {
                return total != 0 ? (int64_t)total : status;
            }
            if (vfs_write_fd(vfs_fd, kernel_buffer, requested,
                             &bytes_written) != 0) {
                return total != 0 ? (int64_t)total : ERR_BADF;
            }

            total += bytes_written;
            if (bytes_written < requested) {
                break;
            }
        }
        return (int64_t)total;
    }

    if (fd != FD_STDOUT && fd != FD_STDERR) {
        return ERR_BADF;
    }

    while (total < len) {
        uint64_t requested = syscall_vfs_chunk_size(len - total);

        status = sys_copy_from_user(process, kernel_buffer, buf + total,
                                    requested);
        if (status != 0) {
            return total != 0 ? (int64_t)total : status;
        }
        for (uint64_t i = 0; i < requested; i++) {
            uart_putc((char)kernel_buffer[i]);
        }
        total += requested;
    }

    return (int64_t)total;
}

int64_t sys_open(process_t *process, uint64_t path_ptr, uint64_t flags) {
    char path[VFS_MAX_PATH];
    int fd;

    if ((flags & ~VFS_O_ALLOWED) != 0 ||
        (flags & VFS_O_ACCMODE) == VFS_O_ACCMODE ||
        sys_user_copy_cstr(process, path_ptr, path, sizeof(path)) != 0) {
        return ERR_INVAL;
    }

    fd = vfs_open_flags(path, (uint32_t)flags);
    if (fd < 0) {
        return ERR_NOENT;
    }

    return (int64_t)fd + FD_FILE_BASE;
}

int64_t sys_close(uint64_t fd) {
    if (fd < (uint64_t)FD_FILE_BASE) {
        return ERR_BADF;
    }

    if (vfs_close((int64_t)fd - FD_FILE_BASE) != 0) {
        return ERR_BADF;
    }

    return 0;
}

int64_t sys_seek(uint64_t fd, uint64_t offset, uint64_t whence) {
    if (fd < (uint64_t)FD_FILE_BASE || whence != 0) {
        return ERR_INVAL;
    }

    if (vfs_seek((int64_t)fd - FD_FILE_BASE, offset) != 0) {
        return ERR_BADF;
    }

    return (int64_t)offset;
}

int64_t sys_read(process_t *process, uint64_t fd, uint64_t buf,
                 uint64_t len) {
    uint8_t kernel_buffer[SYSCALL_VFS_IO_CHUNK];
    uint64_t total = 0;
    int64_t status;

    if (fd == FD_STDIN) {
        int c;

        status = sys_user_buf_out(process, buf, len);
        if (status != 0) {
            return status;
        }

        if (len == 0) {
            return 0;
        }

        c = input_queue_poll_char();
        if (c < 0) {
            return ERR_AGAIN;
        }

        kernel_buffer[0] = (uint8_t)c;
        status = sys_copy_to_user(process, buf, kernel_buffer, 1);
        return status == 0 ? 1 : status;
    }

    if (fd < (uint64_t)FD_FILE_BASE) {
        return ERR_BADF;
    }

    status = sys_user_buf_out(process, buf, len);
    if (status != 0) {
        return status;
    }

    /* Preserve the existing NULL/zero-length file-read rejection. */
    if (len == 0) {
        uint64_t bytes_read = 0;

        if (buf == 0 ||
            vfs_read_fd((int64_t)fd - FD_FILE_BASE, kernel_buffer, 0,
                        &bytes_read) != 0) {
            return ERR_BADF;
        }
        return (int64_t)bytes_read;
    }

    while (total < len) {
        uint64_t requested = syscall_vfs_chunk_size(len - total);
        uint64_t bytes_read = 0;

        if (vfs_read_fd((int64_t)fd - FD_FILE_BASE, kernel_buffer, requested,
                        &bytes_read) != 0) {
            return total != 0 ? (int64_t)total : ERR_BADF;
        }
        if (bytes_read == 0) {
            break;
        }

        status = sys_copy_to_user(process, buf + total, kernel_buffer,
                                  bytes_read);
        if (status != 0) {
            return total != 0 ? (int64_t)total : status;
        }

        total += bytes_read;
        if (bytes_read < requested) {
            break;
        }
    }

    return (int64_t)total;
}

int64_t sys_stat(process_t *process, uint64_t path_ptr, uint64_t stat_ptr) {
    char path[VFS_MAX_PATH];
    vfs_stat_t stat;

    if (sys_user_copy_cstr(process, path_ptr, path, sizeof(path)) != 0 ||
        sys_user_buf_out(process, stat_ptr, sizeof(stat)) != 0) {
        return ERR_INVAL;
    }

    if (vfs_stat(path, &stat) != 0) {
        return ERR_NOENT;
    }

    return sys_copy_to_user(process, stat_ptr, &stat, sizeof(stat)) == 0
               ? 0
               : ERR_INVAL;
}

int64_t sys_readdir(process_t *process, uint64_t path_ptr, uint64_t buf,
                    uint64_t len) {
    char path[VFS_MAX_PATH];
    uint64_t bytes_written = 0;

    if (sys_user_copy_cstr(process, path_ptr, path, sizeof(path)) != 0 ||
        sys_user_buf_out(process, buf, len) != 0) {
        return ERR_INVAL;
    }

    /*
     * vfs_list currently has no offset/iterator contract, so readdir cannot be
     * safely chunked without defining new VFS semantics. Keep this final direct
     * handoff isolated until the directory-list API is converted separately.
     */
    if (vfs_list(path, (uint8_t *)(uintptr_t)buf, len, &bytes_written) != 0) {
        return ERR_NOENT;
    }

    return (int64_t)bytes_written;
}

int64_t sys_unlink(process_t *process, uint64_t path_ptr) {
    char path[VFS_MAX_PATH];

    if (sys_user_copy_cstr(process, path_ptr, path, sizeof(path)) != 0) {
        return ERR_INVAL;
    }
    if (vfs_unlink(path) != 0) {
        return ERR_NOENT;
    }
    return 0;
}

int64_t sys_rename(process_t *process, uint64_t old_ptr, uint64_t new_ptr) {
    char old_path[VFS_MAX_PATH];
    char new_path[VFS_MAX_PATH];

    if (sys_user_copy_cstr(process, old_ptr, old_path, sizeof(old_path)) != 0 ||
        sys_user_copy_cstr(process, new_ptr, new_path, sizeof(new_path)) != 0) {
        return ERR_INVAL;
    }
    if (vfs_rename(old_path, new_path) != 0) {
        return ERR_NOENT;
    }
    return 0;
}
