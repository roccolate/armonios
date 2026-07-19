#include "kernel/syscall_internal.h"

#include <stdint.h>

#include "input/input.h"
#include "kernel/syscall_helpers.h"
#include "kernel/vfs.h"
#include "uart/pl011.h"

int64_t sys_write(process_t *process, uint64_t fd, uint64_t buf,
                  uint64_t len) {
    const char *text = (const char *)(uintptr_t)buf;
    uint64_t bytes_written = 0;

    int64_t status = sys_user_buf_in(process, buf, len);
    if (status != 0) {
        return status;
    }

    if (fd >= (uint64_t)FD_FILE_BASE) {
        if (vfs_write_fd((int64_t)fd - FD_FILE_BASE,
                         (const uint8_t *)(uintptr_t)buf, len,
                         &bytes_written) != 0) {
            return ERR_BADF;
        }
        return (int64_t)bytes_written;
    }

    if (fd != FD_STDOUT && fd != FD_STDERR) {
        return ERR_BADF;
    }

    for (uint64_t i = 0; i < len; i++) {
        uart_putc(text[i]);
    }

    return (int64_t)len;
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
    uint64_t bytes_read = 0;
    int64_t status;

    if (fd == FD_STDIN) {
        uint8_t *out = (uint8_t *)(uintptr_t)buf;
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

        out[0] = (uint8_t)c;
        return 1;
    }

    if (fd < (uint64_t)FD_FILE_BASE) {
        return ERR_BADF;
    }

    status = sys_user_buf_out(process, buf, len);
    if (status != 0) {
        return status;
    }

    if (vfs_read_fd((int64_t)fd - FD_FILE_BASE, (uint8_t *)(uintptr_t)buf,
                    len, &bytes_read) != 0) {
        return ERR_BADF;
    }

    return (int64_t)bytes_read;
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

    *(vfs_stat_t *)(uintptr_t)stat_ptr = stat;
    return 0;
}

int64_t sys_readdir(process_t *process, uint64_t path_ptr, uint64_t buf,
                    uint64_t len) {
    char path[VFS_MAX_PATH];
    uint64_t bytes_written = 0;

    if (sys_user_copy_cstr(process, path_ptr, path, sizeof(path)) != 0 ||
        sys_user_buf_out(process, buf, len) != 0) {
        return ERR_INVAL;
    }

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
