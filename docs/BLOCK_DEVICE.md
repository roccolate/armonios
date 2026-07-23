# Generic Block Device Contract

## Status

The v0.3 storage foundation now has two completed layers:

1. a transport-neutral descriptor and bounded view contract;
2. transport adapters for QEMU virtio-blk and diagnostic EMMC.

The QEMU board uses its writable block-device descriptor internally while
retaining the existing `board_storage_read()` and `board_storage_write()`
functions as compatibility wrappers. FAT32 still consumes those wrappers until
the canonical device mount lands in the next cut.

The EMMC adapter is read-only because the current driver does not implement
writes. It derives the card capacity from the CSD response and refuses to
publish a descriptor when the device is not ready or its capacity is invalid.
The Raspberry Pi 4 backend still does not advertise generic storage support;
EMMC remains an opt-in diagnostic path until physical hardware evidence exists.

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
`block_device_flush()` succeeds as a no-op. This does not claim persistence;
there is no durable-write guarantee until a transport and filesystem flush
policy is explicitly implemented and tested.

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

## Transport adapters

### QEMU virtio-blk

`virtio_blk_block_device.h` publishes:

- the capacity reported by the virtio configuration space;
- 512-byte logical blocks;
- writable read and write callbacks;
- no flush callback until virtio flush negotiation is implemented.

The QEMU board initializes this descriptor after the transport queue is ready.
Its legacy board callbacks delegate through the descriptor, so the production
path now exercises the shared storage vocabulary without changing the kernel or
FAT32 API yet.

### Diagnostic EMMC

`emmc_block_device.h` publishes:

- capacity decoded from CSD v1 or high-capacity CSD layouts;
- 512-byte logical blocks;
- read-only state;
- the existing multi-sector EMMC read operation;
- no write or flush callbacks.

The adapter is separately host-tested. Its presence does not change RPi4 board
capabilities and does not claim that physical SD/eMMC bring-up is complete.

## Current evidence

`tests/run_block_view_fat32_test.sh` builds host tests that prove:

- invalid descriptors are rejected;
- reads and writes cannot cross device or view bounds;
- read-only policy is enforced by the generic helper;
- flush reaches the parent;
- writable views translate writes correctly;
- nested views translate to the expected physical block;
- FAT32 mounts and lists through a read-only block-device view;
- virtio adapters preserve capacity and multi-block translation;
- EMMC CSD v1 and high-capacity layouts produce bounded sector counts;
- the EMMC descriptor remains read-only and rejects an unready transport.

## Migration sequence

1. **Complete:** establish and test the generic descriptor and view contract.
2. **Complete:** publish QEMU virtio-blk and diagnostic EMMC as block devices
   with honest capacity and read-only metadata.
3. **Next:** add a canonical `fat32_mount_device()` path while retaining callback
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
