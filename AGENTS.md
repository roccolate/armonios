# Agent Guide

This file is the live entry point for agents working in this repository.

## Current project classification

ArmoniOS is a **v0.1 QEMU desktop baseline** with most of the original v0.2
cleanup work already implemented. It is a real freestanding AArch64 operating
system, not a Linux application or distribution.

The validated runtime tree from PR #41 was merged into `main` by commit
`ea13f0e`. The exact PR head validated by GitHub Actions was `fedb070`:

- `Verify ArmoniOS` run `29824050151`: success;
- `CI - Tests` run `29824050165`: success.

Those runs validate the code tree that was merged. The merge commit itself did
not receive a separate workflow run. Do not describe it as independently
retested.

The current release claim applies only to QEMU `virt`. Raspberry Pi 4 remains
build-verified, fail-closed scaffolding without physical boot, storage, display,
or input evidence.

## Read first

Read these documents in order before changing code or making public claims:

1. `docs/DOCUMENTATION_POLICY.md`
2. `docs/CURRENT_STATE.md`
3. `docs/TECHNICAL_RISKS.md`
4. `docs/ROADMAP.md`
5. `docs/ARCHITECTURE.md`
6. `docs/RUNTIME_SERVICE.md`
7. `docs/MEMORY_MAP.md`
8. `docs/SYSCALLS.md`
9. `docs/GUI_ABI_NOTES.md`
10. `docs/CONTRIBUTING.md`
11. `docs/PORTING.md`

`CURRENT_STATE.md` is the operational source of truth. `ROADMAP.md` describes
future intent and must never be used as evidence that a feature exists.

## Architectural facts that must remain explicit

- EL0 processes are preemptive.
- EL1 helper threads are cooperative.
- Timer-originated GUI, input, device, and network work runs in one post-EOI
  bottom half.
- EOI does **not** mean exception return: IRQs remain masked and EL0 remains
  paused while the current runtime-service pass executes.
- The runtime pending mask is valid under the current single-core, one-consumer
  model. It is not an SMP-safe work queue.
- User-copy permission validation exists, but the final copy is not
  fault-recoverable.
- Each process still carries kernel mappings in TTBR0; TTBR1, ASIDs, and scoped
  TLB invalidation are future hardening.
- FAT32 is limited to root-directory 8.3 files.
- QEMU is the only verified runtime platform.

## Verification workflow

Use the smallest relevant test while developing, then run the full promotion
gate before merging code or changing a verified claim:

```sh
bash tools/verify.sh
```

The current baseline includes:

- QEMU and RPi4 build contracts;
- `.data == 0` and the 108000-byte kernel limit;
- RPi4 EMMC2 diagnostic, MBR, and block-view host gates;
- native kernel, VFS, filesystem, GUI, driver-parser, and ABI tests;
- deferred runtime-service ordering/coalescing checks;
- parent/wait lifecycle and process-local FD isolation;
- user-copy and KLI1 contracts;
- userland stack checking;
- FAT32 QEMU smoke;
- usercopy and focus QEMU regressions;
- framebuffer, USB, and network marker gates;
- visible-target FAT + GPU wiring.

Manual visual behavior is a separate evidence class:

```sh
make qemu-fb-visible
```

Record the tester, date, exact commit, workflow, observed behavior, and any
limitations. Automated serial markers must not be presented as visible manual
validation.

## Change discipline

### Kernel or driver changes

- Keep hard-IRQ handlers bounded.
- Do not add blocking operations, unbounded queue drains, complete device scans,
  or rendering to an IRQ callback.
- Preserve `.data == 0`, W^X mappings, kernel-size, and stack limits.
- Add focused host tests and a QEMU regression when behavior crosses an
  exception, device, process, or user/kernel boundary.

### Syscall or ABI changes

- Append syscall numbers; never reuse an existing number.
- Update kernel implementation, `libkarm` wrapper, tests, and `SYSCALLS.md` in
  the same change.
- Import caller data into kernel-owned buffers before lower layers use it.
- Validate the entire output destination before consuming queued state.

### Storage changes

- Keep filesystem selection out of generic VFS code.
- Preserve fail-closed behavior on unverified hardware.
- Add host-image tests before making broader FAT or partition claims.
- Do not call the current root-only FAT32 bridge a general FAT implementation.

### Userland changes

- Shipping KLI1 images must keep mutable `.data` and `.bss` empty.
- Put large mutable state in `SYS_MMAP`, not the fixed process stack.
- Keep the stack gate green.
- Prefer shared helpers only when they reduce duplication or image size.

## Documentation rules

- Keep technical documentation in English.
- Update documentation from evidence, not intent.
- Use exact commits and workflow run IDs for promoted baselines.
- Separate IMPLEMENTED, HOST-VERIFIED, BUILD-VERIFIED, QEMU-VERIFIED,
  CI-VERIFIED, MANUAL-VERIFIED, and physical-hardware evidence.
- Keep README, `CURRENT_STATE.md`, `ROADMAP.md`, `TECHNICAL_RISKS.md`, and this
  guide synchronized whenever release state changes.
- Do not add archive, historical handoff, or duplicate status documents.
- Do not reintroduce obsolete pre-reset baseline references.
