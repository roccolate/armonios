# Roadmap

This roadmap starts from the v0.1 QEMU desktop baseline and targets a real
v1.0 mini desktop OS. It is ordered by user value, evidence, and maintenance
risk, not by feature novelty.

Status claims must follow `DOCUMENTATION_POLICY.md`. Current operational truth
lives in `CURRENT_STATE.md`; correctness, security, and hardware risks live in
`TECHNICAL_RISKS.md`.

## Product target

ArmoniOS v1.0 is a usable QEMU desktop OS.

The v1.0 user should be able to boot to the desktop, browse persistent storage,
edit multi-line text, save settings, use a command shell, inspect processes and
memory, and verify that files and configuration survive a reboot.

Chosen defaults:

- primary v1 platform: QEMU `virt` desktop;
- product shape: small personal desktop OS;
- compatibility policy: internal APIs may change before v1; existing userland
  syscall numbers and KLI1 layout should stay stable unless a specific
  incompatibility is documented;
- v1 filesystems: real FAT as the primary writable format and ext2 as a
  second mounted format, at least read-only;
- hardware track: Raspberry Pi remains separate from the v1 QEMU release line.

## v0.1 baseline

**State:** stable QEMU desktop baseline.

v0.1 means:

- the full automated baseline passes through `bash tools/verify.sh`;
- the visible Files/Editor/FAT workflow has dated manual evidence;
- syscall-boundary, VFS descriptor, KLI1, kernel W^X, deterministic QEMU, and
  CI evidence risks are closed for the QEMU baseline;
- Raspberry Pi 4 remains build-verified scaffolding only.

The goal of v0.1 was to create a clean first public history point for continued
work.

## v0.2 cleanup and hardening

Goal: make the kernel safer to evolve before adding broad functionality.

- Harden the syscall/user-copy boundary so internal layers receive
  kernel-owned buffers rather than raw EL0 pointers.
- Make Raspberry Pi storage fail closed while eMMC remains unverified.
- Decouple VFS from FAT32-specific `/fat/` knowledge and remove the global
  default-FAT authority.
- Keep `bash tools/verify.sh` as the promotion gate.
- Avoid user-visible features unless they are needed to preserve behavior after
  cleanup.

Exit criteria:

- `bash tools/verify.sh` passes;
- VFS no longer includes or dispatches FAT32 directly;
- `BOARD=rpi4` still builds without claiming working storage;
- documentation records any changed risk state.

## v0.3 storage and VFS platform

Goal: turn storage from a fixed demo path into a filesystem platform.

- Add an internal block-device layer with sector size, sector count,
  read/write, flush, and read-only state.
- Add a mount table for `/fat`, `/ext`, `/tmp`, and `/armonios`.
- Add a common path resolver for normalization, parent/child traversal, and
  mounted-root boundaries.
- Add an internal filesystem driver interface for probe, mount, open, read,
  write, readdir, stat, mkdir, unlink, rename, and truncate.
- Preserve existing syscall behavior while preparing new ABI extensions for
  structured directory entries and filesystem metadata.

Planned syscall extensions for this track:

- `SYS_MKDIR`;
- `SYS_TRUNCATE`;
- `SYS_STATX` with file type, size, and flags;
- `SYS_READDIRX` with structured directory entries;
- `SYS_FSINFO` with volume type, read-only state, and capacity information
  when available.

Exit criteria:

- host tests cover multiple mounts and nested paths;
- Shell can list `/`, `/fat`, `/tmp`, and `/armonios`;
- existing apps and syscalls continue to work;
- new ABI numbers and structs are documented before code promotion.

## v0.4 real FAT

Goal: replace the narrow FAT32 root-directory bridge with usable FAT storage.

- Integrate a real FAT driver behind the new filesystem interface.
- Support FAT12/16/32, long file names, subdirectories, create/read/write,
  rename, delete, mkdir, readdir, stat, and truncate.
- Support simple MBR partition discovery for QEMU disk images.
- Keep `/fat` as the primary writable desktop volume.
- Keep exFAT outside v1 unless it can be added without destabilizing the
  release line.

Exit criteria:

- host image tests cover long names, nested directories, file growth, truncate,
  rename, delete, and persistence;
- QEMU can create, edit, rename, delete, and reopen files in subdirectories;
- Files and Shell no longer depend on 8.3 names.

## v0.5 userland runtime and widgets

Goal: make application work practical without violating KLI1 constraints.

- Add a small userland heap/runtime in `libkarm` backed by `SYS_MMAP`.
- Add shared path, argv, dynamic-buffer, small-vector, and string helpers.
- Add `libkarmdesk` widgets for list views, text fields, buttons, status bars,
  scrolling, and simple dialogs.
- Keep shipping KLI1 images free of mutable `.data` and `.bss`.

Exit criteria:

- Editor, Files, Settings, and Monitor share common UI and parsing helpers;
- `make stack-check` stays under the configured limit;
- `tests/run_kli1_contract_test.sh` continues to reject mutable static storage.

## v0.6 useful desktop applications

Goal: convert the visible apps from useful demos into daily tools.

- Files: browse directories, switch volumes, open with Editor, create files and
  folders, rename, move, delete, refresh, and show type/size.
- Editor: real multi-line view, vertical and horizontal scrolling, dirty state,
  save/save-as, file truncation on save, and chunked loading where needed.
- Shell: improve `ls`, `cd`, `cat`, `run`, `kill`, `mem`, `ps`, and `ticks`;
  add `cp`, `mv`, `rm`, `mkdir`, `touch`, `echo`, `edit`, `open`, `df`, and
  `clear`.
- Settings: persist preferences in `/fat/CONFIG.INI` and make at least path
  defaults, clock visibility, theme choice, input repeat, and hostname
  observable.
- Monitor: show process state, memory, ticks, and support killing a selected
  process.

Exit criteria:

- a user can create a folder, write a multi-line file, copy it, rename it,
  delete it, and verify persistence after a QEMU reboot;
- Editor's single-visible-line issue is fixed or explicitly re-scoped with
  evidence;
- Settings changes at least three observable desktop behaviors.

## v0.7 ext2

Goal: add a second useful filesystem without blocking the main desktop path.

- Implement ext2 read-only support behind the filesystem interface.
- Support superblock parsing, block groups, inode lookup, directories, regular
  files, and clear rejection of unsupported features.
- Mount the volume at `/ext`.
- Defer ext2 writes unless the read-only path is already stable and the write
  surface can be tested thoroughly before v1.

Exit criteria:

- host tests read known ext2 images with nested directories and files;
- Files and Shell can navigate `/ext`;
- write attempts on read-only `/ext` fail with a clear error path.

## v0.8 desktop polish

Goal: make the OS feel coherent in a normal desktop session.

- Panel: launcher, taskbar, minimize/restore, clock, and storage status remain
  reliable under repeated use.
- Windows: resizing, focus, close, minimize, restore, and damage repaint have
  focused tests and visible validation.
- Apps share wording, status bars, and error language.
- Shell help and app-level status text are accurate and useful.
- Hardcoded geometry is reduced where it blocks usability.

Exit criteria:

- a 30-minute manual QEMU desktop pass records no crash, user fault, or data
  loss;
- deterministic focus, storage, and visible-wiring gates still pass;
- visible evidence is recorded in `CURRENT_STATE.md`.

## v0.9 v1 beta

Goal: stabilize rather than add features.

- Freeze syscall numbers and user-visible structs for v1.
- Freeze KLI1 v1 layout.
- Add fuzz-style and host image tests for path resolution, VFS, FAT, ext2, and
  app parsing.
- Add QEMU persistence and multi-process lifecycle gates where practical.
- Review kernel size, PMM pressure, GUI backing buffers, and userland heap use.
- Update every claim in the docs from evidence.

Exit criteria:

- `bash tools/verify.sh` passes;
- the v1 manual desktop workflow is recorded;
- no QEMU-desktop P0/P1 risk remains open.

## v1.0 release

v1.0 is ready when:

- ArmoniOS boots to the QEMU desktop;
- `/fat` is a real writable FAT volume with long names and directories;
- `/ext` mounts a stable ext2 volume, at least read-only;
- Files, Editor, Shell, Settings, Monitor, Panel, and Clock are useful;
- settings persist;
- process/window lifecycle cleanup remains reliable;
- syscall and KLI1 v1 contracts are documented;
- the final manual workflow is recorded:
  1. create a folder;
  2. create a text file;
  3. edit multiple lines;
  4. save, close, and reopen;
  5. copy, move, rename, and delete;
  6. change a setting;
  7. reboot QEMU;
  8. verify persisted files and settings.

## Hardware track

Raspberry Pi work stays outside the QEMU desktop release line until real
hardware evidence exists.

Required order:

1. controlled CPU entry and secondary-core parking;
2. repeatable serial marker on physical hardware;
3. memory and timer validation;
4. storage fail-closed cleanup, then read-only sector evidence;
5. mailbox/framebuffer bring-up;
6. input bring-up;
7. desktop workflow only after the earlier milestones pass.

Do not claim Raspberry Pi support before the hardware evidence rules in
`DOCUMENTATION_POLICY.md` and `PORTING.md` are met.

## Explicit non-goals before v1

- POSIX compatibility;
- dynamic linking;
- package management;
- TCP sockets or a browser;
- audio;
- SMP;
- accelerated graphics;
- Raspberry Pi desktop support;
- ext2 write support unless it can be completed without threatening v1.
