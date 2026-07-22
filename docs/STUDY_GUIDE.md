# Study Guide

ArmoniOS is easiest to study as a small operating system first and a desktop
demo second. The educational core is the path from boot to user processes:

1. `boot/start.S`, `kernel/kernel.c`, and `kernel/mm/` for entry, memory
   discovery, PMM, heap, page tables, and MMU enablement.
2. `kernel/process.c`, `kernel/sched/`, `kernel/user_image.c`, and
   `kernel/user_vm.c` for EL0 image loading, process state, scheduling, and
   anonymous mappings.
3. `kernel/syscall*.c` and `kernel/syscall_numbers.h` for the syscall ABI and
   user-copy boundary.
4. `kernel/vfs.c`, `kernel/bootfs.c`, `kernel/tmpfs.c`, and `kernel/fat32.c`
   for the simple filesystem stack.
5. `kernel/gui_*`, `drivers/input/`, `drivers/usb/`, and
   `drivers/input/virtio_input.c` for GUI/input event flow.
6. `programs/libkarm/` and the small apps in `programs/apps/` for how EL0
   code calls back into the kernel.

The current visible apps are `shell`, `editor`, `files`, `monitor`, `control`,
and `clock`, launched by `panel`. They are useful examples of userland,
syscalls, GUI usage, and persistence, but they are not yet complete daily-use
tools. Treat `control` in particular as a demo of userland state and persistence
plumbing, not as kernel core material.

After the boot-to-userland path is clear, read `docs/ROADMAP.md` as the product
path from v0.1 to v1.0. The most important next study areas are the syscall
copy boundary, the VFS/FAT split, a real mount/path/storage model, the planned
FAT and ext2 drivers, and shared userland/runtime widgets for making the apps
useful.

For verification habits, start with `bash tools/verify.sh`, then read
`docs/CURRENT_STATE.md` before making claims. Host tests prove pure logic and
mocked driver contracts; QEMU marker tests prove serial-observable runtime
paths; visible workflow claims still require named manual testing.
