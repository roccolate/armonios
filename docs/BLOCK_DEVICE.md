# Generic Block Device Contract

## Status

The v0.3 storage foundation now has four completed layers:

1. a transport-neutral descriptor and bounded view contract;
2. transport adapters for QEMU virtio-blk and diagnostic EMMC;
3. a canonical board accessor that exposes the initialized device;
4. direct FAT32 and MBR integration over finite block-device descriptors.

The QEMU production path initializes a writable virtio descriptor, exposes it
through `board_storage_device()`, reads sector zero through
`block_device_read()`, and mounts FAT32 through `fat32_mount_device()`.
If sector zero is an MBR rather than a FAT32 boot sector, the kernel opens the
first supported FAT32 partition as a bounded `block_device_view_t` and mounts
that child device. No production FAT32 code manually adds a partition offset.

The old `board_storage_read()`, `board_storage_write()`, `fat32_mount()`, and
`fat32_set_write_sector()` interfaces remain compatibility shims for existing
callers and host fixtures. The legacy `block_view_t` implementation is no longer
used by QEMU, the kernel storage path, or the RPi4 EMMC diagnostic probe. It may
be removed in a later mechanical cleanup after downstream users have migrated.

The EMMC adapter is read-only because the current driver does not implement
writes. It derives card capacity from the CSD response and refuses to publish a
descriptor when the device is not ready or its capacity is invalid. The
Raspberry Pi 4 backend still does not advertise generic storage support; EMMC
remains an opt-in diagnostic path until physical hardware evidence exists.

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
there is no durable-write guarantee until a transport implements and negotiates
a real flush operation.

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

`mbr_open_fat32_partition()` reads sector zero through the parent descriptor,
parses the first supported primary FAT32 entry, and returns a view only when the
entire partition lies inside the physical device. Filesystem code receives that
bounded child rather than a raw offset.

## FAT32 integration

`fat32_mount_device()` is the canonical mount entry point. It requires:

- a valid finite descriptor;
- 512-byte logical blocks;
- a read callback;
- a write callback when the descriptor is not marked read-only;
- a BPB total-sector count no larger than the supplied device or view.

A writable descriptor is automatically connected to the existing FAT32 write
path. A read-only descriptor never receives a write callback. `fat32_flush()`
propagates to the mounted descriptor when one exists; callback-backed legacy
fixtures have no flush contract and therefore return success without claiming
durability.

The kernel tries a whole-device FAT32 mount first, then an MBR-bounded mount.
This preserves support for both superfloppy images and partitioned media while
keeping every filesystem access inside a measured address space.

## Transport adapters

### QEMU virtio-blk

`virtio_blk_block_device.h` publishes:

- the capacity reported by the virtio configuration space;
- 512-byte logical blocks;
- writable read and write callbacks;
- no flush callback until virtio flush negotiation is implemented.

The QEMU board initializes this descriptor after the transport queue is ready.
The canonical accessor returns it to the kernel; the legacy board callbacks
delegate through the same object.

### Diagnostic EMMC

`emmc_block_device.h` publishes:

- capacity decoded from CSD v1 or high-capacity CSD layouts;
- 512-byte logical blocks;
- read-only state;
- the existing multi-sector EMMC read operation;
- no write or flush callbacks.

The opt-in RPi4 probe reads sector zero, mounts whole-device FAT32 or opens an
MBR view, and reports geometry through this descriptor. Its presence does not
change RPi4 board capabilities and does not claim that physical SD/eMMC bring-up
is complete.

## Current evidence

The storage host gates prove:

- invalid descriptors are rejected;
- reads and writes cannot cross device or view bounds;
- read-only policy is inherited and enforced;
- flush reaches the parent descriptor;
- writable and nested views translate to the expected physical block;
- FAT32 mounts directly from a read-only block-device view;
- the FAT32 BPB cannot claim more sectors than the supplied descriptor;
- MBR discovery returns a bounded child and rejects partitions past media end;
- virtio adapters preserve capacity and multi-block translation;
- EMMC CSD v1 and high-capacity layouts produce bounded sector counts;
- the EMMC descriptor remains read-only and rejects an unready transport.

The production gates additionally exercise QEMU FAT32 mounting, file reads and
writes, framebuffer/FAT integration, repeated VMM boots, and the RPi4 diagnostic
build.

## Migration sequence

1. **Complete:** establish and test the generic descriptor and view contract.
2. **Complete:** publish QEMU virtio-blk and diagnostic EMMC as block devices.
3. **Complete:** expose the canonical board descriptor while retaining wrappers.
4. **Complete:** mount FAT32 directly from whole devices and bounded MBR views.
5. **Complete:** migrate the RPi4 EMMC diagnostic path away from `block_view_t`.
6. **Complete at the abstraction boundary:** expose flush propagation without
   claiming durable writes where the transport has no flush operation.

## Remaining non-goals

- changing the VFS path model;
- adding FAT long filenames or nested directories;
- changing the on-disk FAT32 implementation;
- exposing storage descriptors to EL0;
- claiming Raspberry Pi storage support without hardware evidence;
- implementing virtio flush negotiation or a general writeback cache.
