# Contributing to ArmoniOS

ArmoniOS is intended to remain small enough that one developer can understand the whole system. That requires disciplined scope, explicit evidence, and documentation that never upgrades intention into fact.

## Read first

Read these documents in order:

1. `DOCUMENTATION_POLICY.md` — how claims and verification are recorded;
2. `CURRENT_STATE.md` — audited operational status;
3. `TECHNICAL_RISKS.md` — active correctness and release risks;
4. `ROADMAP.md` — work order and release exit criteria;
5. `ARCHITECTURE.md` — current implementation model;
6. `SYSCALLS.md` and `GUI_ABI_NOTES.md` — user/kernel ABI;
7. `MEMORY_MAP.md` — current address and translation model;
8. `PORTING.md` — board contract and hardware rules.

## Current project rule

The project is stabilizing the v0.1 QEMU desktop baseline. Correctness, deterministic verification, and small release hygiene fixes take priority over new features.

- do not broaden the kernel ABI for convenience;
- do not begin multimedia or game-engine work in the kernel;
- do not claim Raspberry Pi support;
- do not treat a timeout-only QEMU launch as a passing test;
- do not describe user pointers as writable unless permissions are enforced;
- do not add more global process-visible resources.

The v0.1 QEMU blockers are closed on the documented baseline. Treat new correctness, ABI, or evidence regressions as release-critical until triaged in `TECHNICAL_RISKS.md`.

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

## Verification levels

Use the evidence labels defined in `DOCUMENTATION_POLICY.md`.

### Fast local checks

For documentation-neutral pure C changes, begin with the smallest relevant test and then run:

```bash
make
make size
make -C tests test
```

### Userland changes

When applications or userland libraries change:

```bash
make
make size
make -C tests test
make stack-check
```

Run the visible desktop when behavior can be observed only through windows or input.

### Kernel, ABI, VFS, storage, GUI, driver, or boot changes

Run:

```bash
make
make size
make -C tests test
make stack-check
make qemu-fs-test
```

Then run the relevant deterministic QEMU tests. Plain framebuffer, USB, and network launch targets are not sufficient by themselves because they do not assert final markers. Use `tools/qemu_marker_test.sh`, `tools/qemu_usercopy_test.sh`, `tools/qemu_focus_test.sh`, `tools/qemu_fb_fat_test.sh`, or `bash tools/verify.sh` for pass/fail evidence.

### Raspberry Pi changes

Before any hardware claim:

1. `make BOARD=rpi4` must compile and link;
2. the exact physical board and firmware setup must be recorded;
3. a serial milestone must be reproduced;
4. later subsystem claims must have their own evidence.

The current RPi4 backend is experimental and should not be used as a known-good driver reference.

## Required pull-request evidence

Every PR must contain:

```text
What changed:
Why:
Affected contracts:

Commands run:
- command -> exact result/marker

Manual checks:
- workflow -> result and tester

Not run:
- check -> reason

Known limitations or follow-up:
- item
```

Never write “all tests pass” when only a subset was run. Never write “works in QEMU” when only host tests ran.

## Documentation ownership

The author of a behavior-changing change owns the corresponding documentation update.

Update in this order:

1. tests and implementation;
2. `TECHNICAL_RISKS.md` when risk state changes;
3. `CURRENT_STATE.md` with exact evidence;
4. architecture, memory, ABI, or porting documents when contracts changed;
5. `ROADMAP.md` when ordering or exit criteria changed;
6. `README.md` last.

## Code standards

### Language and runtime

- Kernel and userland: freestanding C11.
- Boot, exceptions, and context switching: GNU AArch64 assembly in `.S` files.
- No C++ in the kernel or shipping userland.
- No libc, POSIX, or hosted-runtime assumptions.
- Keep assembly boundaries narrow and commented.

### Style

- 4-space indentation;
- braces on the same line;
- `snake_case` functions and variables;
- `UPPER_SNAKE_CASE` constants;
- `_t` suffix for typedef names where useful;
- `g_` prefix for mutable file-scope globals;
- fixed-width integer types at ABI and hardware boundaries;
- overflow-safe range arithmetic for addresses and sizes.

Public contracts in headers should explain ownership, valid ranges, failure behavior, and whether calls may run in IRQ context.

### Freestanding application rules

The current KLI1 contract forbids ordinary mutable `.data` and `.bss` in shipping app images:

- do not add non-zero mutable file-scope application data;
- do not rely on large zero-initialized application globals;
- use stack storage only within the measured stack limit;
- use anonymous mappings for large mutable application state;
- keep `tests/run_kli1_contract_test.sh` passing when adding new storage patterns.

### Assembly

Follow AAPCS64 and keep C/assembly layouts protected by static assertions where possible.

Comment:

- exception-level assumptions;
- register ownership;
- stack layout;
- page-table or TLB barriers;
- exact saved-context layout.

## ABI changes

Syscall numbers and user-visible structures are frozen unless an intentional ABI extension is approved.

Any ABI change must update:

- kernel constants and dispatch;
- userland wrappers;
- host ABI tests;
- shipping applications affected;
- `SYSCALLS.md` or `GUI_ABI_NOTES.md`;
- `CURRENT_STATE.md` if behavior or evidence changed.

Do not silently change:

- syscall numbers;
- argument registers;
- event IDs;
- struct field order or width;
- KLI1 header layout;
- descriptor semantics.

## Board abstraction rules

Generic kernel code should not contain board physical addresses.

A board implementation must satisfy the complete generic contract or return explicit safe failures for unsupported optional capabilities. Do not leave required symbols missing.

Prefer capability-neutral generic names. New generic code should not assume that every physical board exposes virtio-mmio or virtio-input.

## Commit messages

Use conventional commits:

```text
type(scope): concise description
```

Common types:

- `feat`
- `fix`
- `docs`
- `refactor`
- `test`
- `chore`

Keep commits focused. Documentation recovery and behavior changes may be separate commits, but a PR cannot merge with contradictory final documentation.

## PR checklist

- [ ] Scope is small enough to review.
- [ ] No unrelated files are included.
- [ ] `make` result recorded.
- [ ] `make size` result recorded for kernel or embedded-app changes.
- [ ] Relevant host tests recorded.
- [ ] `make stack-check` recorded for userland changes.
- [ ] Relevant deterministic QEMU evidence recorded.
- [ ] Manual UI or hardware steps name the tester and environment.
- [ ] Unrun checks are listed explicitly.
- [ ] New risks or changed risk status are recorded.
- [ ] `CURRENT_STATE.md` was updated when evidence changed.
- [ ] ABI documents match the code.
- [ ] README claims remain a summary of verified documents.

## Changes that will not be accepted

- unsupported hardware claims;
- code-present-equals-working documentation;
- timeout-only tests presented as passing subsystem tests;
- new global process-visible state without ownership and cleanup design;
- unsafe user-pointer dereferences without a documented boundary;
- broad rewrites without staged tests;
- hidden ABI breaks;
- speculative POSIX/libc or large runtime additions;
- temporary hacks without a tracked risk or issue.

## Communication

Use issues for defects and scoped work. Use pull requests for reviewable changes. Keep repository code and technical documentation in English so one canonical set of contracts exists.

## License

Contributions are licensed under GPL-2.0.
