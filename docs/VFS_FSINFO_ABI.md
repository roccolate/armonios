# Filesystem errors and information ABI

ArmoniOS exposes filesystem-specific status values and a versioned filesystem
information call without changing older syscall numbers or layouts.

The global public ABI remains `1.0` during pre-release development. Optional
filesystem capability must be queried; it must not be inferred from the ABI
revision.

See also `PUBLIC_ABI.md`, `SYSCALLS.md`, and `VFS_METADATA_ABI.md`.

## Filesystem statuses

These are ArmoniOS-native statuses, not POSIX errno compatibility:

| Value | Public name | Meaning |
|---:|---|---|
| -14 | `ARMONIOS_ERR_EXIST` | Destination or resource already exists. |
| -15 | `ARMONIOS_ERR_NOTDIR` | A required path component is not a directory. |
| -16 | `ARMONIOS_ERR_ISDIR` | A regular-file operation received a directory. |
| -17 | `ARMONIOS_ERR_NOTEMPTY` | A directory is not empty. |
| -18 | `ARMONIOS_ERR_NOSPC` | Filesystem or fixed-capacity storage is exhausted. |
| -19 | `ARMONIOS_ERR_ROFS` | The selected filesystem is read-only. |
| -20 | `ARMONIOS_ERR_NOTSUP` | The provider does not implement the operation. |
| -21 | `ARMONIOS_ERR_RANGE` | A value cannot be represented safely. |

Existing status values retain their numeric meanings. New operations should
return the most specific result the provider can prove. Frozen legacy operations
may preserve broader historical failure behavior.

## `SYS_FSINFO`

```text
x8 = 51
x0 = absolute path inside the target mount
x1 = pointer to arm_fsinfo_t
```

The path is canonicalized and resolved to the owning mount. The caller initializes
`version` and `struct_size`. The kernel validates the complete writable
destination, builds one kernel-owned result, validates the provider report, and
copies the record only on success.

Typical failures:

- `INVAL` — invalid path, version, structure size, or user buffer;
- `NOENT` — no mount owns the canonical path;
- `NOTSUP` — the owning mount has no filesystem-information provider;
- `RANGE` — provider values are inconsistent or not representable.

## Record layout

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

The record is fixed at 64 bytes. `filesystem` is a lower-case NUL-terminated
implementation name. `reserved` is zero.

## Capability flags

```text
ARM_FS_FLAG_READ_ONLY
ARM_FS_FLAG_DIRECTORIES
ARM_FS_FLAG_LONG_NAMES
ARM_FS_FLAG_FLUSH
ARM_FS_FLAG_TRUNCATE
ARM_FS_FLAG_FREE_BYTES_VALID
```

Rules:

- ignore `free_bytes` unless `FREE_BYTES_VALID` is present;
- do not advertise long names until the visible naming contract exists;
- do not advertise truncate until resize and rollback behavior exist;
- advertise flush only when the selected transport/provider has a real callback;
- a flush callback does not by itself prove reboot durability;
- read-only state describes the owning filesystem/device view selected for the
  path.

## FAT32 provider

The `/fat` mount currently reports:

- filesystem name `fat32`;
- total mounted-volume capacity;
- 512-byte blocks;
- maximum visible 8.3 component length of 12 characters;
- maximum canonical path length of 63 characters;
- directory traversal support;
- read-only state inherited from the mounted transport;
- flush support only when the block-device path provides it.

It does not advertise:

- valid exact free-byte accounting;
- long names;
- truncate;
- nested mutation;
- reboot-verified durability.

## Consumer behavior

Files is the first graphical consumer. It displays filesystem identity, capacity,
and read-only/read-write state without widening the current directory-mutation
contract.

A consumer should:

- initialize version and size;
- check flags before using optional fields;
- treat unknown providers and unsupported reporting distinctly;
- avoid interpreting `total_bytes` as writable free capacity;
- avoid translating ArmoniOS-native statuses into guessed POSIX meanings.

## Change discipline

A filesystem-information change must update together:

- public record or flags;
- VFS provider contract;
- kernel validation and copy boundary;
- `libkarm` wrapper;
- provider tests and ABI assertions;
- a real consumer;
- this document, `SYSCALLS.md`, and affected current-state/roadmap text.
