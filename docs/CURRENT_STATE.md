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
reproducible. The planned automated runtime-hardening evidence for v0.2 has now
landed.

Runtime hardening includes:

- aggregate and per-class generic-counter telemetry;
- count bounds for virtio-input, USB HID visits, shared input consumption,
  partial compositor damage, and post-EOI network RX;
- an enforced service-wide generic-counter deadline at safe checkpoints;
- conservative republishing of original work after deadline exhaustion;
- strict routing that prevents virtio-net receive outside the NETWORK phase;
- deterministic host regressions for expiry and native continuation;
- forced-expiry QEMU stress with repeated EL0 heartbeats;
- natural-deadline QEMU RX saturation with input and redraw activity.

The natural RX run consumed 29,234 frames, reached the 16-frame cap in 1,827
passes, requeued all 1,827 capped passes, and still recorded 38,912 EL0 yields.
Its maximum complete service pass was 385,763 of 625,000 configured ticks
(61.7%), with zero natural deadline overruns and zero observable input overflow.

The remaining v0.2 work is release disposition rather than missing runtime
instrumentation: accept or defer device-level RX-drop telemetry, decide whether
the one-operation full-redraw boundary is acceptable, perform a dated visible
pass on the final runtime boundary, and investigate the intermittent VMM panic
tracked in issue #63.

ArmoniOS is accurately described as:

> A compact AArch64 QEMU desktop alpha with freestanding EL0 applications, a
> kernel compositor, a narrow writable FAT32 workflow, broad automated
> regression coverage, count-bounded runtime classes, a cooperative service-wide
> deadline, forced-expiry heartbeat evidence, and natural RX saturation evidence.

It is not a production OS, general FAT implementation, POSIX system, or verified
Raspberry Pi operating system.

## Audit metadata

- **Audit date:** 2026-07-22
- **Primary verified platform:** QEMU `virt`, Cortex-A72 CPU model
- **Runtime tracking issue:** #43
- **Intermittent VMM investigation:** #63
- **Global deadline:** PR #60, merged as
  `1e51e81867e3518d9efd56a7a9ec67d40c7caf34`
- **Forced-expiry heartbeat:** PR #61, merged as
  `172496d855e523c080225d8133a4aa0004835bda`
- **Natural RX saturation candidate:** PR #62
- **Validated head:**
  `eac4ff990baddbf83406567b4a20e58bcae6600d`
- **Hosted validation:**
  - `Verify ArmoniOS` run `29896102906` (#290): success
  - `CI - Tests` run `29896102904` (#430): success
- **Production loadable QEMU kernel size:** 107918 bytes
- **Kernel size limit:** 108000 bytes
- **Remaining production margin:** 82 bytes

The stress images are built separately with test-only macros. The production
image, ABI, `.data == 0`, and fixed size ceiling remain the release controls.

## Release phases

| Phase | State | Real interpretation |
|---|---|---|
| v0.1 QEMU baseline | COMPLETE | Boot, desktop, narrow FAT workflow, deterministic QEMU gates, CI, and dated manual evidence exist. |
| v0.2 cleanup/hardening | FINAL CANDIDATE | Count bounds, global deadline, forced-expiry heartbeat, strict network routing, and natural RX saturation landed. Final visible evidence and residual-risk disposition remain. |
| v0.3 storage/VFS platform | NEXT AFTER v0.2 | No common path resolver, rich block metadata, or structured filesystem ABI. |
| v0.4 real FAT | PLANNED | Current FAT remains root-only 8.3 FAT32. |
| v0.5 userland runtime/widgets | PLANNED | No reusable heap, dynamic containers, or shared widget toolkit. |
| v0.6 useful applications | PARTIAL DEMOS ONLY | Seven apps run, but issue #2's daily-use workflows are incomplete. |
| v0.7 ext2 | PLANNED | No ext2 implementation. |
| v0.8 polish | EARLY PARTIAL | Basic window/panel behavior exists; sustained visible-session evidence does not. |
| v0.9 beta | NOT STARTED | No ABI freeze, fuzz campaign, reboot-persistence gate, or beta record. |
| v1.0 | NOT READY | Storage, apps, ext2, persistence, remaining hardening, and final evidence are incomplete. |

## Verification record

| Check | Evidence class | Result and scope |
|---|---|---|
| QEMU build and size | BUILD-VERIFIED | `.data == 0`; production kernel 107918 bytes under the 108000-byte ceiling. |
| RPi4 build/probe gates | BUILD/HOST-VERIFIED | Normal and diagnostic images build; unsupported normal capabilities fail closed. |
| Native host suite | HOST-VERIFIED | Kernel, memory, VFS, FAT32, GUI, parser, driver, and ABI tests pass. |
| Runtime service regression | HOST-VERIFIED | Timing, EOI order, coalescing, reset, metrics, count caps, redraw batches, strict network routing, and deadline behavior pass. |
| Global deadline | HOST-VERIFIED | Expiry skips later optional work, stops network at checkpoints, counts exhaustion, and republishes readiness. |
| Forced-expiry runtime stress | QEMU-VERIFIED, test image | 509 EL0 heartbeats and 311 deadline republications; real input, redraw, network, and DHCP markers; zero overflow and panic. |
| Natural RX saturation | QEMU-VERIFIED, test image | 29,234 consumed frames; 1,827 capped/requeued passes; 38,912 EL0 yields; max 385,763/625,000 ticks; zero natural overruns, overflow, and panic. |
| Virtio-input producer | HOST-VERIFIED | Ten used descriptors on an eight-entry ring complete as 8 + 2 with ten queued events. |
| USB HID producer | HOST-VERIFIED | A malformed count of 255 still visits exactly the fixed four HID slots. |
| Partial redraw | HOST-VERIFIED | Twenty rectangles complete as 8 + 8 + 4; failed submission preserves all damage. |
| Input queue telemetry | HOST-VERIFIED | Zero state, 64-entry high-water, full-queue overflow, draining, and reset. |
| Process/VFS/user-copy/KLI1 | HOST-VERIFIED | Parent/wait, local FDs, permission-aware copy, and mutable-storage contracts. |
| Stack check | HOST-VERIFIED | Editor maximum remains 368 bytes against 3072. |
| FAT32 smoke | QEMU-VERIFIED with open flake investigation | Current head passes. One earlier attempt panicked in `next_table()` and is tracked in issue #63. |
| User-copy/focus | QEMU-VERIFIED | Invalid output rejection and app focus transitions. |
| Framebuffer/USB/network | QEMU-VERIFIED | Window/panel, xHCI/two HID, and DHCP markers. |
| Visible FAT + GPU wiring | QEMU-VERIFIED | FAT32, display, and panel readiness in one boot. |
| Visible desktop workflow | MANUAL-VERIFIED, dated | Rocco verified create/edit/save/rename/reopen/delete on 2026-07-17; a final post-runtime-change pass remains required. |
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

Network polling and descriptor receive return without consuming anything outside
the active NETWORK phase. The old cooperative console-thread poll therefore no
longer bypasses the post-EOI count and time contracts.

EOI does not leave the exception. During the pass execution remains in EL1, the
288-byte exception frame remains on the EL1 stack, nested IRQ helpers restore the
vector's prior masked state, and EL0 remains paused.

The deadline is cooperative. It stops work only at explicit safe checkpoints and
cannot asynchronously interrupt a driver or full redraw already executing.

## Natural RX stress evidence

`tools/qemu_runtime_net_stress_test.sh` builds a separate test image without
shortening the production deadline. It completes DHCP, injects sustained UDP via
QEMU `hostfwd`, injects USB keyboard events, and keeps panel redraw work active.

Final recorded summary:

```text
EL0 yields:                       38,912
input events consumed:                 16
redraw submissions:                   738
virtio-net frames consumed:        29,234
maximum frames/pass:                   16
network cap exhaustions:             1,827
runtime requeues:                    1,827
natural deadline overruns:               0
maximum duration:                  385,763 ticks
configured budget:                 625,000 ticks
maximum / budget:                    61.7%
input overflow:                          0
kernel panic:                            0
```

The test proves software-visible continuation and EL0 progress under repeated RX
saturation. It cannot prove that every host-submitted packet reached a guest
receive descriptor because the current driver exposes no device-drop counter.

## Important fixed limits

| Area | Current limit |
|---|---|
| Kernel image | 108000 bytes; production image 107918 bytes |
| PMM | 128 MiB managed |
| Processes | 16 slots; eight user regions each |
| VFS | 24 nodes, four mounts, eight FDs/process, 64-byte paths |
| FAT32 | Root 8.3 files only |
| GUI | 16 windows; 32 queued events/window; 32 damage rectangles; eight partial rectangles/redraw |
| Input queue | 64 events; overflow counted but not prevented |
| Virtio input | Ring up to 16; one negotiated ring-length drained/call |
| USB | Four direct HID devices; no hubs; four visits/call |
| Network RX | 16 descriptors; valid RX capped at 16/NETWORK pass; outside-phase receive suppressed; device drops unavailable |
| Editor | 512-byte buffer; caret-line viewport |
| Files | `/fat` only; eight displayed root entries |
| Network API | No sockets, TCP, DNS API, or HTTP |
| User copy | Permission-aware but not fault-recoverable |
| RPi4 | Build/host scaffolding only |

## Risks by release impact

### Blocks formal v0.2

- **RISK-017:** automated runtime proof is complete, but the device-drop boundary,
  full-redraw residual, and final visible pass need explicit disposition.
- **RISK-018 / issue #63:** one intermittent EL1 VMM panic occurred in the FAT32
  smoke path and needs investigation or a justified release disposition.
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

1. Merge the strict-routing and natural RX evidence cut after final CI.
2. Add a repeated FAT32/QEMU diagnostic gate for issue #63.
3. Decide or document acceptance of device-drop and full-redraw residuals.
4. Record a dated visible pass, close or explicitly accept remaining risks, and
   tag v0.2.
5. Begin v0.3 storage/VFS work.
