# Codex Notes for KolibriARM

This repo is an early AArch64 bare-metal OS inspired by KolibriOS/MenuetOS. Keep changes small, testable, and close to the existing C + AArch64 assembly style.

## Current Direction

The first userland hello world is implemented as an embedded EL0 demo:

- Enter EL0 from the kernel with `eret`.
- Handle `svc #0` exceptions from EL0.
- Dispatch syscall number from `x8`.
- `sys_write` forwards stdout/stderr to UART after validating the demo user range.
- `sys_exit` returns control to the kernel through the EL0 exception frame.
- Keep the embedded user program until a tiny loader-owned image exists.

## Boundaries

- Do not port KolibriOS x86 assembly literally. Port ideas, ABI shape, IPC/message concepts, GUI concepts, and small demos by reimplementing them for AArch64.
- Keep QEMU-specific addresses out of generic kernel code when touching related areas.
- Prefer a `drivers/boards/qemu_virt/` platform layer before adding Raspberry Pi support.
- Avoid introducing libc, POSIX assumptions, hosted runtime behavior, or large abstractions.
- No vendored third-party protocol stacks (lwIP, FreeRTOS, etc.). The kernel's net stack is hand-written in `kernel/net/`.
- Keep the kernel readable enough to understand in one sitting.

## Build and Test

Use these before committing kernel changes:

```bash
make
make size
make -C tests test
```

For boot/runtime checks, use:

```bash
make qemu
make qemu-debug
```

## Implementation Style

- C code is freestanding and compiled with `-ffreestanding -nostdlib`.
- AArch64 assembly should be minimal, documented where control flow is subtle, and kept near the CPU boundary.
- Use existing modules before adding new ones: `kernel/mm`, `kernel/sched`, `kernel/timer`, `drivers/irq`, `drivers/uart`.
- Add focused host tests for pure C logic when possible, especially memory-management code.
- Do not hide important hardware behavior behind vague abstractions; name the architectural thing being controlled.

## Documentation

When changing direction or milestones, update `ROADMAP.md`. When changing build/run expectations, update `README.md`. When moving board-specific code, update `PORTING.md`.
