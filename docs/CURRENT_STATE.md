# Current State

> Canonical operational source of truth for ArmoniOS.
>
> Evidence terminology: `DOCUMENTATION_POLICY.md`  
> Active risks: `TECHNICAL_RISKS.md`  
> Milestone order: `ROADMAP.md`  
> Development workflow: `DEVELOPMENT_GUIDE.md`

## Executive classification

ArmoniOS is a real freestanding AArch64 operating system with a verified **v0.1
QEMU desktop baseline**.

The v0.2 cleanup/runtime-hardening implementation and planned automated runtime
evidence have landed. Formal v0.2 promotion is still blocked by explicit
residual-risk decisions, the intermittent VMM investigation, a final dated visible
workflow, and the release record.

Accurate wording:

> ArmoniOS is a compact AArch64 QEMU desktop alpha with a verified v0.1 baseline
> and a v0.2 promotion candidate containing count-bounded post-EOI work, a
> cooperative service-wide deadline, strict network-phase routing, and automated
> forced-expiry and natural RX stress evidence.

Do not call the current tree a release candidate. The documentation policy
requires every affected P1 risk to be closed or explicitly accepted and the
required manual evidence to be recorded first.

ArmoniOS is not a production OS, POSIX environment, general FAT implementation,
complete daily-use desktop, or verified Raspberry Pi operating system.

## Audit and evidence identity

- **Audit date:** 2026-07-22
- **Primary runtime:** QEMU `virt`, Cortex-A72 CPU model, 128 MiB RAM
- **Audited current `main`:**
  `ce950cc756949fec6508256fcd6ca450b50bb071`
- **Current-main runtime replay commit:**
  `d5c104a0badc3a2d553516159b2b745737dd252f`
- **Original GitHub PR #62 merge metadata:**
  `7ea3d309047659c8bbe9c601c3d98217bcaafb02`
- **Evidence-producing PR #62 implementation head:**
  `eac4ff990baddbf83406567b4a20e58bcae6600d`
- **Final PR #62 head:**
  `04f65776d1bbe07545113652342c32f2448bfc7b`
- **Final PR #62 validation:**
  - `Verify ArmoniOS` `29896952424` (#295): success
  - `CI - Tests` `29896952435` (#435): success
- **Production QEMU kernel:** 107918 / 108000 bytes; margin 82 bytes
- **Runtime tracking:** issue #43 / `RISK-017`
- **VMM investigation:** issue #63 / `RISK-018`
- **Documentation synchronization:** draft PR #65, branch
  `agent/documentation-working-base`

The repository history was replayed while this documentation audit was in
progress. The current `main` commit IDs therefore differ from the original PR
merge IDs even where the same runtime changes are represented. Current-tree
claims must use current `main` plus final-tree workflow evidence; original PR IDs
remain provenance for the stress measurements and review record.

PR #65 is a documentation-only synchronization. It aligns claims and operating
instructions with already-promoted implementation; it does not upgrade runtime or
hardware evidence.

The runtime stress images are separate test-only builds. The production image
remains subject to `.data == 0`, the 108000-byte ceiling, the public ABI, KLI1,
and stack gates.

## Release phases

| Phase | State | Meaning |
|---|---|---|
| v0.1 QEMU baseline | COMPLETE | Boot, desktop, narrow FAT workflow, deterministic gates, CI, and dated visible evidence exist. |
| v0.2 cleanup/hardening | PROMOTION CANDIDATE | Runtime implementation/evidence landed. Risk disposition, issue #63, final visible evidence, and release record remain. |
| v0.3 storage/VFS platform | NEXT AFTER v0.2 | No common path resolver, rich block descriptor, or structured filesystem ABI. |
| v0.4 real FAT | PLANNED | Current FAT remains root-only FAT32 8.3. |
| v0.5 userland runtime/widgets | PLANNED | No shared heap-backed containers or widget toolkit. |
| v0.6 useful applications | PARTIAL DEMOS ONLY | Seven apps run; issue #2's daily workflow is incomplete. |
| v0.7 ext2 | PLANNED | No ext2 implementation. |
| v0.8 polish | EARLY PARTIAL | Basic windows/panel exist; long visible-session evidence does not. |
| v0.9 beta | NOT STARTED | No ABI freeze, fuzz campaign, persistence gate, or beta record. |
| v1.0 | NOT READY | Storage, apps, ext2, persistence, hardening, and release evidence remain incomplete. |

## Implemented system

The current QEMU tree includes:

- AArch64 EL1 entry with DTB handoff;
- fixed 128 MiB PMM, heap, four-level 4 KiB page tables, MMU, and kernel W^X;
- preemptive freestanding EL0 processes;
- private image, stack, anonymous mappings, parent/wait, kill, exit, zombie, and
  cleanup behavior;
- a narrow append-only non-POSIX syscall ABI;
- permission-aware user-pointer validation and kernel-owned syscall payloads;
- process-local VFS descriptors;
- bootfs, tmpfs, and a writable root-only FAT32 8.3 bridge;
- kernel compositor/window manager with backing buffers, damage, focus, dragging,
  minimize/restore, and event queues;
- Panel, Shell, Editor, Files, Monitor, Control, and Clock KLI1 applications;
- QEMU virtio block, GPU, input, and network;
- PCI/xHCI with directly attached keyboard and mouse;
- Ethernet, ARP, IPv4, UDP, and DHCP sufficient for a QEMU lease;
- a measured post-EOI service with count budgets, strict routing, and a
  cooperative deadline;
- deterministic host, build, QEMU, stress, size, stack, ABI, storage, GUI, USB,
  network, and board-contract gates.

## Runtime model

```text
EL0 process
  -> timer IRQ, 288-byte EL1 exception frame
  -> fixed timer callback
       -> account
       -> rearm
       -> publish PERIODIC | INPUT | NETWORK
       -> update scheduler counters
  -> board_irq_end()
  -> post-EOI runtime pass
       -> deadline = start + one nominal timer interval
       -> PERIODIC / INPUT phase
       -> NETWORK phase if time remains
       -> expiry: count once, republish original work, skip later work
  -> process dispatch
  -> eret
```

EOI is not exception return. During the service pass execution remains in EL1,
EL0 remains paused, the exception frame stays on the EL1 stack, and normal IRQs
remain masked by the entry state.

The deadline is cooperative. It stops only at explicit safe checkpoints and
cannot interrupt one operation already in progress.

## Enforced runtime bounds

| Work class | Rule | Continuation |
|---|---|---|
| Whole service | One nominal timer interval at safe checkpoints | Republish original work on expiry. |
| Virtio-input | <= negotiated ring and <=16 descriptors/call | Later descriptors remain in ring. |
| USB HID | Four fixed device visits/call | All supported direct slots fit. |
| Shared input | 16 events/active pass | Requeue INPUT when events remain. |
| Partial redraw | Eight ordered rectangles/successful submission | Preserve remainder; failure consumes none. |
| Virtio-net RX | 16 valid frames/NETWORK pass | Conservatively requeue NETWORK at cap. |

Network polling and descriptor receive consume nothing outside NETWORK phase. The
legacy cooperative poll cannot bypass the count/deadline contracts.

## Automated runtime evidence

### Forced-expiry stress

PR #61 recorded:

```text
EL0 heartbeat markers:        509
deadline republish markers:   311
input/redraw/network/DHCP:     present
observable input overflow:         0
panic markers:                     0
```

This proves liveness while the production republish path is exercised. Forced
expiry is instrumentation, not natural latency evidence.

### Natural RX saturation

PR #62 preserved the production deadline and recorded:

```text
EL0 yields:                       38,912
input events consumed:                 16
redraw submissions:                   738
virtio-net frames consumed:        29,234
maximum frames/pass:                   16
network cap exhaustions:             1,827
runtime requeues:                    1,827
natural deadline overruns:               0
maximum duration:                  385,763 / 625,000 ticks (61.7%)
observable input overflow:               0
panic markers:                           0
```

This proves continuation and repeated EL0 progress for frames that reached
software. It cannot prove delivery of every host-submitted packet because the
virtio-net interface exposes no trustworthy device/ring-drop counter.

## Verification matrix

| Check | Evidence | Current scope |
|---|---|---|
| QEMU build/size | BUILD/CI-VERIFIED | `.data == 0`; 107918 bytes under 108000. |
| RPi4 build/probe | BUILD/HOST/CI-VERIFIED | Normal/probe images build; unsupported normal capabilities fail closed. |
| Native host suite | HOST/CI-VERIFIED | Kernel, memory, VFS, FAT, GUI, parser, driver, and ABI contracts pass. |
| Runtime service | HOST/QEMU/CI-VERIFIED | EOI, coalescing, reset, metrics, count, routing, deadline, forced expiry, and natural RX stress pass. |
| Process/VFS/user copy | HOST/QEMU/CI-VERIFIED | Parent/wait, process-local descriptors, permission-aware copy, invalid-output rejection pass. |
| KLI1/stack | BUILD/HOST/CI-VERIFIED | Mutable-storage contract passes; recorded Editor max 368 / 3072 bytes. |
| FAT32 smoke | QEMU/CI-VERIFIED with open investigation | Current tree passes; one earlier intermittent EL1 VMM panic is `RISK-018`. |
| GUI/focus/framebuffer | QEMU/CI-VERIFIED | Panel/window readiness, focus, and FAT + GPU wiring markers pass. |
| USB/network | QEMU/CI-VERIFIED | xHCI, two HID devices, and DHCP markers pass. |
| Visible FAT workflow | MANUAL-VERIFIED, historical | Rocco completed create/edit/save/rename/reopen/delete on 2026-07-17; final post-runtime pass remains. |
| Physical Raspberry Pi | UNVERIFIED | No promoted physical subsystem evidence. |

## Fixed limits

| Area | Limit |
|---|---|
| Production kernel | 108000 bytes; audited image 107918 |
| PMM | 128 MiB |
| Processes | 16 slots |
| User regions | Eight/process |
| VFS | 24 nodes, four mounts, eight FDs/process, 64-byte paths |
| FAT32 | Root 8.3 only; no directories/LFN |
| GUI | 16 windows, 32 events/window, 32 damage rectangles, eight partial/submission |
| Input | 64 events; overflow counted but not prevented |
| Virtio-input | Ring <=16; one ring-length/call |
| USB HID | Four direct devices; no hubs |
| Network RX | 16 valid frames/NETWORK pass; device drops unavailable |
| Editor | 512-byte buffer; caret-line viewport |
| Files | `/fat` only; eight displayed entries |
| Network API | No sockets, TCP, DNS, HTTP, or general user UDP API |
| User copy | Permission-aware, not fault-recoverable |
| RPi4 | Build/host scaffolding only |

## Formal v0.2 blockers

### RISK-017

Automated contract evidence is complete. Promotion still requires explicit
acceptance or replacement of:

- missing device-level RX-drop telemetry;
- the one-operation full-redraw boundary;
- final visible/manual evidence on the promotion tree.

### RISK-018 / issue #63

One earlier FAT32 smoke run panicked at the `table[index]` load in `next_table()`.
Reruns passed, so the observation is intermittent and unexplained.

Draft PR #64 adds repeated production FAT32 boots and live GDB capture. Its branch
status at this audit:

- Verify #299: success;
- CI #439: cancelled by the workflow time limit.

PR #64 is investigation evidence only. It has not changed the promoted main gate
or closed the risk.

### Release record

No final v0.2 promotion commit, exact full-gate record, post-runtime visible
workflow, tag, or release note exists.

## Incomplete product work

- generic block descriptor and flush/read-only contract;
- normalized path and mount resolver;
- structured directory/metadata ABI;
- mkdir, truncate, structured stat/readdir, filesystem info;
- FAT long names/directories;
- ext2;
- shared userland heap/container/widget layer;
- complete daily applications;
- reboot persistence;
- 30-minute visible-session evidence;
- fault-contained copyin/copyout;
- TTBR1, ASIDs, user-only TTBR0, scoped invalidation;
- verified physical Raspberry Pi runtime.

## Promotion commands

Automated:

```sh
bash tools/verify.sh
```

Visible/manual:

```sh
make qemu-fb-visible
```

The manual record must name tester, date, exact tree/image, setup, steps, result,
and limitations.

## Next sequence

1. Complete or explicitly disposition PR #64 / issue #63.
2. Accept or replace the device-drop and full-redraw residuals in `RISK-017`.
3. Run the full automated gate on the exact promotion tree.
4. Run the dated visible FAT workflow on that tree.
5. Close or accept `RISK-017` and `RISK-018` with rationale.
6. Create the v0.2 tag and release record with exact identities and limitations.
7. Begin v0.3 through small block-descriptor, path-normalizer, mount-resolver, and
   structured-metadata cuts.
