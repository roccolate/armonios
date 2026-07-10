# Current State

ArmoniOS is currently at the **v0.9 QEMU desktop baseline**.

The current focus is turning that baseline into a repeatable **v1.0 QEMU desktop release candidate**.

## What currently works

- AArch64 kernel boot on QEMU `virt`.
- EL1 kernel with EL0 user programs.
- Flat KLI1 user images loaded from bootfs.
- Process table, scheduler, `spawn`, `wait`, `exit`, and `kill` syscalls.
- User anonymous memory mapping through `mmap` / `munmap`.
- Basic VFS with bootfs, tmpfs, FAT32 root mount, and dynamic FAT32 file nodes.
- FAT32 root-directory 8.3 create, read, write, rename, delete, list.
- Dynamic `/fat/<name>` VFS nodes are invalidated after successful rename/delete so stale paths and open descriptors fail cleanly.
- Host coverage exists for VFS static unmount and FAT32 dynamic node invalidation paths.
- Virtio block storage path for QEMU.
- Virtio framebuffer desktop path for QEMU.
- GUI compositor, windows, title bars, focus, drag, minimize/restore, close events, cursor, backing buffers, damage tracking.
- Panel, shell, editor, files, monitor, clock apps.
- USB HID keyboard/mouse parsing and basic QEMU USB path.
- Virtio input and basic virtio-net DHCP path.
- System info syscalls: `timeinfo`, `meminfo`, `proclist`.

## What is not claimed yet

- Real Raspberry Pi 4 boot.
- USB hubs.
- TCP/HTTP applications.
- Automated visual screenshot diffing.
- SMP.
- Audio.
- Multimedia/game runtime APIs.

## Current release gates

Before calling this v1.0, run:

```sh
make
make size
make -C tests test
make stack-check
make qemu-fs-test
timeout 25s make qemu-fb
timeout 25s make qemu-usb
timeout 25s make qemu-net
```

Manual visible pass:

```sh
make qemu-fb-visible
```

## Current manual workflow still to confirm

In `make qemu-fb-visible`, verify:

- open `files` from panel;
- list `/fat`;
- create an 8.3 file;
- open it in `editor`;
- type content;
- save with Ctrl-S;
- close editor;
- return to `files`;
- rename the file;
- reopen renamed file and confirm saved content;
- delete the file;
- list `/fat` again and confirm it is gone;
- confirm there is no user fault, compositor blank frame, scheduler stall, or stale editor path.

## Current project stance

v1.0 is **QEMU desktop stability**, not Raspberry Pi hardware.

Raspberry Pi 4 bring-up belongs to the later v1.5 track.

Engine/multimedia work belongs after the desktop core is stable and should start in userland unless a kernel ABI need is proven.
