# Historical Kernel Technical-Debt Review

> **Historical record — not the current risk register.**
>
> This review tracked a previous cleanup pass and was closed against its original checklist. A later repository audit identified additional open correctness and portability risks. Current work must use `TECHNICAL_RISKS.md`, `CURRENT_STATE.md`, and `ROADMAP.md`.

Do not cite this file as evidence that the kernel has no technical debt or that v1.0 is ready.

## Original scope

The review covered `kernel/`, `drivers/`, panel/application support, and the host tests that pinned the contracts found during the panel/desktop cleanup.

Its purpose was to close a finite backlog, not to provide a permanent certification of the codebase.

## Original verification baseline

```sh
make
make size
make -C tests test
```

Additional QEMU-desktop checks used during that work were:

```sh
make stack-check
make qemu-fs-test
timeout 25s make qemu-fb
timeout 25s make qemu-usb
timeout 25s make qemu-net
```

The later documentation audit clarified that timeout-only QEMU launches are not deterministic pass/fail tests. Current verification rules live in `DOCUMENTATION_POLICY.md`.

## Items closed by the original review

| Item | Original result |
|---:|---|
| 0 | EL0 launch and argv packing received host tests. |
| 1 | The GUI monolith was split into focused `gui_*` modules. |
| 2 | Owner-window syscall checks were centralized. |
| 3 | Unchecked GUI internal helpers were named explicitly. |
| 4 | User-pointer range checks were centralized in syscall helpers. |
| 5 | KLI1 image-format constants moved to a dedicated header. |
| 6 | Process user-region capacity increased and stack spacing gained assertions. |
| 7 | Boot initialization status became queryable. |
| 8 | Numeric formatting moved out of the UART driver. |
| 9 | User layout, SPSR values, and exit constants were centralized. |
| 10 | A QEMU FAT storage smoke test was added. |
| 11 | GUI headers reduced hidden framebuffer coupling. |
| 12 | Kernel-console command dispatch became table-driven. |
| 13 | Per-process image and stack backing moved from static BSS to PMM pages. |
| 14 | Panel recovery moved behind generic callbacks. |
| 15 | Userland stack usage gained `make stack-check`. |
| 16 | Process next-runnable activation was centralized. |
| 17 | GUI backing-buffer resize became atomic and tested. |
| 18 | Compiler attributes were centralized. |
| 19 | DHCP option parsing gained a pure tested helper. |
| 20 | Build help and compact output were added. |

These remain useful accomplishments. They do not close later-discovered risks.

## Later audit findings that supersede broad closure language

The 2026-07-13 audit found, among other items:

- user-output pointer checks do not enforce write permission (`RISK-001`);
- VFS file descriptors are global rather than process-owned (`RISK-002`);
- the visible desktop target does not attach the FAT image (`RISK-003`);
- spawned-editor focus is incorrect in the observed workflow (`RISK-004`);
- several QEMU gates are timeout-only launch commands (`RISK-005`);
- the RPi4 board contract is incomplete (`RISK-006`);
- the experimental eMMC code is not a valid hardware driver claim (`RISK-007`);
- kernel mappings remain full-RAM identity RWX (`RISK-008`);
- KLI1 mutable `.data`/`.bss` semantics are undefined (`RISK-009`).

The earlier item “user-buffer validation centralized” is still true as a refactoring statement. It must not be interpreted as permission-safe user copying.

## Preservation rule

Keep this document as historical context. Do not append new backlog items here.

New risks belong in `TECHNICAL_RISKS.md` and may close only under the evidence rules in `DOCUMENTATION_POLICY.md`.
