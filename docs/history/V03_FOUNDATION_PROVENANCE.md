# v0.3 storage/VFS foundation provenance

This historical record preserves the implementation sequence behind the live
`../V03_IMPLEMENTATION_STATUS.md` summary.

## Landed cuts

| PR | Foundation |
|---:|---|
| #80 | generic block-device descriptor, range checks, read-only state, flush contract, and bounded views |
| #81 | QEMU virtio-blk and Raspberry Pi diagnostic EMMC storage adapters |
| #82 | board storage to bounded MBR view to FAT32 mount path |
| #90 | canonical absolute paths and longest component-prefix mount selection |
| #93 | traversal of existing nested FAT32 8.3 directory trees |
| #95 | native structured VFS metadata, `SYS_STAT_V2`, `SYS_READDIR_V2`, and the first Files consumer |
| #97 | filesystem-specific statuses, `SYS_FSINFO`, FAT32 capability reporting, and Files integration |

## Structured metadata cut

PR #95 established:

- filesystem-neutral internal metadata and directory-entry records;
- native FAT32 type, size, and attribute mapping;
- `SYS_STAT_V2 = 49`;
- `SYS_READDIR_V2 = 50`;
- fixed, versioned public records;
- typed `libkarm` wrappers;
- Files as the first EL0 consumer;
- unchanged legacy calls 45 and 46.

Recorded squash merge:

```text
a078c995f485bab84135233c149e28ba081b11b0
```

## Filesystem-information cut

PR #97 established:

- public `EXIST`, `NOTDIR`, `ISDIR`, `NOTEMPTY`, `NOSPC`, `ROFS`, `NOTSUP`, and
  `RANGE` statuses;
- a shared public source for kernel and `libkarm` aliases;
- `SYS_FSINFO = 51`;
- fixed 64-byte `arm_fsinfo_t`;
- canonical owning-mount resolution;
- distinct `NOENT`, `NOTSUP`, and `RANGE` results for the new interface;
- FAT32 identity, capacity, block-size, path/name-limit, directory, read-only, and
  flush reporting;
- Files as a graphical consumer.

Recorded squash merge:

```text
9a697eb1bae909eed3f17a3da6928bc66192dac3
```

Recorded final validation:

- Export FAT32 run #188: success;
- Verify ArmoniOS run #530: success;
- CI - Tests run #664: success.

## Scope at this checkpoint

The sequence intentionally stopped before:

- complete relative/end seek behavior;
- truncate and cluster shrink/grow;
- mkdir/rmdir;
- nested mutation;
- application-visible durability;
- VFAT long names;
- ext2.

Those items remain live roadmap work. This file preserves provenance only and
must not be used as the current capability source after later cuts land.
