#ifndef ARMONIOS_INCLUDE_ARMONIOS_ABI_VFS_H
#define ARMONIOS_INCLUDE_ARMONIOS_ABI_VFS_H

#include <stdint.h>

#include "include/armonios/abi/base.h"

/* Standard descriptors visible to EL0. */
#define ARM_FD_STDIN     0
#define ARM_FD_STDOUT    1
#define ARM_FD_STDERR    2
#define ARM_FD_FILE_BASE 3

/* Public SYS_OPEN flags. */
#define ARM_O_RDONLY  0U
#define ARM_O_WRONLY  1U
#define ARM_O_RDWR    2U
#define ARM_O_ACCMODE 3U
#define ARM_O_CREAT   0x40U
#define ARM_O_ALLOWED (ARM_O_ACCMODE | ARM_O_CREAT)

/* Only absolute seek is currently implemented. */
#define ARM_SEEK_SET 0U

/* Current SYS_STAT payload. Future richer metadata uses a new syscall/layout. */
typedef struct {
    uint64_t size;
} arm_stat_t;

_Static_assert(sizeof(arm_stat_t) == 8, "ABI drift: arm_stat_t");
_Static_assert(ARM_FD_STDIN == 0, "ABI drift: stdin descriptor");
_Static_assert(ARM_FD_STDOUT == 1, "ABI drift: stdout descriptor");
_Static_assert(ARM_FD_STDERR == 2, "ABI drift: stderr descriptor");
_Static_assert(ARM_FD_FILE_BASE == 3, "ABI drift: file descriptor base");
_Static_assert(ARM_O_RDONLY == 0U, "ABI drift: O_RDONLY");
_Static_assert(ARM_O_WRONLY == 1U, "ABI drift: O_WRONLY");
_Static_assert(ARM_O_RDWR == 2U, "ABI drift: O_RDWR");
_Static_assert(ARM_O_CREAT == 0x40U, "ABI drift: O_CREAT");

#endif
