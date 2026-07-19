# Agent Guide

This file is the live entry point for agents working in this repository.

## Current Status

ArmoniOS is a v0.1 QEMU desktop baseline on the `main` baseline.

The latest automated baseline recorded in the documentation is:

- command: `bash tools/verify.sh`
- source tree: v0.1 pre-reset baseline
- timestamp: `2026-07-19T01:05:06Z`
- result: all automated baseline gates passed

Manual visible desktop evidence remains the existing `make qemu-fb-visible` workflow recorded by rocco on 2026-07-17. No newer manual desktop check is recorded.

Raspberry Pi 4 is build-verified scaffolding only. Do not claim hardware support.

## Read First

1. `docs/DOCUMENTATION_POLICY.md`
2. `docs/CURRENT_STATE.md`
3. `docs/TECHNICAL_RISKS.md`
4. `docs/ROADMAP.md`
5. `docs/ARCHITECTURE.md`
6. `docs/MEMORY_MAP.md`
7. `docs/SYSCALLS.md`
8. `docs/GUI_ABI_NOTES.md`
9. `docs/CONTRIBUTING.md`
10. `docs/PORTING.md`

## Verification

Use the smallest relevant test while developing, then run the full baseline before promoting a code or build-contract change:

```sh
bash tools/verify.sh
```

The baseline runs build, size, BOARD=rpi4 build-contract, host tests, process-local VFS descriptor isolation, user-copy permissions, KLI1 contract, stack check, FAT32 QEMU smoke, usercopy/focus QEMU gates, framebuffer/USB/network marker gates, and visible-desktop FAT+GPU wiring.

Manual visible claims require:

```sh
make qemu-fb-visible
```

Record the tester, date, exact commit, workflow, and observed limitations.

## Documentation Rules

- Keep technical documentation in English.
- Update docs from evidence, not intent.
- Keep manual visible evidence separate from automated QEMU marker evidence.
- Do not add archive or historical handoff files.
- Do not reintroduce obsolete handoff/history files.
