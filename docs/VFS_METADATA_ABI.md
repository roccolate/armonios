# Structured VFS metadata ABI

ArmoniOS provides versioned file metadata without changing the frozen legacy
`SYS_STAT` and `SYS_READDIR` contracts.

The global public ABI remains `1.0` during pre-release development. The `v2`
suffix names the successor calls and layouts; it does not imply ABI major version
2.

See also `PUBLIC_ABI.md`, `SYSCALLS.md`, and `VFS_FSINFO_ABI.md`.

## Compatibility

Legacy interfaces remain unchanged:

- `SYS_STAT = 45` writes the eight-byte `arm_stat_t` size payload;
- `SYS_READDIR = 46` writes the historical newline-delimited byte stream.

Structured interfaces are:

- `SYS_STAT_V2 = 49`;
- `SYS_READDIR_V2 = 50`.

Existing KLI1 images may continue to use legacy numbers and layouts.

## File metadata

```c
typedef struct {
    uint32_t version;
    uint32_t struct_size;
    uint64_t size;
    uint32_t type;
    uint32_t attributes;
    uint64_t reserved;
} arm_stat_v2_t;
```

The record is 32 bytes. Before calling the typed wrapper, the caller initializes:

```text
version     = ARM_VFS_METADATA_VERSION
struct_size = sizeof(arm_stat_v2_t)
```

The kernel rejects unknown versions or sizes. On success every field is
initialized and `reserved` is zero.

## Directory entries

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

The record is 96 bytes.

`SYS_READDIR_V2(path, start_index, entries, max_entries)` returns the number of
complete records copied. Pagination uses a logical entry index, not a byte offset.
Names are NUL-terminated and directory names do not carry the legacy trailing
slash.

The current syscall accepts at most eight entries per call so the kernel can use
bounded storage. Callers continue with:

```text
start_index += returned_count
```

No entry is partially returned.

## Types

```text
ARM_FILE_TYPE_UNKNOWN   = 0
ARM_FILE_TYPE_REGULAR   = 1
ARM_FILE_TYPE_DIRECTORY = 2
```

## Attributes

The generic attribute bits cover read-only, hidden, system, and archive state.
A provider returns zero for a bit it cannot establish. Missing metadata is not
fabricated as a filesystem-specific value.

## Name and path bounds

`ARM_DIRENT_NAME_MAX` is 64 bytes including the terminator. The current canonical
VFS path budget is also 64 bytes including the terminator.

An entry that cannot be represented is rejected instead of silently truncated.
These current limits do not define a future VFAT long-name encoding or promise
that the path budget will remain fixed forever; any public change requires an
explicit compatible contract.

## Kernel adapter

The VFS owns filesystem-neutral internal metadata and directory-entry records.
Mounts may provide native path-aware stat and bounded readdir callbacks.

FAT32 maps short-name entries, size, file/directory type, and FAT attributes
directly into the internal records. Bootfs, tmpfs, and older list-only mounts may
use compatibility adapters that convert their legacy operations into the same
internal representation.

The public syscall layer validates, versions, and copies the internal result. It
does not parse FAT-specific directory text.

## Userland consumer

Files is the first graphical consumer of the structured metadata calls. Legacy
fallback remains available while the old interfaces are supported.

A new consumer should:

- initialize version and structure-size fields;
- handle bounded pagination;
- inspect the type instead of inferring directories from punctuation;
- treat names as bounded NUL-terminated bytes;
- preserve compatibility behavior explicitly when supporting old kernels.
