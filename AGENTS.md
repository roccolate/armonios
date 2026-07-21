# Agent Guide

This file is the live entry point for agents working in this repository.

## Current project classification

ArmoniOS is a **v0.1 QEMU desktop baseline** and an active **v0.2
cleanup/runtime-hardening candidate**. It is a real freestanding AArch64
operating system, not a Linux application or distribution.

The runtime foundation measures requests, coalescing, requeues,
last/maximum/cumulative generic-counter duration, interval overruns, input
produced/consumed, successful redraws, consumed virtio-net RX frames, and input
queue depth/high-water/overflow.

Measurement does not complete v0.2. The service still has no class budgets,
global deadline, reliable network device-drop signal, or sustained-load QEMU
proof of EL0 progress. Track remaining work in issue #43 and RISK-017.

Issue #2 is the **v0.6 useful desktop applications** milestone. Do not treat it
as v1.1 work or use it to bypass v0.3 storage/VFS, v0.4 real FAT, and v0.5 shared
runtime/widget foundations.

The release claim applies only to QEMU `virt`. Raspberry Pi 4 remains
build-verified, fail-closed scaffolding without physical boot, storage, display,
or input evidence.

## Read first

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

`CURRENT_STATE.md` is operational truth. `ROADMAP.md` describes future intent and
must never be used as evidence that a feature exists.

## Architectural facts

- EL0 processes are preemptive.
- EL1 helper threads are cooperative.
- Timer-originated GUI, input, device, and network work runs in one post-EOI
  bottom half.
- EOI is not exception return: EL0 remains paused and the vector's IRQ-mask state
  is preserved while the pass executes.
- `irq_disable()`/`irq_enable()` are nested state-preserving helpers; do not
  replace them with unconditional DAIF clear/set pairs.
- The timer callback is bounded; the complete exception path is measured but not
  yet bounded.
- Timing uses `CNTPCT_EL0`; `CNTFRQ_EL0` supplies conversion; one timer interval
  is only an observation threshold.
- Work-class reports are accepted only during the active runtime pass so console
  thread activity is not mixed into bottom-half telemetry.
- Input overflow is counted but not prevented.
- Consumed network frames are measured, but the 16-descriptor virtio RX path has
  no reliable device-drop/ring-overflow counter.
- Device operations and compositor damage/full-redraw batches are not yet
  measured.
- Runtime telemetry is kernel-internal. Do not expose the internal structure
  directly as a syscall ABI.
- Pending state and telemetry assume one CPU and one consumer; they are not
  SMP-safe synchronization.
- User-copy permission validation exists, but final copies are not
  fault-recoverable.
- Each process still carries kernel mappings in TTBR0; TTBR1, ASIDs, and scoped
  TLB invalidation are future hardening.
- FAT32 is root-directory 8.3 only.
- QEMU is the only verified runtime platform.

## Verification workflow

Use the smallest relevant test while developing, then run:

```sh
bash tools/verify.sh
```

The baseline includes:

- QEMU and RPi4 build contracts;
- `.data == 0` and the 108000-byte kernel limit;
- RPi4 EMMC2 diagnostic, MBR, and block-view gates;
- native kernel, VFS, filesystem, GUI, parser, driver, and ABI tests;
- runtime EOI/coalescing/timing/requeue/reset and indexed input/redraw/network
  metric checks;
- static network metric wiring validation;
- input queue depth/high-water/overflow regression;
- parent/wait and process-local FD isolation;
- user-copy and KLI1 contracts;
- userland stack checking;
- FAT32 QEMU smoke;
- usercopy and focus QEMU regressions;
- framebuffer, USB, and network markers;
- visible-target FAT + GPU wiring.

Manual visual evidence is separate:

```sh
make qemu-fb-visible
```

Record tester, date, exact commit, workflow, observed behavior, and limitations.
Serial markers are not visible manual validation.

## Change discipline

### Runtime and interrupt changes

- Keep hard-IRQ callbacks bounded.
- Do not add blocking operations, unbounded scans, queue drains, or rendering to a
  hard-IRQ callback.
- Treat timing and work counts as measurement, not proof of a bound.
- Keep reports compact and count completed work, not polling attempts.
- Do not infer “no drops” from consumed-frame counts.
- Preserve or republish pending work when future budgets expire.
- Add host tests and deterministic QEMU progress tests for exception/scheduler
  boundary changes.
- Preserve `.data == 0`, W^X, kernel-size, and stack limits.

### Syscall or ABI changes

- Append syscall numbers; never reuse one.
- Land kernel, `libkarm`, tests, and ABI documentation together.
- Import caller data into kernel-owned buffers before lower layers use it.
- Validate complete output destinations before consuming queued state.
- Define a versioned diagnostic ABI before exposing runtime telemetry to Monitor.

### Storage changes

- Keep filesystem selection out of generic VFS code.
- Preserve fail-closed behavior on unverified hardware.
- Add host-image tests before broader FAT or partition claims.
- Do not call the current root-only FAT32 bridge general FAT.

### Userland changes

- Shipping KLI1 images keep mutable `.data` and `.bss` empty.
- Put large mutable state in `SYS_MMAP`, not the fixed stack.
- Keep stack checks green.
- Add shared helpers only when they reduce duplication or image size.
- Respect milestone order before broad v0.6 app work.

## Documentation rules

- Keep technical documentation in English.
- Update from evidence, not intent.
- Use exact commits and run IDs for promoted baselines.
- Separate IMPLEMENTED, HOST-, BUILD-, QEMU-, CI-, MANUAL-, and physical-hardware
  evidence.
- A validated PR tree is not automatically a separately tested merge commit.
- Synchronize README, current state, roadmap, risks, and this guide when release
  state changes.
- Do not add archive, historical handoff, duplicate status, or obsolete pre-reset
  references.
