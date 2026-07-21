# Agent Guide

This file is the live entry point for agents working in this repository.

## Current project classification

ArmoniOS is a **v0.1 QEMU desktop baseline** and an active **v0.2
cleanup/runtime-hardening candidate**. It is a real freestanding AArch64
operating system, not a Linux application or distribution.

Runtime measurement Phase 1B is complete for the work classes currently
observable on QEMU. The kernel records:

- runtime requests, coalescing, requeue, non-empty and empty passes;
- last, maximum, and cumulative generic-counter duration;
- one-timer-interval overruns;
- input events produced and consumed;
- shared input queue depth, lifetime high-water, and overflow;
- USB HID polling operations that reach xHCI;
- valid virtio-net RX frames consumed;
- successful redraw submissions;
- merged partial-damage rectangles and full-redraw fallbacks.

Phase 2 has begun. Network readiness is independent from periodic readiness,
and post-EOI virtio-net receive is capped at 16 valid frames per service pass.
When the cap is reached, `RUNTIME_WORK_NETWORK` is conservatively republished and
a network-budget exhaustion is counted. Exactly 16 frames may therefore cause
one empty follow-up network pass.

This does not complete v0.2. Input consumption, USB polling, redraw/damage work,
and total service time remain unbounded; no sustained-load QEMU test proves EL0
progress. Track this work in issue #43 and `RISK-017`.

The virtio-net path exposes no trustworthy device-drop or ring-overflow counter.
Do not invent one or infer “no drops” from consumed-frame counts.

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
- Timer-originated input, GUI, USB, and network work runs in one post-EOI bottom
  half.
- Pending readiness currently has separate periodic and network bits.
- EOI is not exception return: EL0 remains paused and the vector's IRQ-mask state
  is preserved while the pass executes.
- `irq_disable()` and `irq_enable()` are nested state-preserving helpers; do not
  replace them with unconditional DAIF operations.
- The timer callback is bounded. Network RX is count-bounded; the complete
  exception path is not yet globally bounded.
- Timing uses `CNTPCT_EL0`; `CNTFRQ_EL0` supplies conversion. One timer interval
  remains an observation threshold, not the final global budget.
- Work-class reports are accepted only during the active runtime pass, excluding
  cooperative console-thread work.
- Cooperative network polling outside the active runtime service remains
  unbudgeted and must not be confused with the post-EOI network cap.
- Input overflow is counted but not prevented.
- Network frames consumed are measurable and capped in the post-EOI network
  phase; device-level RX loss is not measurable.
- Redraw submissions, partial-damage items, and full redraws are separate metrics.
- Runtime telemetry is kernel-internal. Do not copy its internal structure into a
  syscall ABI.
- Pending state and telemetry assume one CPU and one consumer; they are not
  SMP-safe synchronization.
- User-copy permission validation exists, but final copies are not
  fault-recoverable.
- Every process still carries kernel mappings in TTBR0; TTBR1, ASIDs, and scoped
  TLB invalidation remain future hardening.
- FAT32 is root-directory 8.3 only.
- QEMU is the only verified runtime platform.

## Verification workflow

Use the smallest relevant test while developing, then run:

```sh
bash tools/verify.sh
```

The shorter hosted workflow also executes the deferred-runtime regression with
strict pipeline failure handling and uploads `runtime-service-test-log` for
future diagnosis.

The baseline includes:

- QEMU and RPi4 build contracts;
- `.data == 0` and the 108000-byte kernel limit;
- RPi4 EMMC2 diagnostic, MBR, and block-view gates;
- native kernel, VFS, filesystem, GUI, parser, driver, and ABI tests;
- runtime EOI/coalescing/timing/requeue/reset checks;
- indexed input, network, USB-poll, redraw, damage, and full-redraw metrics;
- deterministic 16-frame network cap and conservative continuation checks;
- direct partial/full redraw-helper regression and static wiring checks;
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
- Count completed work or actual device operations, not generic polling attempts.
- Do not infer absence of loss from consumption counters.
- Preserve the independent network pending bit and its conservative requeue rule.
- The next runtime cuts should bound input consumption, USB polling, redraw/damage
  work, and total generic-counter time.
- Every exhausted class must remain pending and increment an explicit counter.
- Add deterministic and sustained-load EL0 progress tests.
- Preserve `.data == 0`, W^X, kernel-size, and stack limits.
- Do not raise the 108000-byte ceiling to make runtime work fit; compact or
  redesign first.

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
- Do not add archive, historical handoff, duplicate status, or obsolete pre-reset
  references.
