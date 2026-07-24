# ArmoniOS documentation

This directory separates current implementation, future intent, active risks,
reference contracts, operating guidance, and historical evidence.

A document should answer one kind of question. Do not repair contradictions by
adding another temporary banner; correct the stale source.

## Start here

| Question | Canonical document |
|---|---|
| What is ArmoniOS and how do I run it? | [`../README.md`](../README.md) |
| What exists on `main` right now? | [`CURRENT_STATE.md`](CURRENT_STATE.md) |
| How is the implemented system organized? | [`ARCHITECTURE.md`](ARCHITECTURE.md) |
| What should be built next? | [`ROADMAP.md`](ROADMAP.md) |
| What technical risks remain open? | [`TECHNICAL_RISKS.md`](TECHNICAL_RISKS.md) |
| How should I work in this repository? | [`DEVELOPMENT_GUIDE.md`](DEVELOPMENT_GUIDE.md) |
| How should I contribute a change? | [`CONTRIBUTING.md`](CONTRIBUTING.md) |
| How are documentation claims governed? | [`DOCUMENTATION_POLICY.md`](DOCUMENTATION_POLICY.md) |

## Current reference contracts

| Area | Document |
|---|---|
| Built-in applications and their limits | [`APPLICATIONS.md`](APPLICATIONS.md) |
| Public syscall surface | [`SYSCALLS.md`](SYSCALLS.md) |
| Public ABI ownership and compatibility | [`PUBLIC_ABI.md`](PUBLIC_ABI.md) |
| Structured VFS metadata | [`VFS_METADATA_ABI.md`](VFS_METADATA_ABI.md) |
| Filesystem status and capability reporting | [`VFS_FSINFO_ABI.md`](VFS_FSINFO_ABI.md) |
| Userland runtime | [`LIBKARM.md`](LIBKARM.md) |
| Desktop userland layer | [`LIBARMDESK.md`](LIBARMDESK.md) |
| GUI ownership and event rules | [`GUI_ABI_NOTES.md`](GUI_ABI_NOTES.md) |
| Deferred runtime service | [`RUNTIME_SERVICE.md`](RUNTIME_SERVICE.md) |
| Memory map and page ownership | [`MEMORY_MAP.md`](MEMORY_MAP.md) |
| Kernel image budget | [`SIZE_POLICY.md`](SIZE_POLICY.md) |
| v0.3 storage/VFS checkpoint | [`V03_IMPLEMENTATION_STATUS.md`](V03_IMPLEMENTATION_STATUS.md) |
| Board boundary and physical porting | [`PORTING.md`](PORTING.md) |
| Selective Retrocore/rkc design adoption | [`RETROCORE_ADOPTION.md`](RETROCORE_ADOPTION.md) |

## Historical evidence

Exact commits, workflow IDs, measured sizes, and dated observations belong under
[`history/`](history/README.md).

Current records:

- [`history/V02_RUNTIME_EVIDENCE.md`](history/V02_RUNTIME_EVIDENCE.md);
- [`history/V03_FOUNDATION_PROVENANCE.md`](history/V03_FOUNDATION_PROVENANCE.md).

Historical evidence applies only to the tree and test scope named by the record.
It does not replace current-state or reference documentation.

## Document roles

- `CURRENT_STATE.md` describes current capability and release classification.
- `ARCHITECTURE.md` describes implemented structure and ownership.
- `ROADMAP.md` contains remaining work and exit criteria.
- `TECHNICAL_RISKS.md` contains active risks and a compact closed-risk summary.
- focused references describe exact subsystem contracts.
- `history/` preserves provenance and dated evidence.
- issues, pull requests, reviews, and chat are working context until durable facts
  are promoted into the appropriate document.

## Source-of-truth order

When documentation disagrees, use:

1. current code and permanent tests;
2. public ABI headers for values and layouts;
3. the focused subsystem reference;
4. `ARCHITECTURE.md`;
5. `CURRENT_STATE.md`;
6. `ROADMAP.md` for future intent;
7. historical records and issue/PR discussion for provenance only.

A contradiction is a documentation bug.

## Update discipline

A behavior-changing change should update, where relevant:

- public ABI headers and `SYSCALLS.md`;
- a focused subsystem reference;
- `ARCHITECTURE.md` when a boundary or ownership rule changes;
- `CURRENT_STATE.md` when user-visible capability or a major limit changes;
- `ROADMAP.md` when a planned cut lands or ordering changes;
- `TECHNICAL_RISKS.md` when a risk opens, closes, or changes boundary;
- `APPLICATIONS.md` when a built-in application changes behavior;
- tests and evidence records.

Documentation-only changes may correct, clarify, reorganize, or downgrade claims.
They may not upgrade runtime evidence.

## Style rules

- Describe current facts in the present tense.
- Label planned work explicitly.
- Prefer stable component names over PR numbers.
- Keep exact evidence identities in release or historical records.
- Do not call QEMU evidence physical-hardware evidence.
- Do not infer durability from a successful write without reboot evidence.
- Do not infer capability from ABI version alone.
- Do not document closed unmerged work as part of `main`.
