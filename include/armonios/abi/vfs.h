#ifndef ARMONIOS_INCLUDE_ARMONIOS_ABI_VFS_H
#define ARMONIOS_INCLUDE_ARMONIOS_ABI_VFS_H

#include <stdint.h>

#include "base.h"

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

/* Frozen SYS_STAT v1 payload. */
typedef struct {
    uint64_t size;
} arm_stat_t;

/* Structured metadata ABI used by SYS_STAT_V2 and SYS_READDIR_V2. */
#define ARM_VFS_METADATA_VERSION 2U
#define ARM_DIRENT_NAME_MAX      64U

#define ARM_FILE_TYPE_UNKNOWN   0U
#define ARM_FILE_TYPE_REGULAR   1U
#define ARM_FILE_TYPE_DIRECTORY 2U

#define ARM_FILE_ATTR_READ_ONLY 0x00000001U
#define ARM_FILE_ATTR_HIDDEN    0x00000002U
#define ARM_FILE_ATTR_SYSTEM    0x00000004U
#define ARM_FILE_ATTR_ARCHIVE   0x00000008U

/*
 * Callers initialize version and struct_size before SYS_STAT_V2. The kernel
 * validates both fields, fills the remaining fields, and zeroes reserved data.
 */
typedef struct {
    uint32_t version;
    uint32_t struct_size;
    uint64_t size;
    uint32_t type;
    uint32_t attributes;
    uint64_t reserved;
} arm_stat_v2_t;

/*
 * SYS_READDIR_V2 returns an array of fixed-size entries beginning at a caller
 * supplied logical index. Names are NUL-terminated and never truncated: an
 * entry whose name does not fit ARM_DIRENT_NAME_MAX is not representable.
 */
typedef struct {
    uint32_t version;
    uint32_t struct_size;
    uint64_t size;
    uint32_t type;
    uint32_t attributes;
    uint64_t reserved;
    char name[ARM_DIRENT_NAME_MAX];
} arm_dirent_v2_t;

_Static_assert(sizeof(arm_stat_t) == 8, "ABI drift: arm_stat_t");
_Static_assert(sizeof(arm_stat_v2_t) == 32, "ABI drift: arm_stat_v2_t");
_Static_assert(sizeof(arm_dirent_v2_t) == 96, "ABI drift: arm_dirent_v2_t");
_Static_assert(ARM_VFS_METADATA_VERSION == 2U,
               "ABI drift: VFS metadata version");
_Static_assert(ARM_FILE_TYPE_REGULAR == 1U,
               "ABI drift: regular file type");
_Static_assert(ARM_FILE_TYPE_DIRECTORY == 2U,
               "ABI drift: directory file type");
_Static_assert(ARM_FD_STDIN == 0, "ABI drift: stdin descriptor");
_Static_assert(ARM_FD_STDOUT == 1, "ABI drift: stdout descriptor");
_Static_assert(ARM_FD_STDERR == 2, "ABI drift: stderr descriptor");
_Static_assert(ARM_FD_FILE_BASE == 3, "ABI drift: file descriptor base");
_Static_assert(ARM_O_RDONLY == 0U, "ABI drift: O_RDONLY");
_Static_assert(ARM_O_WRONLY == 1U, "ABI drift: O_WRONLY");
_Static_assert(ARM_O_RDWR == 2U, "ABI drift: O_RDWR");
_Static_assert(ARM_O_CREAT == 0x40U, "ABI drift: O_CREAT");

#endif
