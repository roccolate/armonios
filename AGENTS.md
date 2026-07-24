# Agent guide

This is the compact operating entry point for agents working in ArmoniOS. It is
not the source of current verification evidence.

## Read first

1. `docs/CURRENT_STATE.md` — current `main` capability;
2. `docs/TECHNICAL_RISKS.md` — active risks;
3. `docs/ROADMAP.md` — remaining milestone order;
4. `docs/DEVELOPMENT_GUIDE.md` — repository map and workflow;
5. `docs/ARCHITECTURE.md` — implemented design;
6. the focused reference for the subsystem;
7. `docs/CONTRIBUTING.md` — contribution and evidence discipline;
8. `docs/DOCUMENTATION_POLICY.md` — claim rules.

Useful focused references include:

- `docs/RUNTIME_SERVICE.md`;
- `docs/MEMORY_MAP.md`;
- `docs/SYSCALLS.md`;
- `docs/PUBLIC_ABI.md`;
- `docs/LIBKARM.md`;
- `docs/LIBARMDESK.md`;
- `docs/GUI_ABI_NOTES.md`;
- `docs/V03_IMPLEMENTATION_STATUS.md`;
- `docs/PORTING.md`.

Issues, PRs, reviews, and chat summaries are context until their durable facts are
promoted into canonical documentation.

## Current classification

- v0.1 QEMU graphical baseline: verified;
- v0.2 runtime-hardening implementation and automated evidence: complete;
- v0.2 release record: pending issue #76 for the final visible workflow, exact
  promotion tree, tag, and release notes;
- v0.3 storage/VFS platform: active;
- v0.5 userland runtime: early partial through `libkarm`;
- reusable widget toolkit: not promoted;
- Raspberry Pi 4: build/host-contract scaffolding only.

RISK-017 and RISK-018 are closed. Do not repeat their old investigation state or
block v0.3 work on them. Do not call the tree a completed v0.2 release while issue
#76 remains open.

## Image and build policy

The default QEMU image budget is **128 KiB (131072 bytes)**.

Measure the current image instead of copying a stale size into documentation:

```sh
make BOARD=qemu_virt size
```

Preserve:

- freestanding C11 and narrow AArch64 assembly;
- no hidden libc, POSIX, C++, dynamic linker, or hosted runtime;
- kernel W^X;
- empty loadable `.data` for the production kernel and shipping KLI1 images;
- userland stack gate;
- deterministic QEMU and host tests;
- fail-closed unsupported board capabilities.

## Execution model

```text
EL0 process or EL1 kernel path
  -> IRQ entry
  -> fixed hard callback
  -> interrupt-controller EOI
  -> bounded post-EOI EL1 runtime service
  -> process dispatch only for an IRQ that originated from EL0
  -> eret
```

Only an IRQ frame from EL0 may enter process save/preemption. An IRQ that
interrupts EL1 may service devices and deferred work but must return to the exact
interrupted kernel path without switching process or TTBR0.

The post-EOI service remains inside exception context. EL0 is paused, the frame
remains on the EL1 stack, and the deadline is cooperative rather than
asynchronous.

Current runtime rules:

- one nominal timer interval as the service-wide checkpoint budget;
- virtio-input: one negotiated ring length, capped at 16 descriptors;
- USB HID: four direct-device visits;
- shared input: 16 events;
- partial redraw: eight ordered rectangles after successful submission;
- virtio-net RX: 16 valid frames in NETWORK phase;
- network receive consumes nothing outside NETWORK phase;
- deadline expiry republishes conservative readiness;
- native continuation remains in rings, queues, or damage state.

Consumption counters do not prove absence of device/ring drops. Internal runtime
telemetry is not a public ABI.

## Ownership rules

Every allocated or process-visible resource needs:

- explicit owner;
- valid lifetime;
- foreign-use rejection where applicable;
- failure rollback;
- exactly-once cleanup;
- repeated-cleanup or invalid-state tests.

Keep page-table pages separate from mapped leaf-page ownership. Do not pass raw
user pointers into lower subsystems; import input and build output in kernel-owned
storage.

Final user copies are permission-aware but not fault-contained. Do not claim
recoverable copyin/copyout until RISK-015 is resolved.

## ABI rules

Public numbers and records live under `include/armonios/abi/`.

- append syscall numbers;
- never reuse a number or status value;
- preserve published layout sizes and field order;
- use explicitly versioned replacement records for richer contracts;
- update kernel, wrappers, tests, real consumer, and docs together;
- keep `libkarm` GUI-independent;
- put desktop wrappers and shared controls in `libarmdesk`.

Current VFS ABI includes structured metadata calls 49/50 and filesystem
information call 51. Planned mutation operations are not current ABI until merged.

## Storage rules

Current foundations include:

- generic block devices and bounded views;
- whole-device and primary-MBR FAT32 discovery;
- canonical paths and longest-prefix mount resolution;
- existing nested 8.3 read traversal;
- structured metadata and filesystem information.

Current mutation remains root-entry only. Long names, mkdir/rmdir, truncate,
nested mutation, and reboot-verified durability are incomplete.

Keep filesystem policy behind filesystem callbacks. Add malformed fixtures,
overflow checks, read-only behavior, specific statuses, rollback, and one real
consumer. Do not claim durability without a reboot test.

## Userland and desktop rules

`libkarm` currently provides syscall wrappers, minimal I/O, monotonic arenas,
growable buffers, dynamic strings, complete descriptor writes, and rollback-safe
file reads.

`libarmdesk` currently provides GUI wrappers, rectangle helpers, and theme tokens.
The closed unmerged widget work is not part of `main`.

For userland changes:

- keep mutable static `.data` and `.bss` empty;
- use `SYS_MMAP` for larger mutable state;
- measure stack and image size;
- land reusable components with tests and a real application consumer;
- run visible QEMU for layout or interaction claims;
- do not add controls that perform no real action.

## Raspberry Pi rules

Generic code must not contain board physical addresses or silently assume virtio.
A board implementation satisfies the generic contract or fails unsupported
capabilities explicitly.

`make BOARD=rpi4` and diagnostic probe tests are build/host evidence only. Physical
support requires exact board, firmware, boot, serial, memory/timer, subsystem,
repeatability, and destructive-storage safety evidence.

## Verification

Develop with focused tests. Before promotion run:

```sh
bash tools/verify.sh
```

Common focused commands are maintained in `docs/DEVELOPMENT_GUIDE.md`.

Visible evidence is separate:

```sh
make qemu-fb-visible
```

Record tester, date, exact tree/image, setup, workflow, result, and limitations.

## Documentation rules

- describe current facts in present tense;
- label future work explicitly;
- do not create a competing current-state or progress-log document;
- keep hashes, workflow IDs, measured sizes, and dated observations in
  `docs/history/` or release records;
- documentation-only work may correct or downgrade claims, but may not invent
  runtime evidence;
- update focused references with code, broader canonical documents when their
  role changes, and README last.

## Pull-request expectations

A behavior-changing PR should state:

- exact problem and contract;
- implementation and ownership/failure boundary;
- compatibility impact;
- focused commands and results;
- full-gate result on the final tree;
- manual evidence or explicit `not run`;
- what the evidence does not prove;
- risk and documentation changes.

Keep the change small enough that code, tests, evidence, and documentation remain
one coherent reviewable unit.
