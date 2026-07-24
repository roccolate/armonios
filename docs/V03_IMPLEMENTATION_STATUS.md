# v0.3 storage/VFS implementation status

> Live implementation checkpoint for the v0.3 storage/VFS work.
>
> This document records code already merged after the older documentation audit.
> Release evidence and manual promotion status remain governed by
> `CURRENT_STATE.md`, `TECHNICAL_RISKS.md`, and issue #76.

## Identity

- checkpoint date: 2026-07-23;
- audited `main`: `a078c995f485bab84135233c149e28ba081b11b0`;
- v0.2 kernel/runtime defect issue #63: closed after the EL1/EL0 IRQ-origin fix;
- remaining v0.2 task: issue #76, the dated visible QEMU validation and release record;
- global public ABI remains `1.0` until the first official ArmoniOS release.

## Landed foundations

The following v0.3 foundations are already merged:

1. **Generic block-device contract** — PR #80
   - bounded read/write ranges;
   - block size and block count;
   - read-only flag;
   - flush contract;
   - bounded nested block-device views.

2. **Board storage adapters** — PR #81
   - writable QEMU virtio-blk descriptor with real capacity;
   - read-only EMMC diagnostic descriptor with CSD-derived capacity.

3. **Block device to MBR to FAT32 path** — PR #82
   - `board_storage_device()` production path;
   - FAT32 mounting through `block_device_t`;
   - whole-device and primary-MBR FAT32 discovery;
   - bounded partition views;
   - read-only and flush propagation.

4. **Canonical paths and mount resolution** — PR #90
   - absolute-path normalization;
   - repeated slash and `.` collapse;
   - bounded `..` resolution with root-escape rejection;
   - longest component-prefix mount selection;
   - canonical identity for nodes and mounts.

5. **Nested FAT32 traversal** — PR #93
   - read-only traversal of existing nested 8.3 directory trees;
   - open/read regular files below subdirectories;
   - list and stat nested directories;
   - path-aware VFS callbacks;
   - explicit rejection of nested create/unlink/rename until mutation transactions exist.

6. **Structured metadata ABI and first consumer** — PR #95
   - squash merge `a078c995f485bab84135233c149e28ba081b11b0`;
   - filesystem-neutral `vfs_metadata_t` and `vfs_dirent_t`;
   - native FAT32 type, size, and attribute mapping;
   - `SYS_STAT_V2 = 49` and `SYS_READDIR_V2 = 50`;
   - fixed versioned public records with the global ABI still at 1.0;
   - typed `libkarm` wrappers;
   - Files as the first EL0 consumer with a legacy fallback;
   - legacy calls 45 and 46 remain unchanged;
   - final validation: Export FAT32 #157, Verify ArmoniOS #496, and CI - Tests #630 succeeded.

## Remaining v0.3 work

After structured metadata, the correct order is:

1. filesystem-specific error codes;
2. filesystem information/capability reporting;
3. complete seek semantics;
4. truncate and safe cluster shrink/grow;
5. mkdir and rmdir with explicit rollback rules;
6. nested create/unlink/rename/move;
7. explicit flush/fsync and reboot-persistence evidence.

Long file names belong to the following real-FAT phase, after the generic mutation
and durability contracts are established.

## Current scope boundary

ArmoniOS still does **not** claim:

- a completed or tagged v0.2 release until issue #76 is completed;
- long FAT names;
- directory creation or removal;
- nested mutation transactions;
- durable writes across reboot;
- ext2;
- a stable frozen post-release ABI;
- physical Raspberry Pi runtime support.
