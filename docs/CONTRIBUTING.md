# Contributing to ArmoniOS

ArmoniOS is a freestanding AArch64 operating system with fixed-capacity
subsystems, a small public ABI, and a QEMU-first verification policy. Changes
should be narrow enough that implementation, ownership, tests, evidence, and
documentation describe one coherent contract.

Read first:

1. `CURRENT_STATE.md`;
2. `TECHNICAL_RISKS.md`;
3. `ROADMAP.md`;
4. `ARCHITECTURE.md`;
5. `DEVELOPMENT_GUIDE.md`;
6. the focused reference document for the subsystem;
7. `DOCUMENTATION_POLICY.md`.

Repository-root `AGENTS.md` is the compact operating guide derived from these
sources.

## Current project boundary

- the v0.1 QEMU desktop baseline is verified;
- v0.2 runtime-hardening implementation and automated evidence are complete;
- issue #76 remains the manual visible validation, tag, and v0.2 release-record
  task;
- v0.3 storage/VFS work is active;
- v0.5 userland-runtime foundations exist through `libkarm`, but reusable widgets
  are not promoted;
- Raspberry Pi 4 is build/host-contract scaffolding, not physical runtime support.

Do not block current v0.3 work on already closed RISK-017 or RISK-018. Do not call
the tree a final v0.2 release until issue #76 is complete.

## Development setup

On Ubuntu or WSL2:

```sh
sudo apt update && sudo apt install -y \
  qemu-system-arm \
  gcc-aarch64-linux-gnu \
  binutils-aarch64-linux-gnu \
  gdb-multiarch \
  make

git clone https://github.com/roccolate/armonios
cd armonios
make BOARD=qemu_virt
```

The default development and regression board is QEMU `virt`.

## Before changing code

Identify:

- the exact problem;
- the current contract;
- the owner of every allocated or queued resource;
- failure and rollback behavior;
- the smallest test that can demonstrate the defect or missing behavior;
- whether the change affects the public ABI, KLI1 format, image size, userland
  stack, `.data`, runtime-service budgets, storage durability, or board claims.

Do not start from an issue title or chat summary alone when current code and
canonical documentation are available.

## Evidence labels

Use the labels defined in `DOCUMENTATION_POLICY.md`:

- `IMPLEMENTED`;
- `HOST-VERIFIED`;
- `BUILD-VERIFIED`;
- `QEMU-VERIFIED`;
- `CI-VERIFIED`;
- `MANUAL-VERIFIED`;
- `UNVERIFIED`;
- `KNOWN-BROKEN`;
- `PLANNED`.

Never write “all tests pass” for a subset. Never convert host evidence into QEMU
evidence, QEMU markers into visible evidence, or build-only board code into
physical support.

## Change size

Prefer one reviewable contract per change:

- one ownership rule;
- one parser or normalization layer;
- one ABI addition;
- one runtime budget or continuation rule;
- one device boundary;
- one filesystem operation;
- one reusable userland primitive;
- one visible workflow.

A good change has permanent focused tests, a real consumer when adding reusable
surface, and no temporary workflow or migration helper in the final tree.

## Non-negotiable invariants

Preserve the applicable invariants:

- freestanding C11 and narrow AArch64 assembly;
- no libc, POSIX, C++, dynamic linker, or hosted-runtime dependency;
- kernel W^X;
- empty loadable `.data` for production kernel and shipping applications;
- fixed 128 KiB QEMU image budget unless deliberately revised;
- userland stack gate;
- append-only syscall numbers and stable public layouts;
- explicit ownership and exactly-once cleanup;
- process-local VFS descriptors;
- fail-closed unsupported board capabilities;
- deterministic tests rather than timeout-only success.

## Public ABI changes

A public ABI change must include:

- headers under `include/armonios/abi/`;
- kernel dispatch and implementation;
- `libkarm` or `libarmdesk` wrappers;
- compile-time layout assertions;
- compatibility tests;
- a real consumer;
- `SYSCALLS.md` and `PUBLIC_ABI.md` updates;
- current-state, architecture, risk, or roadmap updates when applicable.

Never renumber or reuse an existing syscall or status value. Planned operations
are not part of the ABI until code, wrappers, tests, consumer, and docs land.

## Memory and ownership changes

Document who owns:

- page-table pages;
- mapped leaf pages;
- descriptors;
- windows and backing buffers;
- IPC messages;
- queue entries;
- arena storage;
- block-device views;
- filesystem mutation state.

Test allocation failure, partial construction, rollback, cleanup, repeated
cleanup, and process exit where relevant.

Only an IRQ originating from EL0 may enter process save/preemption. Preserve the
EL1 IRQ-origin gate for memory, process, exception, and scheduler changes.

## Runtime and IRQ changes

Read `RUNTIME_SERVICE.md` before changing timer publication, IRQ dispatch, input
polling, USB polling, redraw submission, or network receive.

Preserve or deliberately revise:

- EL0 versus EL1 IRQ-origin classification;
- fixed hard-callback work;
- EOI ordering;
- independent PERIODIC, INPUT, and NETWORK readiness;
- count budgets;
- deadline checkpoints;
- continuation and readiness republication;
- telemetry semantics;
- forced-expiry and natural-load stress coverage.

Consumption counters do not prove absence of device loss.

## Storage and filesystem changes

Keep filesystem policy behind VFS/filesystem callbacks. Add:

- malformed fixtures;
- overflow and range checks;
- read-only behavior;
- specific status results;
- rollback and partial-progress rules;
- a real userland consumer;
- QEMU evidence for integration.

Current nested 8.3 traversal is not a complete FAT implementation. Do not claim
durability until a reboot workflow verifies it.

## Userland and GUI changes

- use public ABI headers;
- keep `libkarm` GUI-independent;
- place desktop wrappers and shared controls in `libarmdesk`;
- do not document closed unmerged widget work as current capability;
- keep mutable state out of static `.data` and `.bss`;
- measure per-application stack and image growth;
- run visible QEMU when layout or interaction changes.

A reusable widget should land with host tests and a real application consumer,
not as unused framework surface.

## Raspberry Pi changes

At minimum run:

```sh
make BOARD=rpi4
make rpi4-emmc2-probe
bash tests/run_board_build_test.sh
bash tests/run_rpi4_emmc2_probe_diag_test.sh
```

These remain build/host evidence. Physical claims require exact board, firmware,
boot, power, serial, storage, display/input setup, repeated results, and a safety
plan for destructive writes.

## Verification

Develop with the smallest focused test. Common commands are listed in
`DEVELOPMENT_GUIDE.md`.

Before promotion run:

```sh
bash tools/verify.sh
```

The complete gate covers the current QEMU and Raspberry Pi build contracts, host
suites, image size, `.data`, stack, process/VFS/user-copy behavior, runtime
service, FAT32 QEMU smoke, GUI/device markers, and stress tests.

Manual visible evidence is separate:

```sh
make qemu-fb-visible
```

Record tester, date, exact tree/image, setup, steps, result, and limitations.

## Code style

- four-space indentation;
- braces on the same line;
- `snake_case` functions and variables;
- `UPPER_SNAKE_CASE` constants;
- `_t` suffix for useful typedefs;
- `g_` prefix for mutable file-scope globals;
- fixed-width types at ABI and hardware boundaries;
- overflow-safe arithmetic;
- explicit capacities, ownership, and failure behavior.

Assembly should follow AAPCS64 and document exception-level assumptions, IRQ-mask
behavior, register ownership, stack/trap-frame layout, barriers, and return
semantics.

## Pull-request description

A behavior-changing PR should state:

```text
Problem:
- exact defect, missing contract, or milestone slice

Change:
- implementation and ownership/failure boundaries

Focused evidence:
- command -> result or asserted marker

Full evidence:
- final-tree command/workflow -> result

Manual evidence:
- tester/date/tree/workflow/result
- or: not run -> reason

Not proved:
- explicit evidence boundary

Risks and docs:
- risk IDs and canonical files changed
```

A PR-head run proves the tree it exercised. It does not automatically prove a
later content-different merge.

## Documentation ownership

The author of a behavior-changing change owns its documentation. Update:

1. implementation and focused tests;
2. focused reference documents and public headers;
3. risks when risk state changes;
4. architecture when boundaries change;
5. current state when capability changes;
6. roadmap when planned work lands;
7. development/agent guides when operating rules change;
8. repository README last.

Exact historical identities belong in `history/` or a release record, not in live
contract documents.
