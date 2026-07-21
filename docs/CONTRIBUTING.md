# Contributing to ArmoniOS

ArmoniOS should remain small enough that one developer can understand the whole
system. That requires narrow changes, explicit ownership, measurable evidence,
and documentation that never upgrades intention into fact.

## Read first

Read these documents in order:

1. `DOCUMENTATION_POLICY.md` — evidence classes and claim rules;
2. `CURRENT_STATE.md` — audited operational status;
3. `TECHNICAL_RISKS.md` — active correctness and release risks;
4. `ROADMAP.md` — dependency order and milestone exit criteria;
5. `ARCHITECTURE.md` — implemented kernel model;
6. `RUNTIME_SERVICE.md` — exact timer/bottom-half semantics;
7. `SYSCALLS.md` and `GUI_ABI_NOTES.md` — user/kernel ABI;
8. `MEMORY_MAP.md` — current address-space model;
9. `PORTING.md` — board contract and hardware evidence rules.

`CURRENT_STATE.md` is authoritative for what works. `ROADMAP.md` is future intent.

## Current project rule

The project has a complete v0.1 QEMU desktop baseline and is in v0.2
cleanup/runtime hardening. The next product milestone after v0.2 is the v0.3
storage/VFS platform.

Do not skip dependency order:

- do not polish Files/Editor around root-only FAT assumptions that v0.3-v0.4 will
  replace;
- do not call current application polish “v1.1” before v1.0 exists;
- do not broaden the syscall ABI for convenience;
- do not add multimedia, browser, audio, SMP, or game-engine scope to the kernel;
- do not claim Raspberry Pi support;
- do not treat a timeout-only QEMU launch as a passing test;
- do not add global process-visible resources without ownership and cleanup;
- do not make filesystem claims without host image tests and QEMU evidence;
- do not add visible controls that do not perform a real action.

New correctness, ABI, runtime, storage, or evidence regressions are
release-critical until triaged in `TECHNICAL_RISKS.md`.

## Development setup

On Ubuntu or WSL2:

```bash
sudo apt update && sudo apt install -y \
  qemu-system-arm \
  gcc-aarch64-linux-gnu \
  binutils-aarch64-linux-gnu \
  gdb-multiarch \
  make

git clone https://github.com/yourname/armonios
cd armonios
make
```

The default board is `qemu_virt`.

## Evidence levels

Use the exact evidence labels defined in `DOCUMENTATION_POLICY.md`:

- IMPLEMENTED;
- HOST-VERIFIED;
- BUILD-VERIFIED;
- QEMU-VERIFIED;
- CI-VERIFIED;
- MANUAL-VERIFIED;
- UNVERIFIED;
- KNOWN-BROKEN;
- PLANNED.

Never write “all tests pass” when only a subset ran. Never write “works in QEMU”
when only host tests ran. Never convert a serial marker into a visible manual
claim.

## Verification workflow

### Small pure-C change

Begin with the smallest relevant test, then normally run:

```bash
make BOARD=qemu_virt
make BOARD=qemu_virt size
make -C tests test
```

### Userland or library change

```bash
make BOARD=qemu_virt
make BOARD=qemu_virt size
make -C tests test
bash tests/run_kli1_contract_test.sh
make stack-check
```

Run `make qemu-fb-visible` when behavior depends on windows, focus, keyboard,
mouse, redraw, or visible layout.

### Process, syscall, or VFS change

Run the focused contract first, then the full baseline. Relevant focused gates
include:

```bash
bash tests/run_process_parent_wait_test.sh
bash tests/run_vfs_process_fd_test.sh
bash tests/run_user_copy_permissions_test.sh
bash tools/qemu_usercopy_test.sh
```

### Timer, IRQ, runtime-service, input, GUI, USB, or network change

Run:

```bash
bash tests/run_runtime_service_test.sh
make -C tests test
bash tools/qemu_focus_test.sh
bash tools/qemu_marker_test.sh all
```

A timer/IRQ change must also prove:

- the physical handler remains bounded;
- EOI ordering is correct;
- no blocking or unbounded backend operation is reachable from the handler;
- pending work is not lost;
- the exception-context assumptions remain documented;
- latency/fairness claims have stress evidence, not only marker evidence.

### Storage change

Run the relevant host image/parser tests and:

```bash
make qemu-fs-test
bash tools/qemu_fb_fat_test.sh
```

Broader FAT, path, partition, or filesystem claims require dedicated malformed
and persistence tests. The existing root-only smoke test is not general FAT
evidence.

### Raspberry Pi change

At minimum:

```bash
make BOARD=rpi4
make rpi4-emmc2-probe
bash tests/run_board_build_test.sh
```

Before any physical claim, record:

1. exact board revision;
2. firmware and boot files;
3. power, UART, and storage setup;
4. tested image commit/hash;
5. repeatable cold-boot serial evidence;
6. subsystem-specific evidence.

The current RPi4 backend is experimental and must not be used as a known-good
hardware reference.

### Full promotion gate

Before merging code, ABI, build-contract, verified-status, or release changes:

```bash
bash tools/verify.sh
```

The current gate covers:

- QEMU and RPi4 builds;
- `.data == 0` and the 108000-byte limit;
- RPi4 EMMC2, MBR, and block-view host checks;
- native host tests;
- runtime-service coalescing/requeue/EOI ordering;
- parent/wait lifecycle;
- process-local FDs;
- user-copy permissions;
- KLI1 mutable-storage contract;
- userland stack usage;
- FAT32 QEMU smoke;
- usercopy and focus QEMU regressions;
- framebuffer, USB, and DHCP marker gates;
- visible-target FAT + GPU wiring.

## Required pull-request evidence

Every behavior-changing PR should contain:

```text
What changed:
Why:
Affected contracts:

Commands run:
- command -> exact result or marker

Manual checks:
- workflow -> result, tester, environment

Not run:
- check -> reason

Known limitations or follow-up:
- item

Evidence classification:
- IMPLEMENTED / HOST / BUILD / QEMU / CI / MANUAL / HARDWARE
```

Use exact commit SHAs and workflow run IDs for promoted baselines.

## Documentation ownership

The author of a behavior-changing change owns the corresponding documentation.
Update in this order:

1. implementation and focused tests;
2. `TECHNICAL_RISKS.md` when risk state changes;
3. `CURRENT_STATE.md` with exact evidence;
4. architecture, runtime, memory, ABI, or porting docs when contracts change;
5. `ROADMAP.md` when ordering or exit criteria change;
6. `AGENTS.md` when agent operating rules change;
7. README last as a verified summary.

Do not add another status or handoff document. Fix the canonical source.

## Code standards

### Language and runtime

- kernel and userland: freestanding C11;
- boot, exceptions, and context switching: GNU AArch64 assembly in `.S` files;
- no C++ in the kernel or shipping userland;
- no libc, POSIX, or hosted-runtime assumptions;
- keep assembly boundaries narrow and commented.

### Style

- four-space indentation;
- braces on the same line;
- `snake_case` functions and variables;
- `UPPER_SNAKE_CASE` constants;
- `_t` suffix for typedefs where useful;
- `g_` prefix for mutable file-scope globals;
- fixed-width types at ABI and hardware boundaries;
- overflow-safe range arithmetic;
- explicit capacities and failure behavior.

Public headers should document ownership, valid ranges, context restrictions, and
whether a call may run in IRQ/exception context.

## Hard-IRQ and deferred-work rules

Hard-IRQ callbacks may:

- acknowledge/account the event;
- copy a bounded device status value;
- rearm hardware;
- publish a bit or bounded record;
- wake or signal later work.

They must not:

- block or wait;
- allocate from an unbounded/general heap path;
- drain a complete queue;
- scan every device;
- render;
- perform filesystem work;
- parse arbitrary network traffic;
- traverse large dynamic structures.

Post-EOI work is not automatically safe. If it still runs before `eret`, it must
have measured and enforced budgets because EL0 and normal IRQs remain paused.

## Freestanding application rules

Shipping KLI1 images forbid mutable static `.data` and `.bss`:

- no non-zero mutable file-scope application state;
- no large zero-initialized globals;
- use stack only within the measured limit;
- use `SYS_MMAP` for large mutable state;
- keep `tests/run_kli1_contract_test.sh` green.

Prefer reusable `libkarm`/`libkarmdesk` helpers only when they reduce duplication,
clarify ownership, or reduce image size.

## ABI changes

Syscall numbers and user-visible structures are append-only before v1 unless an
explicit incompatibility is approved.

Any ABI change must update:

- kernel constants and dispatch;
- userland wrappers;
- host ABI tests;
- affected applications;
- `SYSCALLS.md` or `GUI_ABI_NOTES.md`;
- `CURRENT_STATE.md` and risk state when behavior changes.

Planned filesystem calls such as `SYS_MKDIR`, `SYS_TRUNCATE`, `SYS_STATX`,
`SYS_READDIRX`, and `SYS_FSINFO` are not current ABI.

Do not silently change:

- syscall numbers or argument registers;
- event IDs;
- public struct layout;
- KLI1 header layout;
- descriptor ownership;
- path or filesystem semantics.

## Board abstraction rules

Generic kernel code must not contain board physical addresses or assume every
board provides virtio.

A board implementation must satisfy the generic contract or return an explicit
safe failure for unsupported optional capabilities. Missing symbols and silent
success stubs are not acceptable.

Capabilities may be enabled only with matching runtime evidence.

## Assembly rules

Follow AAPCS64 and protect shared C/assembly layouts with static assertions where
possible.

Comment:

- exception-level assumptions;
- IRQ mask behavior;
- register ownership;
- stack layout and frame size;
- page-table and TLB barriers;
- exact saved-context layout;
- return semantics.

## Commit and PR discipline

Use focused conventional commits:

```text
type(scope): concise description
```

Common types: `feat`, `fix`, `docs`, `refactor`, `test`, `chore`.

A PR may contain multiple focused commits, but its final tree must have coherent
code, tests, and documentation.

## PR checklist

- [ ] Scope is reviewable and contains no unrelated files.
- [ ] Smallest relevant tests are recorded.
- [ ] Build and size result are recorded when applicable.
- [ ] Stack/KLI1 gates are recorded for userland changes.
- [ ] Deterministic QEMU evidence is recorded for runtime changes.
- [ ] Manual UI/hardware checks name tester and environment.
- [ ] Unrun checks are listed with reasons.
- [ ] Runtime/IRQ changes document execution context and budgets.
- [ ] Risk state is updated.
- [ ] `CURRENT_STATE.md` matches the final tree.
- [ ] ABI and architecture documents match code.
- [ ] README remains a summary rather than an independent source of truth.

## Changes that will not be accepted

- unsupported hardware claims;
- “code exists” presented as runtime evidence;
- timeout-only launches presented as tests;
- unowned global process-visible state;
- raw user-pointer use in lower subsystems;
- heavy or unbounded work in IRQ context;
- hidden ABI breaks;
- broad rewrites without staged tests;
- speculative POSIX/libc scope;
- general filesystem claims based on the root-only FAT bridge;
- placeholder UI controls;
- temporary hacks without a tracked risk or issue.

## Communication

Use issues for defects and scoped work. Use pull requests for reviewable changes.
Keep repository code and technical documentation in English so one canonical set
of contracts exists.

## License

Contributions are licensed under GPL-2.0.
