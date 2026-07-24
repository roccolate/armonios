# ArmoniOS Development Guide

This is the practical guide for changing ArmoniOS without violating its current
contracts. It is not a second status document.

Read first:

- current implementation: `CURRENT_STATE.md`;
- implemented design: `ARCHITECTURE.md`;
- future ordering: `ROADMAP.md`;
- active risks: `TECHNICAL_RISKS.md`;
- documentation ownership: `README.md`;
- exact runtime contract: `RUNTIME_SERVICE.md`;
- public syscalls: `SYSCALLS.md`.

## 1. Start every work session from current main

```sh
git switch main
git fetch origin --prune
git pull --ff-only origin main
git status
```

Before changing code:

1. identify the current capability or limitation in `CURRENT_STATE.md`;
2. read the architecture and focused reference for the subsystem;
3. check whether an active risk constrains the work;
4. confirm the cut belongs to the current roadmap order;
5. identify the smallest focused test that can fail before the change;
6. list the invariants affected: ABI, ownership, rollback, image size, stack,
   `.data`, QEMU behavior, or hardware claim;
7. keep the change small enough to explain and validate independently.

Do not begin from a stale branch, an old PR description, or a chat summary alone.
Code and permanent tests are the first source of truth; canonical documentation
explains the intended current contract.

## 2. Current development boundary

The v0.2 implementation is complete. Issue #76 remains a manual visible
validation and release-record task, not missing kernel implementation.

The main implementation sequence is now:

1. complete seek and truncate;
2. add mkdir/rmdir and nested mutation rollback;
3. add explicit durability and reboot-persistence evidence;
4. add VFAT long names;
5. continue consumer-driven `libkarm` and `libarmdesk` work;
6. expand applications into the v1 workflow;
7. add ext2 read-only;
8. stabilize for beta.

Do not regress the v0.2 runtime, IRQ-origin, VMM-soak, size, ABI, stack, storage,
or board gates while working on later phases.

## 3. Repository map

### Boot and boards

| Area | Primary paths | Responsibility |
|---|---|---|
| AArch64 entry | `boot/start.S` | Early stack, vectors, BSS clear, C handoff |
| Kernel orchestration | `kernel/kernel.c` | Ordered subsystem initialization |
| Generic board contract | `drivers/board.h` | Optional capabilities and board operations |
| QEMU backend | `drivers/boards/qemu_virt/` | Verified runtime wiring |
| Raspberry Pi backend | `drivers/boards/rpi4/` | Build/diagnostic fail-closed scaffolding |

Generic code must not embed board physical addresses. An optional board feature
must satisfy the generic contract or return explicit unsupported/failure state.

### Memory and processes

| Area | Primary paths | Responsibility |
|---|---|---|
| Physical pages | `kernel/mm/pmm.c` | Fixed bitmap allocator and reservations |
| Page tables | `kernel/mm/vmm.c` | AArch64 mappings and page-table ownership |
| MMU activation | `kernel/mm/mmu.c` | Translation configuration and TTBR activation |
| Kernel allocation | `kernel/mm/kheap.c` | Explicit kernel dynamic allocation |
| Processes | `kernel/process.c` | Slots, context, parent/wait, zombies, cleanup |
| User mappings | `kernel/user_vm.c` | Process-owned anonymous/fixed mappings |

Mapped leaf pages and page-table pages have different owners. Any change to
process exit, VMM, PMM, mmap, loader, or rollback paths must prove each resource
is released exactly once.

### Exceptions and runtime work

| Area | Primary paths | Responsibility |
|---|---|---|
| Exception vectors | `kernel/exception_vectors.S`, `kernel/irq_asm.S` | Frame layout, origin gate, entry/return |
| Fault handling | `kernel/exceptions.c` | EL1 diagnostics and lower-EL faults |
| IRQ/runtime dispatch | `kernel/irq.c` | Handler routing and post-EOI service |
| Timer | `kernel/timer/` | Fixed callback, rearm, readiness, counters |
| EL0 scheduling | `kernel/process.c` | Context save/restore and selection |
| EL1 helpers | `kernel/sched/` | Cooperative helper execution |

Runtime model:

```text
preemptive EL0
  -> IRQ vector
  -> classify saved PSTATE origin
  -> fixed hard callback
  -> EOI
  -> count- and time-bounded post-EOI EL1 service
  -> process selection only for a valid EL0 frame
  -> eret
```

An IRQ that interrupted EL1 must never enter process save/preemption or switch
TTBR0. EOI is not exception return; EL0 remains paused during the service pass.
Read `RUNTIME_SERVICE.md` before modifying this path.

### Public ABI and syscalls

| Area | Primary paths | Responsibility |
|---|---|---|
| Public ABI | `include/armonios/abi/` | Shared numbers, errors, flags, and layouts |
| Compatibility headers | `kernel/syscall_numbers.h`, older aliases | Temporary in-tree compatibility |
| Dispatcher | `kernel/syscall.c` | Register ABI and routing |
| Copy helpers | `kernel/syscall_helpers.c` | Range, permission, string, argv, input/output checks |
| Domain handlers | `kernel/syscall_*.c` | Process, VFS, GUI, IPC, and information services |
| User wrappers | `programs/libkarm/` | Typed freestanding ABI access |
| Desktop wrappers | `programs/libarmdesk/` | Canonical GUI-facing layer |

Rules:

- userland must not include kernel-private headers for public values;
- lower subsystems must not receive raw EL0 pointers;
- import input into kernel-owned storage;
- assemble output in kernel-owned storage;
- validate the complete destination before consuming state or invoking a provider;
- never renumber or reuse an existing syscall or public error value;
- add implementation, wrapper, tests, consumer, and documentation together.

### Storage and VFS

| Area | Primary paths | Responsibility |
|---|---|---|
| Generic VFS | `kernel/vfs.c` and focused VFS modules | Canonical paths, mounts, descriptors, offsets |
| bootfs | `kernel/bootfs.c`, `kernel/boot_program.c` | Embedded `/armonios` application images |
| tmpfs | `kernel/tmpfs.c` | Fixed-capacity temporary storage |
| FAT32 | `kernel/fat32*.c` | Geometry, directories, root mutation, VFS adapter |
| Block layer | `drivers/storage/` | Devices, MBR, views, QEMU/RPi adapters |
| Metadata/fsinfo | VFS metadata and fsinfo modules | Native records and public adapters |

The current FAT boundary is nested read traversal with root-only mutation. Do not
call it general FAT. Keep canonical path, mount selection, and generic error
policy outside FAT-specific code.

The next storage cuts must define rollback before implementation. Truncate,
mkdir/rmdir, rename/move, directory allocation, and flush ordering can corrupt
persistent state if partial progress is not explicit.

### GUI, input, and applications

| Area | Primary paths | Responsibility |
|---|---|---|
| Window lifecycle | `kernel/gui_pool.c` | Fixed pool, ownership, z-order, focus |
| Input routing | `kernel/gui_input.c`, `kernel/gui_events.c` | Hit testing and event queues |
| Backing storage | `kernel/gui_backing.c` | Window content buffers |
| Damage/compositor | `kernel/gui_damage.c`, `kernel/gui_compositor.c` | Ordered damage and GPU submission |
| Device input | `drivers/input/`, `drivers/usb/` | UART, virtio, xHCI, keyboard, mouse |
| Applications | `programs/apps/` | Panel, Shell, Editor, Files, Monitor, Control, Clock |
| Desktop layer | `programs/libarmdesk/` | Wrappers, geometry, theme, future models |

Shared widgets must keep neutral state separate from syscall drawing adapters.
Prefer caller-owned bounded models with host tests. Measure application binary and
stack deltas before promoting a component into shipping apps.

### Userland runtime

| Area | Primary paths | Responsibility |
|---|---|---|
| Startup | `programs/libkarm/crt0.*` | `_start`, argc/argv, exit |
| Static archive | `programs/libkarm/` and `Makefile` | Reusable GUI-independent runtime |
| Arena | arena sources/headers | Monotonic caller-owned allocation |
| Buffer/string | buffer sources/headers | Growable binary and terminated text storage |
| File helpers | file sources/headers | Complete writes and rollback-safe reads |

`crt0.o` remains explicit; reusable code belongs in `libkarm.a`. Keep function and
data sections so unused archive members can be removed by `--gc-sections`.

Do not add hidden mutable runtime globals. Arena reset/destroy invalidates all
objects backed by that arena. File helpers must preserve descriptor cleanup and
exact arena rollback on failure.

### Networking

| Area | Primary paths | Responsibility |
|---|---|---|
| Virtio network | `drivers/net/virtio_net.c` | Device queues and bounded RX/TX |
| Protocols | `kernel/net/` | Ethernet, ARP, IPv4, UDP, DHCP |
| Runtime routing | IRQ/runtime-service code | NETWORK-phase-only polling and receive |

The current stack exists to support a QEMU DHCP lease. It does not expose sockets,
TCP, DNS, HTTP, or a general user UDP API. Do not infer absence of device drops
from consumed-frame counters.

## 4. Non-negotiable invariants

Apply the relevant subset to every change:

- freestanding C11 and narrow AArch64 assembly;
- QEMU and Raspberry Pi remain separate board products;
- production loadable `.data` remains empty;
- kernel W^X remains enforced;
- production image remains below **128 KiB / 131072 bytes**;
- userland stack gates remain enforced;
- public syscall values and layouts do not drift;
- process ownership and teardown remain exactly-once;
- raw user pointers do not enter lower subsystems;
- unsupported optional capabilities fail closed;
- deterministic tests contain explicit pass/fail assertions;
- host, build, QEMU, manual, and hardware evidence remain distinct.

Runtime invariants:

- the hard timer callback stays fixed and bounded;
- EL1-origin IRQs cannot enter process preemption;
- virtio input is bounded by the ring and a cap of 16;
- USB HID visits no more than four fixed slots;
- shared input consumes at most 16 events per active pass;
- partial redraw consumes at most eight ordered rectangles per successful submit;
- network work consumes nothing outside NETWORK phase;
- network RX consumes at most 16 valid frames per NETWORK pass;
- deadline expiry republishes original work and skips later optional work;
- cooperative checkpoints are not described as asynchronous preemption.

Storage invariants:

- all block ranges are overflow-checked and bounded;
- read-only state propagates through views and mounts;
- unsupported flush is explicit;
- generic VFS does not contain FAT path policy;
- failure leaves descriptor offset and persistent metadata in the documented
  state;
- durability is not claimed without reboot evidence.

## 5. Choose the correct workflow

### Pure host-testable logic

Examples: arithmetic, parsers, path normalization, metadata conversion, rectangle
models, buffers, strings, and ownership helpers.

1. write a focused failing host test;
2. implement the smallest change;
3. run the focused test;
4. run the wider host suite;
5. build QEMU if the code enters production artifacts;
6. measure image or application delta;
7. finish with the complete gate.

### Memory, process, exception, or syscall changes

1. document ownership and rollback before coding;
2. test invalid inputs, partial initialization, cleanup, and reuse;
3. run process, user-copy, ABI, or VMM focused gates;
4. inspect image size and `.data`;
5. run QEMU exception/lifecycle evidence where relevant;
6. update `MEMORY_MAP.md`, `SYSCALLS.md`, architecture, or risks as needed;
7. finish with the complete matrix.

### Runtime, GUI, input, USB, or network changes

1. prove hard-IRQ work remains fixed;
2. identify count, time, queue, and continuation boundaries;
3. add host regressions for stop/resume semantics;
4. preserve useful metrics;
5. add deterministic QEMU evidence;
6. run stress tests for backlog, overflow, latency, or fairness claims;
7. state what remains unobservable;
8. run both runtime stress modes when the shared service changes.

### Storage or filesystem changes

1. define errors, partial progress, rollback, and read-only behavior first;
2. add pure host fixtures and malformed/capacity cases;
3. keep policy behind generic block/mount/filesystem interfaces;
4. test descriptor offsets and state preservation on failure;
5. run FAT32 headless and framebuffer wiring gates;
6. add a real EL0 consumer for new public behavior;
7. add reboot testing before claiming durability;
8. update public headers, `SYSCALLS.md`, state, architecture, roadmap, and risks.

### libkarm or libarmdesk changes

1. require a concrete application or SDK consumer;
2. keep `libkarm` GUI-independent;
3. keep neutral models separate from platform adapters;
4. use caller-owned state and explicit failure returns;
5. test corruption, overflow, allocation failure, and rollback;
6. preserve archive extraction and section garbage collection;
7. measure application stack and binary deltas;
8. do not add a global heap merely for convenience.

### Visible application changes

1. keep KLI1, `.data`, and stack gates green;
2. add deterministic model/ABI tests where possible;
3. run focus and marker gates;
4. run `make qemu-fb-visible` for layout, interaction, and workflow claims;
5. record tester, local date/time, exact commit, host setup, steps, result, and
   limitations.

### Raspberry Pi changes

1. preserve the generic board contract;
2. keep normal unsupported capabilities fail closed;
3. run board build and diagnostic package tests;
4. treat physical evidence separately;
5. never infer physical support from source presence or cross-build success;
6. treat writable media as a separate safety milestone.

## 6. Verification commands

Start focused and finish with the complete gate.

```sh
make BOARD=qemu_virt
make BOARD=qemu_virt size
make -C tests test
make stack-check
make qemu-fs-test
bash tests/run_runtime_service_test.sh
bash tests/run_process_parent_wait_test.sh
bash tests/run_vfs_process_fd_test.sh
bash tests/run_user_copy_permissions_test.sh
bash tests/run_kli1_contract_test.sh
bash tests/run_libarmdesk_foundation_test.sh
bash tests/run_irq_origin_gate_test.sh
bash tools/qemu_usercopy_test.sh
bash tools/qemu_focus_test.sh
bash tools/qemu_marker_test.sh all
bash tools/qemu_runtime_stress_test.sh
bash tools/qemu_runtime_net_stress_test.sh
bash tools/qemu_fb_fat_test.sh
```

Full automated gate:

```sh
bash tools/verify.sh
```

Visible desktop:

```sh
make qemu-fb-visible
```

Use the focused scripts that ship with a new subsystem rather than copying a
stale list from this guide. `tools/verify.sh` remains the authoritative aggregate.

## 7. Documentation workflow

When behavior changes:

- update focused reference docs in the same change;
- update `ARCHITECTURE.md` when boundaries or ownership change;
- update `CURRENT_STATE.md` when capabilities or major limits change;
- update `ROADMAP.md` when a planned cut lands or ordering changes;
- update `TECHNICAL_RISKS.md` when a risk opens, closes, or changes boundary;
- keep historical workflow IDs in the owning issue, PR, or release record;
- do not add a banner to every stale file instead of correcting it.

A documentation-only commit cannot promote code, physical hardware support,
durability, or release status.

## 8. Definition of done

A change is complete when:

- the implementation matches a written contract;
- ownership, limits, failure, and rollback are explicit;
- focused permanent tests cover success and failure;
- the affected production builds pass;
- ABI, image-size, `.data`, stack, and board contracts remain valid;
- QEMU or visible evidence exists where the claim requires it;
- canonical documentation agrees;
- temporary diagnostics and migration helpers are removed;
- `bash tools/verify.sh` passes on the final tree.
