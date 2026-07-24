# Filesystem errors and information ABI

ArmoniOS keeps the global public ABI identifier at 1.0 during pre-release
work. This cut adds filesystem-specific error values and `SYS_FSINFO = 51`
without changing existing syscall numbers or layouts.

## Error values

The new public errors are ArmoniOS-native status values, not a POSIX errno
compatibility promise:

| Value | Name | Meaning |
|---:|---|---|
| -14 | `ARMONIOS_ERR_EXIST` | Destination or resource already exists. |
| -15 | `ARMONIOS_ERR_NOTDIR` | A required path component is not a directory. |
| -16 | `ARMONIOS_ERR_ISDIR` | An operation requiring a regular file received a directory. |
| -17 | `ARMONIOS_ERR_NOTEMPTY` | A directory cannot be removed because it contains visible entries. |
| -18 | `ARMONIOS_ERR_NOSPC` | The filesystem or fixed-capacity object has no remaining space. |
| -19 | `ARMONIOS_ERR_ROFS` | The selected filesystem is read-only for the requested mutation. |
| -20 | `ARMONIOS_ERR_NOTSUP` | The selected mount does not implement the requested operation. |
| -21 | `ARMONIOS_ERR_RANGE` | A value or filesystem report cannot be represented safely. |

Existing errors and their numeric meanings remain frozen. Later filesystem
operations should return the most specific value they can prove instead of
collapsing every failure into `NOENT` or `BADF`.

## `SYS_FSINFO`

```text
x8 = 51
x0 = absolute path inside the target mount
x1 = pointer to arm_fsinfo_t
```

The caller initializes `version` and `struct_size`. The kernel validates the
complete destination, resolves the canonical path to the owning mount, builds
one kernel-owned result, and copies it to EL0 only after successful validation.

Possible failures include:

- `INVAL` for an invalid path, version, size, or user buffer;
- `NOENT` when no mount owns the canonical path;
- `NOTSUP` when the owning mount has no filesystem-information callback;
- `RANGE` when a filesystem returns inconsistent or unrepresentable values.

## Layout

```c
typedef struct {
    uint32_t version;
    uint32_t struct_size;
    uint64_t total_bytes;
    uint64_t free_bytes;
    uint32_t block_size;
    uint32_t max_name_length;
    uint32_t max_path_length;
    uint32_t flags;
    char filesystem[16];
    uint64_t reserved;
} arm_fsinfo_t;
```

The structure is fixed at 64 bytes. `filesystem` is a lower-case,
NUL-terminated implementation name. `reserved` is zero.

## Flags

- `ARM_FS_FLAG_READ_ONLY`
- `ARM_FS_FLAG_DIRECTORIES`
- `ARM_FS_FLAG_LONG_NAMES`
- `ARM_FS_FLAG_FLUSH`
- `ARM_FS_FLAG_TRUNCATE`
- `ARM_FS_FLAG_FREE_BYTES_VALID`

`free_bytes` must be ignored unless its validity flag is set. A filesystem must
not advertise long names, truncate, durable flush, or write support merely
because the generic VFS has a placeholder for that operation.

## Initial FAT32 report

The first native provider is the `/fat` mount:

- filesystem name: `fat32`;
- total bytes: mounted FAT32 volume sectors multiplied by 512;
- block size: 512;
- maximum visible 8.3 component: 12 characters;
- maximum canonical path: 63 characters;
- directory traversal advertised;
- read-only reported from the mounted transport;
- flush advertised only when the block device has a real flush callback;
- long names, truncate, and exact free-byte accounting remain unadvertised.

Files is the first EL0 consumer. It displays the filesystem identity and
read-only/read-write state while retaining its existing directory and mutation
boundaries.
