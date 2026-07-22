# Agent Guide

This file is the live operating entry point for agents working in ArmoniOS.

Do not treat it as the source of current verification evidence. Read the canonical
status and risk documents before changing code.

## Read first

1. `docs/CURRENT_STATE.md` — audited operational truth
2. `docs/TECHNICAL_RISKS.md` — open correctness and release risks
3. `docs/ROADMAP.md` — milestone order and exit criteria
4. `docs/DEVELOPMENT_GUIDE.md` — repository map and change workflow
5. `docs/ARCHITECTURE.md` — implemented system design
6. `docs/RUNTIME_SERVICE.md` — exact post-EOI contract
7. `docs/MEMORY_MAP.md` — address-space and mapping policy
8. `docs/SYSCALLS.md` and `docs/GUI_ABI_NOTES.md` — user/kernel ABI
9. `docs/CONTRIBUTING.md` — evidence, style, and PR discipline
10. `docs/PORTING.md` — board contract and hardware evidence
11. `docs/DOCUMENTATION_POLICY.md` — claim and evidence rules

`CURRENT_STATE.md` says what is verified. `ROADMAP.md` says what is planned.
Issues, PRs, comments, and chat summaries are context until canonical documents are
updated.

## Current project classification

ArmoniOS has a verified v0.1 QEMU desktop baseline. The v0.2 runtime-hardening
implementation and automated evidence have landed, but formal v0.2 promotion is
still blocked by the residual-risk and release-evidence work listed in
`CURRENT_STATE.md`.

Do not call the tree a release candidate while required P1 dispositions and the
final visible pass are missing. Use **v0.2 promotion candidate** or **v0.2
hardening candidate**.

The current production kernel remains under the fixed 108000-byte ceiling. Do not
raise that ceiling to avoid compaction or design work.

The next product milestone after formal v0.2 promotion is v0.3 storage/VFS
infrastructure. Do not bypass it with broad Files, Editor, Shell, or FAT-specific
polish.

## Current runtime facts

The execution model is:

```text
preemptive EL0 processes
  -> bounded hard IRQ callback
  -> EOI
  -> measured post-EOI EL1 runtime pass
  -> process dispatch
  -> eret

plus cooperative EL1 helper threads
```

EOI is not exception return. During the runtime pass:

- execution remains in EL1;
- EL0 remains paused;
- the 288-byte exception frame remains on the EL1 stack;
- normal IRQs remain masked by the vector entry state;
- the service is cooperatively bounded at explicit checkpoints, not
  asynchronously preempted.

Current enforced runtime rules:

- whole service: one nominal timer interval at safe checkpoints;
- virtio-input producer: at most one negotiated ring length and no more than 16
  used descriptors per call;
- USB HID producer: at most four fixed device visits per call;
- shared input consumer: at most 16 events per active pass;
- partial compositor damage: at most eight ordered rectangles per successful
  redraw submission;
- virtio-net RX: at most 16 valid frames per active NETWORK pass;
- network polling and receive consume nothing outside the NETWORK phase;
- deadline expiry is counted once and republishes the original work snapshot;
- native continuation remains in queues, rings, or the damage list.

Known runtime boundaries:

- one already-started full redraw or driver call may cross the nominal deadline
  before the next checkpoint;
- device/ring RX drops are not observable through the current virtio-net
  interface;
- pending state and telemetry assume one CPU and one consumer;
- runtime telemetry is kernel-internal and is not a public syscall ABI.

Read `docs/RUNTIME_SERVICE.md` before changing `kernel/irq.c`, timer publication,
input routing, compositor submission, USB polling, or virtio-net receive.

## Current release blockers

Do not hide or silently downgrade these:

- `RISK-017`: residual v0.2 runtime-boundary disposition and final visible evidence;
- `RISK-018` / issue #63: intermittent EL1 VMM data abort investigation;
- final v0.2 promotion commit, exact full-gate evidence, tag, and release record.

The repeated FAT32/VMM soak work in PR #64 is investigation evidence only until it
is merged and promoted into `CURRENT_STATE.md`.

## Architectural invariants

Preserve the invariants relevant to each change:

- freestanding C11 kernel and userland;
- narrow AArch64 assembly boundaries;
- `.data == 0` for the production kernel and shipping KLI1 images;
- kernel W^X policy;
- 108000-byte production kernel ceiling;
- 3072-byte userland stack gate unless deliberately revised;
- append-only syscall numbers and stable public structure layouts;
- explicit ownership and cleanup for pages, tables, descriptors, windows, IPC,
  mappings, and process resources;
- process-local VFS descriptors;
- fail-closed unverified Raspberry Pi capabilities;
- deterministic QEMU assertions rather than timeout-only launches;
- exact separation of host, build, QEMU, manual, and physical evidence.

## Change discipline

### Memory, VMM, process, and user-copy

- Keep leaf-page ownership separate from page-table ownership.
- Add rollback, failure, cleanup, and double-release tests.
- Do not free a page-table hierarchy while any live process can activate it.
- Validate overflow-safe ranges before mapping or copying.
- Do not pass raw user pointers into lower subsystems.
- Import input into kernel-owned buffers and build output there before copying.
- Fault-contained copyin/copyout remains open hardening; do not claim current
  permission checks make final loads/stores recoverable.

### Runtime and interrupts

- Keep the physical timer callback fixed and bounded.
- Do not render, block, allocate through an unbounded path, drain arbitrary
  queues, parse arbitrary traffic, or scan dynamic structures in hard IRQ.
- Post-EOI work still pauses EL0; preserve count and time stop rules.
- Count completed work or real device operations, not generic attempts.
- Preserve independent PERIODIC, INPUT, and NETWORK readiness.
- Preserve strict NETWORK-phase routing.
- Preserve input ring, queue, and damage-list continuation semantics.
- Do not infer loss absence from consumption counters.

### Syscall and ABI

- Append syscall numbers; never reuse one.
- Land kernel dispatch, user wrappers, tests, and ABI documentation together.
- Validate complete output destinations before consuming events or IPC state.
- Add a versioned diagnostic ABI before exposing internal runtime telemetry.
- Planned v0.3 filesystem syscalls are not current ABI.

### Storage and filesystems

- Keep filesystem policy behind generic mount/filesystem callbacks.
- Do not call the root-only 8.3 bridge general FAT.
- Add host fixtures, malformed-input tests, and overflow checks before QEMU claims.
- Add reboot/persistence evidence before claiming durable writes.
- Keep Raspberry Pi normal storage fail closed until physical evidence exists.

### Userland and GUI

- Shipping KLI1 images keep mutable `.data` and `.bss` empty.
- Use `SYS_MMAP` for large mutable state.
- Keep stack checks green.
- Run visible QEMU for layout, focus, keyboard, mouse, or workflow claims.
- Do not add controls that do not perform a real action.
- Do not broaden v0.6 application work before v0.3-v0.5 foundations.

## Verification workflow

Use the smallest relevant focused test while developing. Before promotion run:

```sh
bash tools/verify.sh
```

Relevant focused commands include:

```sh
make BOARD=qemu_virt
make BOARD=qemu_virt size
make -C tests test
bash tests/run_runtime_service_test.sh
bash tests/run_input_queue_stats_test.sh
bash tests/run_process_parent_wait_test.sh
bash tests/run_vfs_process_fd_test.sh
bash tests/run_user_copy_permissions_test.sh
bash tests/run_kli1_contract_test.sh
make stack-check
make qemu-fs-test
bash tools/qemu_usercopy_test.sh
bash tools/qemu_focus_test.sh
bash tools/qemu_marker_test.sh all
bash tools/qemu_runtime_stress_test.sh
bash tools/qemu_runtime_net_stress_test.sh
bash tools/qemu_fb_fat_test.sh
```

Visible evidence is separate:

```sh
make qemu-fb-visible
```

Record tester, date, exact commit/image, workflow, observed behavior, and
limitations. Serial markers are not visible manual validation.

## Documentation rules

- Keep canonical repository documentation in English.
- Update from evidence, not intent.
- Record exact commit trees and workflow run IDs for promoted baselines.
- Distinguish PR-head, synthetic merge, final merge, local, QEMU, and hardware
  evidence.
- A documentation-only change may clarify or downgrade a claim, but may not
  upgrade runtime evidence.
- Update risk state first, current state second, contract documents third, roadmap
  fourth, operating guides fifth, and README last.
- Do not create another current-state, audit, handoff, archive, or latest-status
  file.

## Pull-request expectations

Every behavior-changing PR must state:

- the exact problem and affected contract;
- ownership and failure behavior;
- focused commands and exact results;
- full-gate evidence on the final tree;
- manual checks or an explicit `not run` entry;
- what the evidence does not prove;
- risk and documentation changes.

Keep PRs narrow enough that code, tests, evidence, and documentation describe one
coherent contract.
