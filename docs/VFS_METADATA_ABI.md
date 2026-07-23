# Structured VFS metadata ABI

ArmoniOS remains on public ABI 1.0 until the first official release. This
pre-release addition introduces structured file metadata without changing the
frozen `SYS_STAT` and `SYS_READDIR` contracts.

## Version policy

The global ABI identifier stays at 1.0 during pre-release development. The
first official release will establish the compatibility baseline; later
additive public changes may advance the minor version intentionally.

The `v2` suffix below identifies the successor metadata interfaces and payload
layouts. It does not imply that the global ArmoniOS ABI has advanced to 1.1 or
2.0.

## Compatibility

The original interfaces remain unchanged:

- `SYS_STAT` (`45`) writes the 8-byte `arm_stat_t` size payload.
- `SYS_READDIR` (`46`) writes the historical newline-delimited byte stream.

New applications may use:

- `SYS_STAT_V2` (`49`)
- `SYS_READDIR_V2` (`50`)

Existing user images continue to use the old numbers and layouts unchanged.

## `arm_stat_v2_t`

```c
typedef struct {
    uint32_t version;      /* ARM_VFS_METADATA_VERSION */
    uint32_t struct_size;  /* sizeof(arm_stat_v2_t) */
    uint64_t size;
    uint32_t type;
    uint32_t attributes;
    uint64_t reserved;
} arm_stat_v2_t;
```

Before calling `kli_stat_v2`, the caller sets `version` and `struct_size`. The
kernel rejects unknown versions or sizes instead of guessing the caller's
layout. On success every field is initialized and `reserved` is zero.

## `arm_dirent_v2_t`

```c
typedef struct {
    uint32_t version;
    uint32_t struct_size;
    uint64_t size;
    uint32_t type;
    uint32_t attributes;
    uint64_t reserved;
    char name[64];
} arm_dirent_v2_t;
```

`SYS_READDIR_V2(path, start_index, entries, max_entries)` returns the number of
complete entries copied. Pagination uses a logical entry index, not a byte
offset. Names are NUL-terminated, directory names do not include the legacy
trailing slash, and entries are never partially returned.

The first implementation accepts at most eight entries per syscall so the
kernel can use bounded stack storage. Callers continue with
`start_index += returned_count`.

## Name and path limits

`ARM_DIRENT_NAME_MAX` and the internal component-name buffer are 64 bytes,
including the terminating NUL. The complete canonical VFS path is also currently
limited to 64 bytes. This cut freezes neither long-name encoding nor a larger
path budget: both must be reviewed deliberately before VFAT long names become a
release claim. A filesystem entry whose name cannot be represented is rejected
rather than silently truncated.

## Types

- `ARM_FILE_TYPE_UNKNOWN = 0`
- `ARM_FILE_TYPE_REGULAR = 1`
- `ARM_FILE_TYPE_DIRECTORY = 2`

## Attributes

The ABI reserves generic bits for read-only, hidden, system, and archive. The
initial VFS adapter returns zero when the underlying filesystem does not expose
an attribute. This is deliberate: absence of metadata is not fabricated as a
filesystem-specific value.

## Current adapter

The VFS now owns filesystem-neutral `vfs_metadata_t` and `vfs_dirent_t` records.
Mounts may provide native path-aware metadata and bounded readdir callbacks. FAT32
maps short-name entries, file size, directory type, and FAT attributes directly
into those records.

Bootfs, tmpfs, and older list-only mounts retain a compatibility fallback that
converts their legacy stat/list operations into the same internal records. The
public syscall layer only versions and copies the internal result; it does not
parse FAT-specific directory text. Files is the first EL0 consumer of
`SYS_READDIR_V2`, with a legacy fallback while the old call remains supported.
