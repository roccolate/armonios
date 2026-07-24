# ArmoniOS documentation

This directory separates current implementation, future intent, active risks,
reference contracts, and historical evidence. Each document should answer one
type of question instead of accumulating patches for another document's role.

## Start here

| Question | Canonical document |
|---|---|
| What is ArmoniOS and how do I run it? | [`../README.md`](../README.md) |
| What exists on `main` right now? | [`CURRENT_STATE.md`](CURRENT_STATE.md) |
| How is the implemented system organized? | [`ARCHITECTURE.md`](ARCHITECTURE.md) |
| What should be built next? | [`ROADMAP.md`](ROADMAP.md) |
| What technical risks remain open? | [`TECHNICAL_RISKS.md`](TECHNICAL_RISKS.md) |
| How should I work in this repository? | [`DEVELOPMENT_GUIDE.md`](DEVELOPMENT_GUIDE.md) |
| How are documentation claims governed? | [`DOCUMENTATION_POLICY.md`](DOCUMENTATION_POLICY.md) |

## Current reference contracts

| Area | Document |
|---|---|
| Public syscall surface | [`SYSCALLS.md`](SYSCALLS.md) |
| Public ABI ownership and compatibility | [`PUBLIC_ABI.md`](PUBLIC_ABI.md) |
| Userland runtime | [`LIBKARM.md`](LIBKARM.md) |
| Desktop userland layer | [`LIBARMDESK.md`](LIBARMDESK.md) |
| Deferred runtime service | [`RUNTIME_SERVICE.md`](RUNTIME_SERVICE.md) |
| Kernel image budget | [`SIZE_POLICY.md`](SIZE_POLICY.md) |
| v0.3 storage/VFS checkpoint | [`V03_IMPLEMENTATION_STATUS.md`](V03_IMPLEMENTATION_STATUS.md) |
| Memory layout and mapping | [`MEMORY_MAP.md`](MEMORY_MAP.md) |
| GUI ABI details | [`GUI_ABI_NOTES.md`](GUI_ABI_NOTES.md) |
| Porting and board boundaries | [`PORTING.md`](PORTING.md) |

## Historical evidence

Exact commits, workflow IDs, measured sizes, and dated observations belong under
[`history/`](history/README.md).

Current historical records include:

- [`history/V02_RUNTIME_EVIDENCE.md`](history/V02_RUNTIME_EVIDENCE.md);
- [`history/V03_FOUNDATION_PROVENANCE.md`](history/V03_FOUNDATION_PROVENANCE.md).

Historical evidence applies only to the tree and test scope named by the record.
It does not replace current-state or reference documentation.

## Document responsibilities

### Repository `README.md`

The public landing page contains:

- product identity;
- honest current status;
- major capabilities and limitations;
- build and run instructions;
- links to deeper documentation.

It does not contain investigation timelines, complete roadmap cuts, or workflow
histories.

### `CURRENT_STATE.md`

The operational source of truth for current `main`:

- implemented subsystems;
- release classification;
- fixed limits;
- incomplete work;
- current verification commands.

It avoids branch names, draft PRs, and commit hashes that become false after a
documentation-only commit. Exact release identities belong in historical release
records.

### `ARCHITECTURE.md`

Describes implemented component boundaries, dependency direction, ownership,
lifecycle, privilege, execution, and public/private interfaces. Future designs
belong in the roadmap or a focused proposal.

### `ROADMAP.md`

Contains remaining ordering and exit criteria. When a cut lands:

1. move implemented detail to architecture, current-state, and reference docs;
2. remove or mark the completed roadmap item;
3. leave only the next incomplete boundary.

### `TECHNICAL_RISKS.md`

Contains active risks and a compact closed-risk summary. A live risk needs:

- severity;
- precise failure or limitation;
- current mitigation or foundation;
- concrete exit criteria.

Experiment timelines and workflow logs belong in issues or historical records.

### Reference documents

Reference files describe exact current contracts. They must change with the code
when their public surface, ownership, limits, or failure behavior changes.

### Historical records

Historical records preserve evidence and provenance. They are intentionally not
rewritten into descriptions of later `main`.

## Source-of-truth order

When documents disagree, inspect in this order:

1. current code and permanent tests;
2. public headers for ABI values and layouts;
3. focused reference document;
4. `ARCHITECTURE.md`;
5. `CURRENT_STATE.md`;
6. `ROADMAP.md` for future intent;
7. historical records, issues, PRs, and release notes for provenance only.

A contradiction is a documentation bug. Correct the stale source instead of
adding another dated warning banner.

## Update discipline

Every behavior-changing change should consider:

- public ABI headers and `SYSCALLS.md`;
- focused reference documents;
- `ARCHITECTURE.md` when ownership or subsystem boundaries change;
- `CURRENT_STATE.md` when user-visible capability or a major limit changes;
- `ROADMAP.md` when planned work lands or ordering changes;
- `TECHNICAL_RISKS.md` when a risk opens, closes, or changes boundary;
- `DEVELOPMENT_GUIDE.md` when operating rules change;
- repository README last.

Documentation-only changes must not upgrade implementation or verification
claims. Code from a deleted branch or closed unmerged PR is not part of `main`.

## Style rules

- Describe current facts in the present tense.
- Label future work explicitly.
- Prefer stable component names over PR numbers.
- Keep exact evidence identities in historical or release records.
- Use explicit capability language: implemented, partial, unsupported, or
  unverified.
- Do not call QEMU evidence physical-hardware evidence.
- Do not call a build-only backend runtime support.
- Do not infer durability from successful writes without a reboot test.
- Do not infer optional capability from ABI version alone.
