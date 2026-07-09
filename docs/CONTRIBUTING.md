# Contributing to ArmoniOS

First: thank you for considering contributing. ArmoniOS is built on the idea
that an OS can be understood by a single person — and that only works if the
code stays clean, minimal, and well-documented.

---

## Before You Start

Read these documents first:

1. [README.md](../README.md) — project overview and philosophy
2. [ARCHITECTURE.md](ARCHITECTURE.md) — how the system is structured
3. [CURRENT_STATE.md](CURRENT_STATE.md) — live baseline and known product gaps
4. [ROADMAP.md](ROADMAP.md) — release order and gates
5. [SYSCALLS.md](SYSCALLS.md) — ABI reference for userland/kernel boundaries

If you're new to OS development, the [OSDev Wiki](https://wiki.osdev.org) and
the [ARM Architecture Reference Manual](https://developer.arm.com/documentation/ddi0487)
are the two most important external references.

---

## Current Project Rule

ArmoniOS is currently stabilizing the QEMU desktop baseline. Unless the roadmap
says otherwise, favor small, testable changes that preserve the current desktop
flow over speculative new subsystems.

For v1.0-bound work:

- Keep QEMU `virt` stable and repeatable.
- Do not claim Raspberry Pi hardware support until a real board reaches a
  documented serial/boot milestone.
- Do not introduce POSIX, libc, hosted runtimes, or large external dependencies.
- Do not renumber syscalls or alter GUI event layouts.
- Keep kernel binary size under `KERNEL_SIZE_LIMIT`.
- Update docs and tests in the same change when a public ABI, build target, or
  user-visible workflow changes.

---

## What We Need Help With

Check the issue tracker for open issues tagged:

- `good first issue` — well-scoped tasks, good starting points
- `driver` — hardware driver work
- `kernel` — core kernel changes
- `docs` — documentation improvements
- `testing` — QEMU test cases and verification

Things we always need:

- Bug reports with minimal reproduction steps
- Documentation fixes for unclear or stale behavior
- Host tests for pure C logic
- QEMU-reproducible runtime checks
- Small driver improvements that do not destabilize the baseline

---

## Development Setup

See the [README](../README.md#building) for the full setup. Short version:

```bash
# WSL2 / Ubuntu
sudo apt update && sudo apt install -y \
  qemu-system-arm \
  gcc-aarch64-linux-gnu \
  binutils-aarch64-linux-gnu \
  gdb-multiarch \
  make

git clone https://github.com/yourname/armonios
cd armonios
make
make help
```

The default board is `qemu_virt`. For most development, start with:

```bash
make
make size
make -C tests test
```

For changes that affect kernel, drivers, boot, syscalls, app images, storage,
input, networking, GUI, or user-visible desktop behavior, run the release gates
from `docs/ROADMAP.md`:

```bash
make
make size
make -C tests test
make stack-check
make qemu-fs-test
timeout 25s make qemu-fb
timeout 25s make qemu-usb
timeout 25s make qemu-net
```

Before a release tag, also run one visible desktop pass:

```bash
make qemu-fb-visible
```

---

## Code Standards

### Language

- Kernel core: **C11** only. No C++.
- Boot and context switch: **AArch64 ASM** (GNU assembler syntax, `.S` files).
- Kernel and userland code stay freestanding. Use the project's own helpers
  instead of assuming libc/POSIX behavior.
- Rust, Lua, scripting runtimes, and other hosted runtimes do not belong in the
  kernel.

### Style

```c
// Functions: snake_case
void pmm_init(uint64_t base, uint64_t size);

// Types: snake_case with _t suffix
typedef struct process process_t;

// Constants and macros: UPPER_SNAKE_CASE
#define PAGE_SIZE  4096
#define MAX_PROCS  256

// Global mutable state: g_ prefix
static uint32_t g_proc_count = 0;

// No typedef for structs unless it adds clarity
struct process { ... };           // OK
typedef struct process process_t;  // also OK, use consistently
```

### Formatting

- 4-space indentation. No tabs.
- Opening brace on the same line: `if (x) {`
- Maximum line length: 100 characters where practical.
- Every public function in a `.h` file gets a doc comment when it is part of a
  public kernel, driver, board, or userland contract.

```c
/**
 * pmm_alloc_page - Allocate a single 4KB physical page frame.
 *
 * Returns the physical address of the allocated frame,
 * or 0 if no memory is available.
 */
uint64_t pmm_alloc_page(void);
```

### Assembly

- Use `.S` (uppercase) so the C preprocessor runs on it.
- Comment every non-obvious instruction.
- Follow AAPCS64: `x0`–`x7` args, `x0` return, `x8`–`x15` caller-saved,
  `x19`–`x28` callee-saved.
- Save/restore callee-saved registers if your function uses them.

### ABI Changes

- Syscall numbers are frozen in `kernel/syscall_numbers.h`.
- `SYSCALLS.md` is the authoritative human-readable syscall reference.
- Window/compositor wrappers live in `programs/libkarmdesk`; process, memory,
  I/O, IPC, and system-info wrappers live in `programs/libkarm`.
- Any syscall, struct layout, event id, flag, or wrapper change must update the
  relevant tests and docs in the same commit.

### Commit Messages

Follow [Conventional Commits](https://www.conventionalcommits.org/):

```
type(scope): short description (max 72 chars)

Optional longer body explaining WHY, not what.
```

Types:

- `feat` — new feature
- `fix` — bug fix
- `docs` — documentation only
- `refactor` — code restructuring, no behavior change
- `test` — adding or fixing tests
- `chore` — build system, toolchain, CI

Examples:

```
feat(mm): add bitmap physical memory allocator

fix(uart): handle TX FIFO full condition in putc

docs(arch): clarify page table layout for user space

feat(sched): implement round-robin preemptive scheduler
```

---

## Pull Request Process

1. Fork the repository and create a branch: `git checkout -b feat/your-feature`.
2. Write your code following the standards above.
3. Run the smallest relevant tests first, then the roadmap gates for broad
   kernel/driver/ABI/desktop changes.
4. Document any new public APIs or user-visible behavior.
5. Open a PR with:
   - a clear title following the commit convention;
   - a description of what changed and why;
   - exact commands used for verification;
   - any open questions or known limitations.

### PR Checklist

- [ ] `make` passes without warnings
- [ ] `make size` passes and stays below `KERNEL_SIZE_LIMIT`
- [ ] `make -C tests test` passes
- [ ] `make stack-check` passes when userland apps or wrappers changed
- [ ] Relevant QEMU smoke target passes (`qemu-fb`, `qemu-fs-test`, `qemu-usb`,
      `qemu-net`, etc.)
- [ ] One `make qemu-fb-visible` manual pass was run for user-visible desktop
      changes
- [ ] No libc/POSIX assumptions were introduced
- [ ] New public functions or ABI changes are documented
- [ ] `SYSCALLS.md`, wrappers, and ABI tests are in sync for syscall changes
- [ ] Commit messages follow the convention
- [ ] No unrelated changes are included

---

## What We Won't Accept

- C++ in the kernel
- POSIX compatibility layers
- libc or hosted runtime assumptions in kernel/userland
- Pulling in external libraries without a strong reason and explicit discussion
- Code that requires Linux-specific runtime features to compile the target
- "Temporary" hacks without a corresponding issue tracking the cleanup
- Raspberry Pi support claims without a real hardware milestone
- Changes that increase kernel binary size significantly without a proportional
  feature gain

---

## Communication

- Issues: for bugs, feature requests, and tasks
- Discussions: for design questions and open-ended conversation
- Keep communication in English while the codebase and docs are English

---

## License

By contributing, you agree that your code will be licensed under
[GPL-2.0](../LICENSE).
