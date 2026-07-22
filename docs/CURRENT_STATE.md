# Current State

> Operational source of truth for ArmoniOS.
>
> Evidence terminology: `DOCUMENTATION_POLICY.md`  
> Active risks: `TECHNICAL_RISKS.md`  
> Future milestones: `ROADMAP.md`

## Executive classification

ArmoniOS is a **v0.1 QEMU desktop baseline** and an active **v0.2
cleanup/runtime-hardening candidate**.

The QEMU `virt` kernel, graphical desktop, narrow writable FAT32 workflow,
freestanding EL0 applications, and automated verification matrix are real and
reproducible. Most original v0.2 cleanup goals have landed.

Runtime hardening now includes:

- aggregate and per-class generic-counter telemetry;
- count bounds for virtio-input, USB HID visits, shared input consumption,
  partial compositor damage, and post-EOI network RX;
- an enforced service-wide generic-counter deadline at safe checkpoints;
- conservative republishing of the original work snapshot after deadline
  exhaustion;
- deterministic tests for expiry before redraw/network, expiry inside the network
  loop, and completed operations that cross the deadline.

The remaining v0.2 blocker is sustained-load evidence. A full redraw is still one
non-preemptible operation and can cross the nominal deadline before the next
checkpoint. Cooperative network polling outside the bottom half is outside the
post-EOI guarantee, and no combined-load QEMU heartbeat test yet proves repeated
EL0 progress and explicit loss accounting.

ArmoniOS is accurately described as:

> A compact AArch64 QEMU desktop alpha with freestanding EL0 applications, a
> kernel compositor, a narrow writable FAT32 workflow, broad automated
> regression coverage, count-bounded runtime classes, and a cooperative
> service-wide deadline in the post-EOI bottom half.

It is not a production OS, general FAT implementation, POSIX system, or verified
Raspberry Pi operating system.

## Audit metadata

- **Audit date:** 2026-07-22
- **Primary verified platform:** QEMU `virt`, Cortex-A72 CPU model
- **Tracking issue:** #43, v0.2 measure and bound deferred runtime service
- **Deadline implementation PR:** #60
- **Validated implementation head:**
  `af07d6de6b4b00fb77b37da1efa51b561aa73d9c`
- **Hosted validation:**
  - `Verify ArmoniOS` run `29891251044`: success
  - `CI - Tests` run `29891251026`: success
- **Loadable QEMU kernel size:** 107930 bytes
- **Kernel size limit:** 108000 bytes
- **Remaining margin:** 70 bytes

Those runs validate QEMU and RPi4 builds, `.data == 0`, the fixed size ceiling,
host tests, runtime deadline regression, stack limits, FAT32 smoke, user-copy,
focus, framebuffer, USB, network, and the existing subsystem gates.

## Release phases

| Phase | State | Real interpretation |
|---|---|---|
| v0.1 QEMU baseline | COMPLETE | Boot, desktop, narrow FAT workflow, deterministic QEMU gates, CI, and dated manual evidence exist. |
| v0.2 cleanup/hardening | IN PROGRESS / CANDIDATE | Count bounds and global deadline landed; sustained-load heartbeat and final promotion evidence remain. |
| v0.3 storage/VFS platform | NEXT AFTER v0.2 | No common path resolver, rich block metadata, or structured filesystem ABI. |
| v0.4 real FAT | PLANNED | Current FAT remains root-only 8.3 FAT32. |
| v0.5 userland runtime/widgets | PLANNED | No reusable heap, dynamic containers, or shared widget toolkit. |
| v0.6 useful applications | PARTIAL DEMOS ONLY | Seven apps run, but issue #2's daily-use workflows are incomplete. |
| v0.7 ext2 | PLANNED | No ext2 implementation. |
| v0.8 polish | EARLY PARTIAL | Basic window/panel behavior exists; sustained visible-session evidence does not. |
| v0.9 beta | NOT STARTED | No ABI freeze, fuzz campaign, reboot-persistence gate, or beta record. |
| v1.0 | NOT READY | Storage, apps, ext2, persistence, remaining runtime proof, and final evidence are incomplete. |

## Verification record

| Check | Evidence class | Result and scope |
|---|---|---|
| QEMU build and size | BUILD-VERIFIED | `.data == 0`; loadable kernel 107930 bytes under the 108000-byte ceiling. |
| RPi4 build/probe gates | BUILD/HOST-VERIFIED | Normal and diagnostic images build; unsupported normal capabilities fail closed. |
| Native host suite | HOST-VERIFIED | Kernel, memory, VFS, FAT32, GUI, parser, driver, and ABI tests pass. |
| Runtime service regression | HOST-VERIFIED | Timing, EOI order, coalescing, reset, class metrics, count caps, redraw batches, and global deadline behavior pass. |
| Global deadline | HOST-VERIFIED | Expiry skips later optional work, stops the network loop at checkpoints, counts exhaustion, and republishes original readiness. |
| Virtio-input producer | HOST-VERIFIED | Ten used descriptors on an eight-entry ring complete as 8 + 2 with ten queued events. |
| USB HID producer | HOST-VERIFIED | A malformed count of 255 still visits exactly the fixed four HID slots. |
| Partial redraw | HOST-VERIFIED | Twenty rectangles complete as 8 + 8 + 4; failed GPU submission preserves all damage; full redraw clears once. |
| Input queue telemetry | HOST-VERIFIED | Zero state, 64-entry high-water, full-queue overflow, draining, and reset. |
| Process/VFS/user-copy/KLI1 | HOST-VERIFIED | Parent/wait, local FDs, permission-aware copy, and mutable-storage contracts. |
| Stack check | HOST-VERIFIED | Editor maximum remains 368 bytes against 3072. |
| FAT32 smoke | QEMU-VERIFIED | Storage initialization and FAT application markers. |
| User-copy/focus | QEMU-VERIFIED | Invalid output rejection and six app focus transitions. |
| Framebuffer/USB/network | QEMU-VERIFIED | Window/panel, xHCI/two HID, and DHCP markers. |
| Visible FAT + GPU wiring | QEMU-VERIFIED | FAT32, display, and panel readiness in one boot. |
| Visible desktop workflow | MANUAL-VERIFIED, dated | Rocco verified create/edit/save/rename/reopen/delete on 2026-07-17. |
| Physical Raspberry Pi | UNVERIFIED | No repeatable physical boot, timer, storage, framebuffer, or input evidence. |

## Runtime execution model

EL0 processes are preemptive. EL1 helper threads are cooperative. The deferred
runtime service is a third execution mode:

```text
timer callback
  -> fixed account/rearm/publish PERIODIC | INPUT | NETWORK
  -> board_irq_end()
  -> runtime pass with deadline = start + one timer interval
       -> periodic/input phase
            -> virtio-input <= min(negotiated ring, 16) descriptors
            -> USB HID <= 4 registered device visits
            -> input queue <= 16 events when INPUT is pending
            -> partial damage <= 8 rectangles/successful redraw
            -> full redraw = one non-preemptible operation
       -> network phase only while deadline remains
            -> <= 16 valid RX frames
       -> on expiry: count once, republish original work, skip later work
  -> process dispatch
  -> eret
```

EOI does not leave the exception. During the pass execution remains in EL1, the
288-byte exception frame remains on the EL1 stack, nested IRQ helpers restore the
vector's prior masked state, and EL0 remains paused.

The deadline is cooperative. It stops work only at explicit safe checkpoints and
cannot asynchronously interrupt a driver or full redraw already executing.

## Runtime telemetry and budgets

The kernel-internal snapshot records requests, coalescing, non-empty and empty
passes, requeues, last/max/total `CNTPCT_EL0` duration, global deadline
exhaustion, input production/consumption, queue pressure/overflow, USB polls,
network frames, redraw/damage shape, class-budget exhaustion, pending work, and
last-consumed work.

Current enforced bounds:

```text
service deadline       = CNTFRQ_EL0 / timer_hz (about 10 ms at 100 Hz)
virtio-input producer <= min(queue_size, 16) used descriptors/call
USB HID producer      <= 4 registered device visits/call
input consumer        <= 16 queued events/post-EOI pass
partial redraw        <= 8 damage rectangles/successful redraw
network RX            <= 16 valid frames/post-EOI pass
```

At deadline exhaustion the original work snapshot is conservatively ORed back
into `pending_work`. Native continuation remains in input queues, virtio rings,
and the compositor damage list. Re-executing a completed class once is accepted
in preference to silently losing readiness.

The current virtio-net interface has no trustworthy device-drop or RX-ring
overflow counter. Consumed frames are not proof of loss-free delivery.

No syscall exposes the telemetry layout. Pending state and counters assume one
CPU and one consumer; they are not SMP-safe synchronization.

## Important fixed limits

| Area | Current limit |
|---|---|
| Kernel image | 108000 bytes; current validated image 107930 bytes |
| PMM | 128 MiB managed |
| Processes | 16 slots; eight user regions each |
| VFS | 24 nodes, four mounts, eight FDs/process, 64-byte paths |
| FAT32 | Root 8.3 files only |
| GUI | 16 windows; 32 queued events/window; 32 damage rectangles; eight partial rectangles/redraw |
| Input queue | 64 events; overflow counted but not prevented |
| Virtio input | Ring up to 16; one negotiated ring-length drained/call |
| USB | Four direct HID devices; no hubs; four visits/call |
| Network RX | 16 descriptors; post-EOI valid RX capped at 16/pass; device drops unavailable |
| Editor | 512-byte buffer; caret-line viewport |
| Files | `/fat` only; eight displayed root entries |
| Network API | No sockets, TCP, DNS API, or HTTP |
| User copy | Permission-aware but not fault-recoverable |
| RPi4 | Build/host scaffolding only |

## Risks by release impact

### Blocks formal v0.2

- **RISK-017:** deterministic deadline behavior exists, but sustained combined-load
  QEMU evidence is still missing.
- A full redraw may cross the nominal deadline before the next checkpoint.
- Cooperative network polling outside the service remains unbudgeted.
- No QEMU heartbeat yet proves repeated EL0 progress with input, redraw, and
  network pressure while deadline exhaustion occurs.
- No final v0.2 tag/evidence record exists.

### Required before v1.0

- **RISK-013:** storage/VFS is too narrow.
- **RISK-014:** applications are incomplete daily tools.
- No ext2, combined reboot-persistence gate, or 30-minute stable visible session.

### Ongoing hardening and hardware

- **RISK-015:** copyin/copyout is not fault-contained.
- TTBR1, ASIDs, and scoped TLB invalidation are absent.
- **RISK-007:** no physical Raspberry Pi evidence.

## Promotion gates

```sh
bash tools/verify.sh
make qemu-fb-visible   # separate manual evidence
```

## Next sequence

1. Add sustained-load QEMU pressure and an EL0 heartbeat/progress probe.
2. Prove deadline exhaustion does not stop repeated EL0 dispatch.
3. Prove unfinished work remains pending/native and observable overflow is counted.
4. Record a dated visible pass, close or explicitly accept RISK-017, and tag v0.2.
5. Begin v0.3 storage/VFS work.
