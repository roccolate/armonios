# v0.3 storage and VFS status

This document is the live implementation checkpoint for the v0.3 storage/VFS
platform. It describes merged capability, not release evidence or PR history.

For broader context:

- current system status: `CURRENT_STATE.md`;
- implemented boundaries: `ARCHITECTURE.md`;
- remaining milestone order: `ROADMAP.md`;
- syscall layouts: `SYSCALLS.md`;
- historical provenance: `history/V03_FOUNDATION_PROVENANCE.md`.

## Goal

v0.3 replaces demo-specific storage plumbing with reusable contracts for block
devices, paths, mounts, metadata, errors, filesystem information, mutation, and
durability.

The phase is **in progress**.

## Implemented foundations

### Block-device contract

The generic storage descriptor provides:

- logical block size and block count;
- overflow-safe range validation;
- bounded reads and writes;
- explicit read-only state;
- explicit flush support or `NOTSUP`;
- bounded child views over a parent device.

QEMU virtio-blk exposes a writable production descriptor. The Raspberry Pi EMMC2
path exposes a separate read-only diagnostic descriptor and does not imply normal
hardware storage support.

### Device, MBR, and FAT32 mount path

The production storage path is:

```text
board storage device
  -> optional bounded MBR partition view
  -> FAT32 mount
  -> VFS mount
```

Whole-device and primary-MBR FAT32 discovery are implemented. Partition views
preserve parent range, read-only, and flush contracts.

### Canonical paths and mount resolution

The VFS uses one bounded absolute-path policy:

- a leading slash is required;
- repeated separators collapse;
- `.` components disappear;
- `..` is resolved without permitting root escape;
- root remains `/`;
- mount selection uses the longest component-aligned prefix;
- canonical path identity is shared by nodes and mounts.

A mount at `/fat` does not match `/fatx`.

### Structured metadata

Filesystem-neutral internal records describe:

- regular files and directories;
- byte size;
- generic attributes;
- bounded directory entries.

The public append-only interfaces are:

- `SYS_STAT_V2 = 49` with `arm_stat_v2_t`;
- `SYS_READDIR_V2 = 50` with `arm_dirent_v2_t`.

Legacy calls 45 and 46 remain unchanged. Files is the first graphical consumer of
the structured interfaces and can retain a compatibility fallback where needed.

### Filesystem errors and information

The public status set includes filesystem-specific values for existing,
not-a-directory, is-a-directory, not-empty, no-space, read-only, unsupported, and
range failures.

`SYS_FSINFO = 51` returns `arm_fsinfo_t` for the mount owning a canonical path.
The FAT32 provider reports:

- filesystem identity;
- volume capacity;
- 512-byte block size;
- current name and path limits;
- directory traversal support;
- transport read-only state;
- whether a real flush callback exists.

It does not advertise long names, truncate, or valid free-byte accounting.

### Nested FAT32 read traversal

Existing nested 8.3 directory trees can be:

- traversed;
- listed;
- statted;
- opened;
- read.

Mutation remains intentionally narrower: create, write, unlink, and rename are
still root-entry operations. Rejecting unsupported nested mutation is part of the
current safety contract.

## Remaining v0.3 work

The dependency order is:

1. complete `SEEK_CUR` and `SEEK_END` semantics;
2. add truncate with safe cluster shrink/grow and rollback behavior;
3. add mkdir and rmdir with explicit partial-write and cleanup rules;
4. extend create, unlink, rename, and move to nested directories;
5. define application-visible flush/fsync semantics;
6. add reboot-persistence evidence on exact images and workflows.

Every new operation must define:

- canonical path behavior;
- read-only behavior;
- partial progress;
- specific status results;
- rollback or recoverable failure boundary;
- fixed-capacity limits;
- host fixtures and QEMU evidence;
- one real userland consumer.

## Boundary with v0.4

Long-file-name support belongs to the real-FAT phase after generic mutation and
durability contracts are established. Adding VFAT parsing before safe directory
mutation would expand format surface without solving the more important ownership
and rollback rules.

## Current non-claims

ArmoniOS does not yet claim:

- exact FAT32 free-space accounting;
- VFAT long names;
- directory creation or removal;
- nested mutation transactions;
- safe path-level replacement of a longer file with a shorter one;
- durable writes proven across reboot;
- ext2;
- broad FAT interoperability;
- physical Raspberry Pi storage support.
