# Generic Block Device Contract

## Status

This document defines the first v0.3 storage foundation. The contract is
host-testable and transport-neutral. It does not yet replace every legacy board
or FAT32 callback.

## Descriptor

`drivers/storage/block_device.h` describes a finite block address space with:

- a 64-bit block count;
- an explicit block size;
- read and optional write operations;
- an optional flush operation;
- an explicit read-only flag;
- caller-owned driver context.

The generic helpers validate the whole requested range before dispatching an
operation. A non-empty transfer may not wrap, cross the device boundary, or use
a null buffer. Zero-length transfers are accepted at any position up to and
including the end of the device.

A writable descriptor must provide a write callback. A read-only descriptor
rejects writes even if an underlying callback exists. A missing flush callback
means the device has no deferred flush work at this abstraction boundary, so
`block_device_flush()` succeeds as a no-op.

## Views

`block_device_view_t` exposes a bounded interval of a parent device as another
`block_device_t`.

A view:

- preserves the parent block size;
- translates child block zero to `base_block` in the parent;
- validates its complete interval during initialization;
- inherits parent read-only state;
- may force an otherwise writable parent view to read-only;
- delegates flush to the parent;
- may itself be used as the parent of another view.

This is the intended basis for MBR partitions and future nested storage views.
Filesystem code should receive the bounded view rather than manually adding a
partition offset.

## Current evidence

`tests/run_block_view_fat32_test.sh` builds a host test that proves:

- invalid descriptors are rejected;
- reads and writes cannot cross device or view bounds;
- read-only policy is enforced by the generic helper;
- flush reaches the parent;
- writable views translate writes correctly;
- nested views translate to the expected physical block;
- FAT32 mounts and lists through a read-only block-device view.

## Migration sequence

1. Establish and test the generic descriptor and view contract.
2. Add board adapters that publish QEMU virtio-blk and diagnostic EMMC as block
   devices with honest capacity and read-only metadata.
3. Add a canonical `fat32_mount_device()` path while retaining callback
   compatibility temporarily.
4. Replace the legacy `block_view_t` callback wrapper with block-device views.
5. Make partition discovery return bounded descriptors rather than raw offsets.
6. Add explicit filesystem flush policy before claiming durable writes.

## Non-goals of this cut

- changing the VFS path model;
- adding FAT long filenames or directories;
- changing the on-disk FAT32 implementation;
- exposing storage descriptors to EL0;
- claiming Raspberry Pi storage support;
- removing legacy callbacks before all callers migrate.
