# Documentation Policy

This document defines how ArmoniOS documentation earns and keeps trust.

The code is the implementation, but code presence alone is not proof that a feature builds, boots, or works end to end. Documentation must always distinguish those states.

## Source-of-truth order

When documents disagree, use this order:

1. `docs/CURRENT_STATE.md` — audited operational status and latest verification evidence.
2. `docs/TECHNICAL_RISKS.md` — open correctness, stability, security, and portability risks.
3. `docs/ROADMAP.md` — ordered future work and release exit criteria.
4. `docs/ARCHITECTURE.md`, `docs/MEMORY_MAP.md`, `docs/SYSCALLS.md`, and `docs/GUI_ABI_NOTES.md` — current technical contracts.
5. `README.md` — concise public overview derived from the documents above.
6. Historical reviews and issue comments — context only; they are not current status unless promoted into `CURRENT_STATE.md`.

`docs/TECH_DEBT_REVIEW.md` is a historical closure record. It is not the active risk registry.

## Evidence labels

Use these labels exactly when recording subsystem status:

| Label | Meaning |
|---|---|
| `IMPLEMENTED` | Relevant code exists and was inspected. No build or runtime claim is implied. |
| `HOST-VERIFIED` | Native host tests passed for the stated behavior. |
| `BUILD-VERIFIED` | The stated target compiled and linked successfully. |
| `QEMU-VERIFIED` | A QEMU run reached explicit expected markers or assertions. |
| `MANUAL-VERIFIED` | A named person completed a documented visible or hardware workflow. |
| `UNVERIFIED` | The code or plan exists, but the required evidence is missing. |
| `KNOWN-BROKEN` | A reproducible defect or static contradiction prevents the claim. |
| `PLANNED` | No current implementation claim. |

A stronger label does not automatically prove every weaker or adjacent claim. For example, host tests do not prove a QEMU driver, and a successful build does not prove hardware boot.

## Required status metadata

Every substantial update to `CURRENT_STATE.md` must include:

- audit date;
- audited branch or commit;
- exact commands run;
- who ran manual or hardware checks;
- commands not run;
- known limitations discovered during verification.

Do not use `latest`, `stable`, `complete`, `supported`, `working`, or `done` without identifying the evidence and scope.

## Claim rules

### Code inspection

Code inspection may justify `IMPLEMENTED` or `KNOWN-BROKEN`. It may not justify `BUILD-VERIFIED`, `QEMU-VERIFIED`, or `MANUAL-VERIFIED`.

### Tests

A test claim must name the command and the behavior it actually asserts. A target that only launches QEMU until an external timeout is not a passing test unless it checks explicit guest output or another deterministic result.

### Hardware

A board is supported only after all of the following are recorded:

1. the board target builds and links;
2. the kernel reaches an explicit serial milestone on the physical board;
3. the exact board revision, firmware setup, boot files, and reproduction steps are documented.

Files under `drivers/boards/<board>/` do not constitute hardware support by themselves.

### Documentation-only changes

Documentation may describe newly found defects without changing code. Such defects must also appear in `TECHNICAL_RISKS.md` when they can affect correctness, release readiness, or future maintenance.

## Update workflow

For every code or build change:

1. Identify the public claim affected by the change.
2. Add or update the smallest relevant automated test.
3. Run the applicable verification commands.
4. Update `CURRENT_STATE.md` with evidence, not intention.
5. Update `TECHNICAL_RISKS.md` if a risk was introduced, changed, mitigated, or closed.
6. Update architecture, ABI, porting, and roadmap documents only where their contract changed.
7. Update `README.md` last so it summarizes the verified documents rather than becoming an independent source of claims.

The author of a behavior-changing change owns the matching documentation update. There is no separate documentation team assumed by the process.

## Closing a risk

A risk may move to `CLOSED` only when its exit criteria are satisfied and the evidence is linked or copied into the risk entry. A comment such as “fixed” or “tests pass” is insufficient without the exact test scope.

Closed risks remain in the file for traceability or are moved to a dated archive. They must not be silently deleted.

## Release claims

A version may be called a release candidate only when:

- every mandatory release gate has deterministic pass/fail behavior;
- all P0 risks for that release are closed;
- every P1 risk is either closed or explicitly accepted with rationale;
- the visible/manual workflow has a dated result;
- `README.md`, `CURRENT_STATE.md`, `ROADMAP.md`, and ABI documents agree.

Until those conditions are met, use `alpha`, `experimental`, or `baseline`, whichever matches the evidence.

## Verification record template

Use this template in issues, pull requests, and `CURRENT_STATE.md` updates:

```text
Date:
Commit/branch:
Environment:

Commands run:
- command -> result and important marker

Manual checks:
- workflow -> result, performed by

Not run:
- command/check -> reason

Known limitations:
- limitation and affected scope
```
