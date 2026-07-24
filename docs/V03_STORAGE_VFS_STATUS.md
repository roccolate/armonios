# v0.3 Storage and VFS status

This document is the post-audit source of truth for the v0.3 storage/VFS work
already merged after the older `CURRENT_STATE.md`, `ARCHITECTURE.md`, and
`ROADMAP.md` snapshots were written.

It does not replace release evidence. Workflow identities remain recorded in the
merged pull requests and must be copied into the canonical release record before
promotion.

## Completed foundations

The following v0.3 foundations are implemented on `main`:

1. **Generic block-device contract**
   - bounded reads and writes;
   - explicit capacity and logical block size;
   - read-only enforcement;
   - bounded nested block-device views;
   - flush delegation;
   - virtio-blk and diagnostic EMMC adapters.

2. **FAT32 mounting through block devices**
   - direct `block_device_t` mount path;
   - whole-device and primary-MBR FAT32 discovery;
   - partition bounds enforced by block-device views;
   - BPB geometry constrained by the selected device/view;
   - QEMU production storage no longer depends on loose sector callbacks.

3. **Canonical VFS paths and mount resolution**
   - absolute paths only;
   - repeated separators collapsed;
   - `.` removed;
   - `..` resolved with root escape rejected;
   - trailing slash removed except for `/`;
   - canonical node and mount identity;
   - longest component-prefix mount selection;
   - cross-mount rename rejected after canonical resolution.

4. **Nested FAT32 directory traversal**
   - existing FAT32 8.3 directory trees can be traversed;
   - nested regular files can be opened and read;
   - root and nested directories can be listed;
   - `.` and `..` entries are hidden from user listings;
   - VFS `stat` and listing callbacks receive complete canonical paths;
   - nested mutation remains deliberately unsupported.

## Structured metadata work in progress

PR #95 adds the next append-only public interfaces while keeping the global ABI
identifier at **1.0** until the first official ArmoniOS release:

- `SYS_STAT_V2 = 49`;
- `SYS_READDIR_V2 = 50`;
- fixed, versioned `arm_stat_v2_t` and `arm_dirent_v2_t` layouts;
- file/directory type information;
- size and generic attribute fields;
- indexed directory pagination;
- typed `libkarm` wrappers;
- preservation of legacy `SYS_STAT = 45`, `SYS_READDIR = 46`, and `arm_stat_t`.

Before promotion, PR #95 should use native structured VFS metadata internally
rather than reconstructing records from the legacy newline stream, and one real
EL0 consumer should use the new interface.

## Current limits that remain intentional

- path components and FAT names are still short-name oriented;
- the complete VFS path remains fixed-capacity;
- FAT long-file-name entries are not exposed;
- `mkdir` and `rmdir` do not exist;
- nested create, unlink, rename, and move are rejected;
- truncate/shrink is not a complete generic filesystem operation;
- no public filesystem-information query exists;
- error codes are too coarse for directory mutation;
- durable application-visible flush/persistence is not yet promised;
- ext2 remains unimplemented.

## Recommended next cuts

The dependency order is:

1. finish native structured VFS metadata and promote PR #95;
2. add filesystem-specific error codes (`EXIST`, `NOTDIR`, `ISDIR`, `NOTEMPTY`,
   `NOSPC`, `ROFS`, `NOTSUP`, `RANGE`);
3. add structured filesystem information and mount capability reporting;
4. complete `SEEK_CUR` and `SEEK_END` semantics;
5. add safe truncate/ftruncate with cluster grow/shrink rollback tests;
6. add `mkdir` and `rmdir` together with explicit partial-write policy;
7. extend create/unlink/rename/move to nested directories;
8. expose flush/fsync and add reboot-persistence evidence;
9. implement VFAT long names;
10. add ext2 read-only through the generic mount interface.

## ABI version policy

During pre-release development, additive interfaces may be added while the global
ABI identifier remains `1.0`. The first official release establishes the frozen
compatibility baseline. After that release:

- append-only compatible additions advance the minor version;
- intentionally incompatible changes advance the major version;
- syscall numbers and published structure layouts are never silently reused.
