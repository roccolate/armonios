# FAT32 directory traversal

ArmoniOS can traverse existing FAT32 directory trees below `/fat` using bounded
8.3 path components.

## Supported operations

The VFS canonicalizes an absolute path first. The FAT32 adapter then strips the
`/fat` mount prefix and resolves each remaining component through its parent
directory cluster.

```text
/fat/docs/sub/../sub/note.txt
        |
        v
/fat/docs/sub/note.txt
        |
        v
root cluster -> DOCS -> SUB -> NOTE.TXT
```

Supported behavior:

- look up an existing file or directory by 8.3 components;
- open and read regular files in nested directories;
- list the root or any existing nested directory;
- omit FAT `.` and `..` entries from listings;
- append `/` to directory names in the textual `readdir` stream;
- stat files and directories through path-aware mount dispatch;
- reject a directory passed to the regular-file open path;
- validate every directory cluster and FAT chain against mounted geometry.

The public `arm_stat_t` payload still contains only a size. A directory therefore
reports size zero; richer type metadata requires a future ABI payload rather than
silently changing the existing structure.

## VFS mount callbacks

Mounts may now provide path-aware callbacks:

```c
stat_path(context, canonical_path, stat)
list_path(context, canonical_path, offset, buffer, capacity, written)
```

The original root-only `list` callback remains supported. The VFS selects the
exact or longest-prefix mount after path canonicalization, so filesystem
adapters never receive `//`, `.` or unresolved `..` components.

## Mutation boundary

This cut intentionally keeps FAT32 mutation at the volume root:

- `O_CREAT` for a nested missing file is rejected;
- nested unlink is rejected;
- nested rename is rejected;
- `mkdir` and `rmdir` do not exist yet.

Directory mutation needs a separate transaction policy for allocating a
directory cluster, writing `.` and `..`, extending directory chains, updating
multiple parent entries, handling partial write failure, and proving recovery.
Read-only traversal does not pretend those guarantees already exist.

## Name and filesystem limits

- components use FAT short-name (8.3) rules;
- long filename entries are skipped;
- symbolic links are not a FAT32 concept here;
- directory paths remain bounded by `VFS_MAX_PATH`;
- FAT chain loops, bad clusters, and out-of-range clusters are rejected by the
  bounded traversal.

## Test fixture

The focused host fixture creates this real cluster layout:

```text
/fat/
└── DOCS/
    ├── README.TXT
    └── SUB/
        └── NOTE.TXT
```

It includes FAT directory attributes, separate clusters, `.` and `..` entries,
file data, VFS canonical traversal, nested list/stat/open, and explicit rejection
of nested mutation.
