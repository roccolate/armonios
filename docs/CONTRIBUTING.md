# Contributing to ArmoniOS

ArmoniOS should remain small enough that one developer can understand the whole
system. That requires narrow changes, explicit ownership, bounded work, evidence
that matches each claim, and documentation that never converts intention into
fact.

## Read first

Read these documents in order:

1. `CURRENT_STATE.md` — audited operational status;
2. `TECHNICAL_RISKS.md` — active correctness and release risks;
3. `ROADMAP.md` — dependency order and milestone exit criteria;
4. `DEVELOPMENT_GUIDE.md` — repository map and practical change workflow;
5. `ARCHITECTURE.md` — implemented kernel model;
6. `RUNTIME_SERVICE.md` — exact timer and post-EOI semantics;
7. `MEMORY_MAP.md` — address-space and mapping policy;
8. `SYSCALLS.md` and `GUI_ABI_NOTES.md` — user/kernel ABI;
9. `PORTING.md` — board contract and hardware evidence;
10. `DOCUMENTATION_POLICY.md` — evidence and claim rules.

Repository-root `AGENTS.md` is a compact operating guide derived from these
sources. `CURRENT_STATE.md` is authoritative for what is promoted. `ROADMAP.md`
is future intent.

## Current project rule

ArmoniOS has a complete v0.1 QEMU desktop baseline. The v0.2 runtime-hardening
implementation and automated evidence have landed, but formal v0.2 promotion
still requires the risk and release work listed in `CURRENT_STATE.md`.

Until v0.2 is promoted:

- preserve all count, deadline, strict-routing, stress, storage, ABI, stack, size,
  and board gates;
- keep issue #63 / `RISK-018` isolated from unrelated features;
- do not describe the tree as a release candidate;
- do not begin a broad v0.3 rewrite on `main` without explicit risk disposition;
- do not broaden application scope around foundations v0.3-v0.5 will replace.

The next architecture milestone after v0.2 is v0.3 storage/VFS infrastructure,
not direct Files/Editor polish and not an in-place expansion of the root-only FAT
bridge.

## Development setup

On Ubuntu or WSL2:

```sh
sudo apt update && sudo apt install -y \
  qemu-system-arm \
  gcc-aarch64-linux-gnu \
  binutils-aarch64-linux-gnu \
  gdb-multiarch \
  make

git clone https://github.com/yourname/armonios
cd armonios
make BOARD=qemu_virt
```

The default board is `qemu_virt`.

## Evidence levels

Use the exact labels in `DOCUMENTATION_POLICY.md`:

- `IMPLEMENTED`;
- `HOST-VERIFIED`;
- `BUILD-VERIFIED`;
- `QEMU-VERIFIED`;
- `CI-VERIFIED`;
- `MANUAL-VERIFIED`;
- `UNVERIFIED`;
- `KNOWN-BROKEN`;
- `PLANNED`.

Never write “all tests pass” when only a subset ran. Never write “works in QEMU”
when only host tests ran. Never convert a marker into a visible/manual claim.
Never treat repeated non-reproduction as a root-cause fix.

## Work from one contract

Prefer one reviewable contract per pull request:

- one ownership rule;
- one parser or normalization layer;
- one ABI addition;
- one runtime budget or continuation rule;
- one device boundary;
- one visible workflow;
- one diagnostic/reproduction harness.

A phase-sized rewrite is harder to prove, review, bisect, and revert. Stage large
milestones through independently green cuts.

## Verification workflow

Start with the smallest relevant test. Finish with the complete automated gate on
the final tree.

### Small pure-C change

Examples: parser, range arithmetic, path normalization, metadata conversion.

```sh
make -C tests test
make BOARD=qemu_virt
make BOARD=qemu_virt size
```

Add a focused host test that exercises invalid input and boundary behavior, not
only the success path.

### Userland or library change

```sh
make BOARD=qemu_virt
make BOARD=qemu_virt size
make -C tests test
bash tests/run_kli1_contract_test.sh
make stack-check
```

Run `make qemu-fb-visible` when behavior depends on layout, windows, focus,
keyboard, mouse, redraw, or user workflow.

### Process, VMM, syscall, or user-copy change

Relevant focused gates include:

```sh
bash tests/run_process_parent_wait_test.sh
bash tests/run_vfs_process_fd_test.sh
bash tests/run_user_copy_permissions_test.sh
bash tools/qemu_usercopy_test.sh
```

Also require:

- overflow-safe range tests;
- failure and rollback tests;
- exactly-once cleanup evidence;
- page-table versus leaf-page ownership preservation;
- no raw user pointer passed into lower subsystems;
- ABI and memory documentation when the contract changes.

An unexpected EL1 fault is release-critical until triaged in
`TECHNICAL_RISKS.md`.

### Timer, IRQ, runtime service, input, GUI, USB, or network change

Run at least:

```sh
bash tests/run_runtime_service_test.sh
bash tests/run_input_queue_stats_test.sh
make -C tests test
bash tools/qemu_focus_test.sh
bash tools/qemu_marker_test.sh all
bash tools/qemu_runtime_stress_test.sh
bash tools/qemu_runtime_net_stress_test.sh
```

A runtime change must prove:

- the physical callback remains fixed and bounded;
- EOI ordering remains correct;
- pending work is not lost;
- every stop rule preserves native continuation;
- class counts measure completed work or real device operations;
- strict NETWORK-phase routing remains enforced;
- the service-wide deadline remains configured and checked at safe boundaries;
- observable queue overflow or panic fails the stress gate;
- natural versus forced timing evidence is distinguished;
- unobservable device loss remains explicit;
- the exact exception-context assumptions remain documented.

Post-EOI work is not automatically safe. EL0 and normal IRQs remain paused until
`eret`.

### Storage or filesystem change

Run focused parser/image tests plus:

```sh
make qemu-fs-test
bash tools/qemu_fb_fat_test.sh
```

Broader path, mount, FAT, or filesystem claims require dedicated evidence for:

- malformed geometry and images;
- overflow and end-of-device access;
- read-only behavior;
- mount boundaries;
- traversal policy;
- rollback and partial progress;
- long names/directories where claimed;
- reboot persistence where durability is claimed.

The current root-only FAT32 smoke is not evidence for general FAT.

### Raspberry Pi change

At minimum:

```sh
make BOARD=rpi4
make rpi4-emmc2-probe
bash tests/run_board_build_test.sh
bash tests/run_rpi4_emmc2_probe_diag_test.sh
```

Before a physical claim, record:

1. exact board revision;
2. firmware and boot files;
3. power, UART, display, and storage setup;
4. tested image commit and hash;
5. repeatable cold-boot evidence;
6. subsystem-specific behavior;
7. destructive-storage safety plan where applicable.

The current RPi4 backend is not a known-good physical reference.

### Complete automated promotion gate

Before merging code, ABI, build-contract, promoted-status, or release changes:

```sh
bash tools/verify.sh
```

The promoted `main` gate currently covers:

- QEMU and RPi4 builds;
- `.data == 0` and the 108000-byte production kernel limit;
- RPi4 diagnostic package, EMMC2, MBR, and bounded block-view host checks;
- native kernel, memory, VFS, filesystem, GUI, parser, driver, and ABI tests;
- runtime EOI, coalescing, reset, metrics, count, routing, deadline, and
  continuation contracts;
- input queue depth, high-water, and overflow;
- process parent/wait and descriptor ownership;
- user-copy permissions;
- KLI1 mutable-storage contract;
- userland stack use;
- FAT32 QEMU smoke;
- user-copy and focus QEMU regressions;
- framebuffer, USB, DHCP, and FAT + GPU wiring markers;
- deterministic forced-expiry runtime stress;
- natural-deadline virtio-net RX saturation.

An open investigation branch may add extra gates. Those are not part of the
promoted baseline until merged and recorded in `CURRENT_STATE.md`.

### Visible evidence

Run separately:

```sh
make qemu-fb-visible
```

Record:

- tester;
- date;
- exact commit and image;
- launch command;
- display and input setup;
- workflow steps;
- result;
- observed limitations.

## Current non-negotiable contracts

### Build and image

- freestanding C11 kernel and userland;
- narrow AArch64 assembly boundaries;
- production kernel `.data == 0`;
- shipping KLI1 mutable `.data` and `.bss` forbidden;
- kernel W^X;
- production kernel <= 108000 bytes;
- userland measured stack <= 3072 bytes unless intentionally revised;
- no hidden libc, POSIX, C++, dynamic linker, or hosted-runtime dependency.

### Ownership

Every process-visible or allocated resource needs:

- owner identity;
- valid lifetime;
- foreign-use rejection where applicable;
- cleanup on exit/failure;
- rollback for partial construction;
- test coverage for repeated cleanup or invalid state.

This applies to pages, page tables, mappings, descriptors, windows, event queues,
IPC state, and device buffers.

### Hard IRQ and post-EOI work

Hard callbacks may:

- acknowledge/account an event;
- copy a bounded status value;
- rearm hardware;
- publish bounded readiness;
- update fixed counters.

They must not:

- block or wait;
- perform filesystem work;
- render;
- drain arbitrary queues;
- scan arbitrary device sets;
- parse arbitrary traffic;
- allocate through an unbounded path;
- traverse large dynamic structures.

The post-EOI service must preserve the current count, deadline, strict-routing,
and continuation rules documented in `RUNTIME_SERVICE.md`.

### ABI

Syscall numbers and public structures are append-only before v1 unless an explicit
incompatibility is approved.

Any ABI change must update together:

- kernel constants and dispatch;
- implementation;
- `libkarm` or `libkarmdesk` wrappers;
- layout/ABI tests;
- real application consumer;
- `SYSCALLS.md` or `GUI_ABI_NOTES.md`;
- current state and risks when behavior changes.

Planned v0.3 filesystem calls are not current ABI.

### Board boundary

Generic kernel code must not contain board physical addresses or assume virtio.
A board implementation must satisfy the generic contract or return an explicit
safe failure. Missing symbols and silent success stubs are not acceptable.
Capabilities may be enabled only with matching runtime evidence.

## Code standards

### Style

- four-space indentation;
- braces on the same line;
- `snake_case` functions and variables;
- `UPPER_SNAKE_CASE` constants;
- `_t` suffix for useful typedefs;
- `g_` prefix for mutable file-scope globals;
- fixed-width types at ABI and hardware boundaries;
- overflow-safe arithmetic;
- explicit capacities and failure behavior.

Public headers should document ownership, valid ranges, context restrictions,
and whether a call may execute in IRQ/exception context.

### Assembly

Follow AAPCS64 and protect C/assembly layouts with static assertions where
possible.

Comment:

- exception-level assumptions;
- IRQ-mask behavior;
- register ownership;
- stack and trap-frame layout;
- page-table/TLB barriers;
- saved-context layout;
- return semantics.

## Required pull-request evidence

Every behavior-changing PR should contain:

```text
Problem:
- exact defect, missing contract, or milestone slice

Change:
- implementation and ownership/failure boundaries

Focused evidence:
- command -> exact result or asserted marker

Full evidence:
- final-tree command/workflow -> exact result

Manual evidence:
- workflow -> tester/date/commit/result
- or: not run -> reason

Not proved:
- explicit evidence boundary

Risks and docs:
- risk IDs and canonical files changed
```

Use exact commit SHAs and workflow IDs only after the final tree is known. A
PR-head run proves the tree it exercised; it does not automatically prove a later
conflict-resolved merge tree.

## Documentation ownership

The author of a behavior-changing change owns its documentation. Update in this
order:

1. implementation and focused tests;
2. `TECHNICAL_RISKS.md` when risk state changes;
3. `CURRENT_STATE.md` with exact evidence;
4. architecture, runtime, memory, ABI, or porting contracts;
5. `ROADMAP.md` when ordering or exit criteria change;
6. `DEVELOPMENT_GUIDE.md`, `CONTRIBUTING.md`, and `AGENTS.md` when operating rules
   change;
7. README last.

Do not create another status, handoff, audit, latest-status, or progress document.

## Commit and PR discipline

Use focused conventional commits:

```text
type(scope): concise description
```

Common types: `feat`, `fix`, `docs`, `refactor`, `test`, `chore`.

A PR may contain multiple focused commits, but its final tree must have one
coherent implementation, evidence, risk, and documentation story.

## PR checklist

- [ ] Scope is reviewable and contains no unrelated files.
- [ ] Ownership and failure behavior are explicit.
- [ ] Smallest relevant tests are recorded.
- [ ] Build, `.data`, and size are recorded when applicable.
- [ ] Stack/KLI1 gates are recorded for userland changes.
- [ ] Deterministic QEMU evidence exists for runtime changes.
- [ ] Stress/soak claims record workload, duration/iterations, liveness, failure
      conditions, and evidence boundary.
- [ ] Manual UI/hardware checks name tester, date, environment, and exact tree.
- [ ] Unrun checks are listed with reasons.
- [ ] Runtime/IRQ changes document execution context, budgets, and continuation.
- [ ] Risk state matches the final tree.
- [ ] `CURRENT_STATE.md` matches promoted evidence.
- [ ] ABI and architecture documents match code.
- [ ] README remains a summary rather than an independent truth source.

## Changes that will not be accepted

- unsupported hardware claims;
- code presence presented as runtime evidence;
- timeout-only launches presented as passing tests;
- in-progress workflows presented as success;
- clean reruns presented as root-cause fixes;
- unowned global process-visible state;
- raw user-pointer use in lower subsystems;
- heavy or unbounded hard-IRQ work;
- hidden ABI breaks;
- broad rewrites without staged tests;
- speculative POSIX/libc scope;
- general filesystem claims based on the root-only FAT bridge;
- controls that do not perform a real action;
- temporary hacks without a tracked risk or issue;
- raising the kernel ceiling to avoid design or compaction work.

## Communication

Use issues for defects and scoped work. Use pull requests for reviewable changes.
Keep repository code and canonical technical documentation in English.

## License

Contributions are licensed under GPL-2.0.
