# ArmoniOS Development Guide

This guide is the practical entry point for continuing development. It explains
how to navigate the repository, choose the correct work sequence, preserve the
kernel's contracts, and produce evidence that can be promoted into the canonical
documents.

It is intentionally not a second status document.

- Current verified behavior: `CURRENT_STATE.md`
- Open defects and accepted boundaries: `TECHNICAL_RISKS.md`
- Milestone ordering: `ROADMAP.md`
- Implemented design: `ARCHITECTURE.md`
- Exact post-EOI contract: `RUNTIME_SERVICE.md`
- Evidence and claim rules: `DOCUMENTATION_POLICY.md`

## 1. Start every work session here

Before changing code:

1. Read `CURRENT_STATE.md` and identify the audited main tree.
2. Read the open risks that affect the subsystem.
3. Confirm the work belongs to the current milestone in `ROADMAP.md`.
4. Read the subsystem contract, not only the implementation file.
5. Identify the smallest focused test that can fail before the fix.
6. Check whether the change affects kernel size, stack, ABI, `.data == 0`, or a
   QEMU/manual claim.

Do not begin broad feature work from an issue title or chat summary alone. Issues
and pull requests are working context; canonical documents define the current
contract.

## 2. Current development boundary

ArmoniOS has a verified v0.1 QEMU desktop baseline. The v0.2 runtime-hardening
implementation and automated stress evidence have landed, but formal v0.2
promotion remains blocked by the explicit residual-risk and release-evidence work
listed in `CURRENT_STATE.md`.

Until v0.2 is promoted:

- keep issue #63 / `RISK-018` isolated from unrelated feature work;
- do not weaken or remove the runtime count, routing, deadline, stress, size, ABI,
  stack, storage, or board gates;
- do not call the tree a release candidate while required P1 dispositions and the
  final visible pass are missing;
- avoid broad v0.3 implementation on `main` unless the remaining v0.2 risk is
  explicitly accepted and documented.

After v0.2 promotion, the next architecture milestone is v0.3: block-device
metadata, path normalization, mount resolution, and structured filesystem
interfaces. Application polish follows those foundations; it does not precede
them.

## 3. Repository map

### Boot and board entry

| Area | Primary files | Responsibility |
|---|---|---|
| AArch64 entry | `boot/start.S` | Exception-level entry, early stack, vector setup, BSS clear, C handoff |
| Kernel orchestration | `kernel/kernel.c` | Ordered subsystem initialization and top-level runtime entry |
| Board contract | `drivers/board.h` | Generic board capabilities and operations |
| QEMU backend | `drivers/boards/qemu_virt/` | Verified GIC, timer, virtio, framebuffer, storage, input, and network wiring |
| Raspberry Pi backend | `drivers/boards/rpi4/` | Build/host-verified fail-closed hardware scaffolding |

Generic kernel code must not contain board physical addresses. Optional board
features must either satisfy the generic contract or fail explicitly.

### Memory and process ownership

| Area | Primary files | Responsibility |
|---|---|---|
| Physical pages | `kernel/mm/pmm.c` | Fixed 128 MiB bitmap allocator and reservation accounting |
| Page tables | `kernel/mm/vmm.c` | AArch64 4 KiB stage-1 tables, mappings, unmaps, and table ownership |
| MMU activation | `kernel/mm/mmu.c` | Translation configuration and TTBR activation |
| Kernel heap | `kernel/mm/kheap.c` | Kernel dynamic allocation |
| Process table | `kernel/process.c` | Fixed process slots, context, parent/zombie lifecycle, user regions, cleanup |
| User mappings | `kernel/user_vm.c` | Anonymous and fixed physical mappings owned by a process |

Ownership rule: mapped leaf pages and page-table pages have different owners.
Process cleanup must release each exactly once. Any VMM, PMM, process-exit, mmap,
or loader change must preserve that distinction and add evidence for rollback and
reclamation paths.

### Exceptions, scheduling, and runtime work

| Area | Primary files | Responsibility |
|---|---|---|
| Exception vectors | `kernel/exception_vectors.S`, `kernel/irq_asm.S` | Trap-frame layout and entry/return assembly |
| EL1/EL0 faults | `kernel/exceptions.c` | Fatal EL1 diagnostics and lower-EL fault handling |
| IRQ dispatch | `kernel/irq.c` | Handler dispatch and the post-EOI runtime service |
| Timer | `kernel/timer/timer.c` | Fixed accounting, rearm, readiness publication, scheduler counters |
| EL0 dispatch | `kernel/process.c` | Save/restore and round-robin process selection |
| EL1 helpers | `kernel/sched/` | Cooperative kernel helper threads |

The runtime model is:

```text
preemptive EL0
  -> IRQ entry
  -> bounded hard callback
  -> EOI
  -> count- and time-bounded post-EOI EL1 pass
  -> process selection
  -> eret

plus separate cooperative EL1 helper threads
```

EOI is not exception return. During the post-EOI pass, EL0 is paused, the
exception frame remains on the EL1 stack, and normal IRQs remain masked by the
entry state. See `RUNTIME_SERVICE.md` before modifying this path.

### Syscalls and ABI

| Area | Primary files | Responsibility |
|---|---|---|
| Numbers | `kernel/syscall_numbers.h` | Append-only syscall numbering |
| Dispatch | `kernel/syscall.c` | Register ABI and syscall routing |
| Safe copies | `kernel/syscall_helpers.c` | User-range, page-permission, string, argv, input, and output handling |
| Domains | `kernel/syscall_*.c` | Process, GUI, IPC, information, and VFS operations |
| User wrappers | `programs/libkarm/` | Freestanding syscall trampolines and small runtime helpers |
| GUI wrappers | `programs/libkarmdesk/` | Typed userland GUI interface |

Do not pass raw user pointers into lower subsystems. Import input into
kernel-owned storage and build output in kernel-owned buffers before copying.
Validate the complete destination before consuming queued state.

### Storage and VFS

| Area | Primary files | Responsibility |
|---|---|---|
| Generic VFS | `kernel/vfs.c` | Mount selection, process-local descriptors, offsets, and ownership |
| Embedded apps | `kernel/bootfs.c`, `kernel/boot_program.c` | `/armonios` KLI1 images |
| In-memory FS | `kernel/tmpfs.c` | Fixed-capacity test and temporary storage |
| FAT bridge | `kernel/fat32.c`, `kernel/fat32_vfs.c` | Writable root-only FAT32 8.3 workflow |
| Block devices | `drivers/storage/` | Virtio block, EMMC2 scaffolding, MBR, and bounded views |

The current FAT implementation is a compatibility bridge, not the v0.4 general
FAT design. v0.3 must first establish generic block metadata, normalized paths,
mount-boundary behavior, and structured metadata/directory interfaces.

### GUI, input, and applications

| Area | Primary files | Responsibility |
|---|---|---|
| Window lifecycle | `kernel/gui_pool.c` | Fixed window pool, ownership, z-order, focus |
| Input routing | `kernel/gui_input.c`, `kernel/gui_events.c` | Hit testing and per-window event queues |
| Backing storage | `kernel/gui_backing.c` | Lazily allocated window content buffers |
| Damage/compositor | `kernel/gui_damage.c`, `kernel/gui_compositor.c` | Ordered damage, partial batches, full redraw, submission |
| Device input | `drivers/input/`, `drivers/usb/` | UART, virtio-input, xHCI, keyboard, and mouse producers |
| Applications | `programs/apps/` | Panel, Shell, Editor, Files, Monitor, Control, Clock |

Shipping KLI1 images may not contain mutable static `.data` or `.bss`. Large
mutable state belongs in `SYS_MMAP`; userland stack usage remains gated.

### Networking

| Area | Primary files | Responsibility |
|---|---|---|
| Virtio RX/TX | `drivers/net/virtio_net.c` | Device queues and bounded frame receive |
| DHCP | `kernel/net/dhcp.c`, `kernel/net/dhcp_options.c` | Minimal lease acquisition |
| Runtime routing | `kernel/irq.c`, `kernel/io_service.h` | Strict NETWORK-phase polling and receive |

The stack currently supports enough Ethernet, ARP, IPv4, UDP, and DHCP for a QEMU
lease. It does not expose sockets, TCP, DNS, HTTP, or a general user UDP API.
Consumed-frame counters do not prove absence of device/ring drops.

## 4. Non-negotiable invariants

Every change must preserve the invariants relevant to its scope:

- freestanding C11 and narrow AArch64 assembly boundaries;
- `.data == 0` for the production kernel and shipping KLI1 images;
- kernel W^X mapping policy;
- 108000-byte production kernel ceiling unless a deliberate replacement budget is
  approved and documented;
- 3072-byte userland stack ceiling unless intentionally revised with evidence;
- append-only syscall numbers and stable KLI1/public structure layouts;
- process ownership and cleanup for descriptors, pages, windows, IPC, and mappings;
- fail-closed behavior for unverified Raspberry Pi capabilities;
- deterministic pass/fail assertions for QEMU gates;
- exact separation between host, build, QEMU, manual, and hardware evidence.

Runtime-specific invariants:

- the physical timer callback performs only fixed accounting, rearm, publication,
  and scheduler-counter work;
- virtio-input processes at most one negotiated ring length and no more than 16
  descriptors per call;
- USB HID visits at most four fixed device slots per call;
- shared input consumption is capped at 16 events per active pass;
- partial redraw consumes at most eight ordered damage rectangles after a
  successful submission;
- network polling and receive consume nothing outside the active NETWORK phase;
- valid network RX is capped at 16 frames per NETWORK pass and requeued at cap;
- the service-wide deadline is checked at safe boundaries and republishes original
  readiness on expiry;
- no documentation may reinterpret cooperative checkpoints as asynchronous
  preemption.

## 5. Choose the correct change workflow

### Pure host-testable logic

Examples: parsers, range arithmetic, path normalization, metadata conversion,
small ownership helpers.

1. Add a focused host test.
2. Run the focused test.
3. Run `make -C tests test`.
4. Build QEMU and enforce size if the code enters the kernel image.
5. Run the full gate before promotion.

### Memory, process, or syscall changes

1. Add failure, rollback, ownership, and cleanup tests.
2. Run the relevant process, user-copy, VFS, or KLI1 focused gates.
3. Build and inspect kernel size.
4. Run QEMU user-copy or lifecycle evidence when exception/user mappings change.
5. Update `MEMORY_MAP.md` or `SYSCALLS.md` when the contract changes.
6. Update the risk register for any new fatal-EL1 or isolation boundary.

### Runtime, IRQ, GUI, input, USB, or network changes

1. Prove the hard callback remains fixed and bounded.
2. Add a host regression for stop/continuation semantics.
3. Preserve explicit metrics for completed work.
4. Add or update deterministic QEMU evidence.
5. Add stress evidence for latency, backlog, overflow, or fairness claims.
6. Run both runtime stress modes when the shared service contract changes.
7. Record what remains unobservable; do not infer loss absence.

### Storage or filesystem changes

1. Start with host fixtures, malformed inputs, and bounded arithmetic.
2. Keep filesystem policy behind mount/filesystem callbacks.
3. Test read-only, mount-boundary, and rollback behavior.
4. Run `make qemu-fs-test` and the FAT + GPU wiring gate.
5. Add a reboot/persistence gate before claiming durable behavior.
6. Do not upgrade root-only 8.3 evidence into a general FAT claim.

### Userland or visible GUI changes

1. Keep KLI1 and stack gates green.
2. Test event IDs, ownership, and failure behavior at the ABI boundary.
3. Run deterministic focus/marker gates where possible.
4. Run `make qemu-fb-visible` for layout, input, focus, or workflow claims.
5. Record tester, date, exact commit/image, steps, and limitations.

### Raspberry Pi changes

1. Preserve the generic board contract and fail-closed normal capabilities.
2. Run the board build, EMMC2 probe package, and host diagnostic tests.
3. Treat physical evidence as a separate track.
4. Never infer hardware support from source presence or successful cross-builds.

## 6. Verification commands

Start focused, finish with the complete gate.

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

Full automated promotion gate:

```sh
bash tools/verify.sh
```

Visible evidence, run separately:

```sh
make qemu-fb-visible
```

The full gate is necessary but not sufficient for a visible or physical claim.

## 7. Pull-request shape

Prefer one reviewable contract per PR. A good PR contains:

```text
Problem:
- exact defect, missing contract, or milestone slice

Change:
- implementation and ownership boundaries

Evidence:
- focused command -> result
- full command/workflow -> result
- manual workflow -> tester/date/commit, when applicable

Not proved:
- explicit evidence boundary

Risks and documentation:
- risk IDs changed
- canonical documents updated
```

Use exact SHAs and run IDs only after the final tree is known. A PR-head run proves
the tree it exercised; it does not automatically prove a later conflict-resolved
merge tree.

## 8. Documentation update order

For behavior-changing work:

1. implementation and focused tests;
2. `TECHNICAL_RISKS.md` when risk state changes;
3. `CURRENT_STATE.md` with exact evidence;
4. the affected architecture/runtime/memory/ABI/porting contract;
5. `ROADMAP.md` if sequencing or exit criteria changed;
6. this guide, `AGENTS.md`, or `CONTRIBUTING.md` if operating rules changed;
7. README last.

Do not create another current-state, audit, handoff, or latest-status document.

## 9. Recommended v0.3 slicing

After formal v0.2 promotion, avoid one large storage rewrite. Use staged cuts:

1. **Block descriptor contract**
   - sector size, capacity, identity, read-only state, read/write/flush operations;
   - host tests for invalid geometry, overflow, read-only behavior, and flush.
2. **Path normalizer**
   - absolute paths, repeated slashes, component bounds, `.` and `..` policy;
   - pure host tests before VFS integration.
3. **Mount resolver**
   - longest valid mount-prefix selection and mount-boundary semantics;
   - tests for `/`, `/fat`, `/tmp`, `/armonios`, and future `/ext`.
4. **Structured metadata and directory ABI**
   - kernel-internal types first;
   - append-only syscall numbers only when implementation, wrappers, tests, and
     documentation can land together.
5. **Compatibility migration**
   - keep the existing v0.1 FAT workflow working through the new resolver;
   - prevent FAT-specific policy from returning to generic VFS code.

Each cut must remain buildable, testable, and reversible without depending on
unfinished later slices.

## 10. Definition of done

A change is done only when:

- the implementation and ownership model are coherent;
- focused tests fail before and pass after the change where practical;
- relevant size, stack, ABI, and `.data` gates pass;
- the full automated gate passes on the final tree;
- visible or hardware claims have separate dated evidence;
- known limitations remain explicit;
- risks and canonical documentation match the final tree;
- the PR states what was not proved.
