# Agent Guide

This file is the live entry point for agents working in this repository.

## Current project classification

ArmoniOS is a **v0.1 QEMU desktop baseline** and an active **v0.2
cleanup/runtime-hardening candidate**. It is a real freestanding AArch64
operating system, not a Linux application or distribution.

Runtime measurement Phase 1B is complete. Phase 2 currently enforces:

- at most 16 shared input-queue events per active post-EOI pass;
- at most 16 valid virtio-net RX frames per active post-EOI network pass;
- independent `RUNTIME_WORK_INPUT` and `RUNTIME_WORK_NETWORK` readiness;
- explicit input and network exhaustion counters;
- pending-work preservation after a real input leftover or network cap.

Input uses a reliable queue-depth check: exactly 16 events with an empty queue do
not requeue, while a seventeenth event remains pending. Network requeue is
conservative, so exactly 16 frames may cause one empty follow-up pass.

This does not complete v0.2. Input producer/USB polling, redraw/damage work, total
service time, and sustained-load EL0 progress proof remain open in issue #43 and
`RISK-017`.

The validated input-budget kernel is 107802 / 108000 bytes, leaving 198 bytes.
Compact or redesign before adding more runtime state; do not raise the ceiling to
avoid architectural work.

The virtio-net path exposes no trustworthy device-drop or ring-overflow counter.
Do not infer “no drops” from consumed-frame counts.

Issue #2 is the **v0.6 useful desktop applications** milestone. Do not treat it as
v1.1 work or bypass v0.3 storage/VFS, v0.4 real FAT, and v0.5 shared
runtime/widget foundations.

The release claim applies only to QEMU `virt`. Raspberry Pi 4 remains
build-verified, fail-closed scaffolding without physical evidence.

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

`CURRENT_STATE.md` is operational truth. `ROADMAP.md` is future intent and is not
evidence that a feature exists.

## Architectural facts

- EL0 processes are preemptive.
- EL1 helper threads are cooperative.
- Timer-originated work runs in one post-EOI bottom half before dispatch and
  `eret`.
- Pending readiness has periodic, input, and network bits.
- EOI is not exception return: EL0 remains paused and the exception frame remains
  on the EL1 stack.
- `irq_disable()` and `irq_enable()` are nested state-preserving helpers.
- The timer callback is bounded.
- Active input consumption is capped at 16 events and only requeues when the queue
  still contains work.
- Active network RX is capped at 16 valid frames and conservatively requeues at
  the cap.
- Input producer/USB polling, redraw/damage, and total generic-counter time remain
  unbounded.
- Cooperative console input and network polling outside the active runtime
  service remain unbudgeted.
- Input overflow is counted but not prevented.
- Device-level RX loss is not measurable.
- Timing uses `CNTPCT_EL0`; one timer interval remains an observation threshold,
  not the final global deadline.
- Runtime telemetry is kernel-internal and not a syscall ABI.
- Pending state and telemetry assume one CPU and one consumer; they are not
  SMP-safe synchronization.
- User-copy permission validation exists, but final copies are not
  fault-recoverable.
- Every process still carries kernel mappings in TTBR0; TTBR1, ASIDs, and scoped
  invalidation remain future hardening.
- FAT32 is root-directory 8.3 only.
- QEMU is the only verified runtime platform.

## Verification workflow

Use the smallest relevant test while developing, then run:

```sh
bash tools/verify.sh
```

The shorter hosted workflow executes the deferred-runtime regression with strict
`pipefail` and uploads `runtime-service-test-log`.

The baseline includes:

- QEMU and RPi4 build contracts;
- `.data == 0` and the 108000-byte kernel limit;
- native kernel, VFS, filesystem, GUI, parser, driver, and ABI tests;
- runtime EOI/coalescing/timing/requeue/reset checks;
- indexed input, network, USB-poll, redraw, damage, and full-redraw metrics;
- deterministic 16-event input and 16-frame network budget tests;
- input queue depth/high-water/overflow regression;
- parent/wait, process-local FDs, user-copy, KLI1, and stack contracts;
- FAT32, focus, framebuffer, USB, network, and visible-target QEMU gates.

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
- Preserve independent input and network pending bits.
- Preserve input’s real-leftover requeue rule and network’s conservative requeue
  rule.
- Count completed work or actual device operations, not generic polling attempts.
- Do not infer absence of loss from consumption counters.
- The next runtime cuts should first compact state, then bound USB/device polling,
  redraw/damage, and total generic-counter time.
- Every exhausted class must remain pending and increment an explicit counter.
- Add deterministic and sustained-load EL0 progress tests.
- Preserve `.data == 0`, W^X, kernel-size, and stack limits.
- Do not raise the 108000-byte ceiling to make runtime work fit.

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
- Use exact commits and workflow run IDs for promoted baselines.
- Separate IMPLEMENTED, HOST-, BUILD-, QEMU-, CI-, MANUAL-, and physical-hardware
  evidence.
- A validated PR tree is not automatically a separately tested merge commit.
- Synchronize README, current state, roadmap, risks, runtime contract, and this
  guide when release state changes.
- Do not add archive, duplicate status, or obsolete pre-reset references.
