# Roadmap

This roadmap starts from the v0.1 QEMU desktop baseline. It is ordered by evidence and maintenance value, not by feature novelty.

Status claims must follow `DOCUMENTATION_POLICY.md`. Current operational truth lives in `CURRENT_STATE.md`; correctness and hardware risks live in `TECHNICAL_RISKS.md`.

## v0.1 baseline

**State:** stable QEMU desktop baseline.

v0.1 means:

- the full automated baseline passes through `bash tools/verify.sh`;
- the visible Files/Editor/FAT workflow has dated manual evidence;
- syscall-boundary, VFS descriptor, KLI1, kernel W^X, deterministic QEMU, and CI evidence risks are closed;
- Raspberry Pi 4 remains build-verified scaffolding only.

The goal of v0.1 is to create a clean first public history point for continued work.

## v0.2 internal simplification

Focus on reducing contributor friction without changing public behavior.

- Continue behavior-preserving splits of oversized files only where boundaries are already clear.
- Keep syscall numbers, KLI1 layout, FAT32 scope, and build targets stable.
- Prefer deleting obsolete history, duplicate helpers, and unreachable scaffolding over adding abstraction.
- Add focused tests before moving code that lacks coverage.
- Keep `bash tools/verify.sh` as the promotion gate.

Candidate areas:

- FAT32 helper boundaries after the VFS bridge split;
- GUI compositor drawing helpers after the damage tracker split;
- app-local command parsing in `shell`;
- small board-capability cleanup that does not claim hardware support.

## v0.3 desktop polish

Improve the visible QEMU desktop while keeping the kernel ABI stable.

- Fix Editor's single-visible-line polish issue if it still reproduces.
- Improve Files create/rename/delete feedback and selection persistence.
- Improve Shell command help and path error messages.
- Tighten panel task behavior only with tests and visible validation.
- Record a fresh `make qemu-fb-visible` workflow before promoting.

## v0.4 memory and process hardening

This is the next kernel-hardening milestone after v0.1.

- Move shared kernel mappings to TTBR1.
- Add ASIDs and scoped TLB invalidation.
- Stop cloning full kernel mappings into every process TTBR0.
- Add fault-contained user copy routines.
- Preserve the existing userland ABI unless a concrete incompatibility is documented.

## Hardware track

Raspberry Pi work stays outside the QEMU desktop release line until real hardware evidence exists.

Required order:

1. controlled CPU entry and secondary-core parking;
2. repeatable serial marker on physical hardware;
3. memory and timer validation;
4. read-only storage sector evidence;
5. display and input bring-up;
6. desktop workflow only after the earlier milestones pass.

Do not claim Raspberry Pi support before the hardware evidence rules in `DOCUMENTATION_POLICY.md` and `PORTING.md` are met.
