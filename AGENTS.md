# Agent Guide

This file is the live entry point for agents working in this repository.

## Current project classification

ArmoniOS is a **v0.1 QEMU desktop baseline** and an active **v0.2
cleanup/runtime-hardening candidate**. It is a real freestanding AArch64
operating system, not a Linux application or distribution.

Runtime measurement Phase 1B is complete. Current enforced post-EOI count
bounds are:

- virtio-input producer: at most 16 used descriptors per call;
- USB HID producer: at most four registered device visits per call;
- shared input consumer: at most 16 queued events per pass;
- partial compositor damage: at most eight rectangles per successful redraw;
- virtio-net RX: at most 16 valid frames per pass.

Virtio-input continuation stays in the used ring. Remaining partial damage stays
in-order in the compositor damage list and `dirty` remains set. Failed GPU
submissions consume no damage. Input readiness and network readiness have
independent pending bits. Input requeues only when queue work remains; network
conservatively requeues at its cap.

This does not complete v0.2. Full redraw elapsed time and total runtime-service
time have no enforced generic-counter deadline, cooperative network polling
outside the bottom half is outside the class guarantee, and no sustained-load
QEMU test proves EL0 progress. Track this work in issue #43 and `RISK-017`.

Latest redraw-bound evidence:

- PR #58 merge `fe4f2a622f5633e55b0eddb2f8f6767453a9ddca`;
- validated head `8b86a8c24f25af0937f1df2e983c1c7c4f489b7d`;
- `Verify ArmoniOS` `29863653280`: success;
- `CI - Tests` `29863653209`: success;
- loadable kernel 107982 / 108000 bytes; margin 18 bytes.

Another compaction is mandatory before adding production deadline code. Do not
raise the kernel ceiling to bypass that work.

Issue #2 is the **v0.6 useful desktop applications** milestone. Do not treat it
as v1.1 work or bypass v0.3 storage/VFS, v0.4 real FAT, and v0.5 shared
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

`CURRENT_STATE.md` is operational truth. `ROADMAP.md` describes future intent
and must never be used as evidence that a feature exists.

## Architectural facts

- EL0 processes are preemptive.
- EL1 helper threads are cooperative.
- Timer-originated input, GUI, USB, and network work runs in one post-EOI bottom
  half before process dispatch and `eret`.
- EOI is not exception return: EL0 remains paused and the vector's IRQ-mask
  state is preserved while the pass executes.
- `irq_disable()` and `irq_enable()` are nested state-preserving helpers.
- Pending readiness has periodic, input, and network bits.
- The timer callback is bounded. Input producer/consumer, partial-damage, and
  network RX counts are bounded; complete exception latency is not.
- Timing uses `CNTPCT_EL0`; one timer interval is an observation threshold, not
  the final enforced deadline.
- Work-class reports are accepted only during the active runtime pass.
- Cooperative input/network polling outside the active runtime service remains
  outside post-EOI guarantees.
- Virtio-input reports successfully queued events; later descriptors stay in the
  ring for a subsequent periodic pass.
- USB HID polling is bounded by the fixed four-device array.
- Partial damage is processed in eight-rectangle batches. A failed board redraw
  consumes nothing. Full redraw is one operation.
- Input overflow is counted but not prevented.
- Network frames consumed are measurable; device-level RX loss is not.
- Runtime telemetry is kernel-internal and must not be copied into a syscall ABI.
- Pending state and telemetry assume one CPU and one consumer; they are not
  SMP-safe synchronization.
- User-copy validation exists, but final copies are not fault-recoverable.
- Every process still carries kernel mappings in TTBR0; TTBR1, ASIDs, and scoped
  TLB invalidation remain future hardening.
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
- runtime EOI/coalescing/timing/requeue/reset checks;
- deterministic input consumer, virtio-input producer, USB producer, partial
  redraw, and network bounds;
- indexed input, network, USB-poll, redraw, damage, full-redraw, and redraw-
  exhaustion metrics;
- input queue depth/high-water/overflow regression;
- parent/wait and process-local FD isolation;
- user-copy and KLI1 contracts;
- userland stack checking;
- FAT32 QEMU smoke;
- usercopy and focus QEMU regressions;
- framebuffer, USB, network, and visible-target FAT+GPU markers.

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
- Treat timing and counts as measurement unless an explicit stop rule exists.
- Count completed work or actual device operations, not generic attempts.
- Do not infer absence of loss from consumption counters.
- Preserve independent input/network readiness and continuation rules.
- Preserve virtio-input ring continuation; do not discard descriptors at a cap.
- Never scan beyond `USB_HID_MAX_DEVICES`, even if state is malformed.
- Preserve partial-damage ordering and consume damage only after redraw success.
- The next production cut must compact first, then enforce a service-wide
  generic-counter deadline while preserving unfinished work.
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
- Do not add archive, historical handoff, duplicate status, or obsolete pre-reset
  references.
