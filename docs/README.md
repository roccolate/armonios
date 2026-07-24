# ArmoniOS documentation

This directory separates current implementation, future intent, active risks,
reference contracts, and historical evidence. A document should answer one type
of question and should not accumulate patches for another document's role.

## Start here

| Question | Canonical document |
|---|---|
| What is ArmoniOS and how do I run it? | [`../README.md`](../README.md) |
| What exists on `main` right now? | [`CURRENT_STATE.md`](CURRENT_STATE.md) |
| How is the implemented system organized? | [`ARCHITECTURE.md`](ARCHITECTURE.md) |
| What should be built next? | [`ROADMAP.md`](ROADMAP.md) |
| What technical risks remain open? | [`TECHNICAL_RISKS.md`](TECHNICAL_RISKS.md) |
| How should I work in this repository? | [`DEVELOPMENT_GUIDE.md`](DEVELOPMENT_GUIDE.md) |
| What is the public syscall contract? | [`SYSCALLS.md`](SYSCALLS.md) |
| What exactly is the runtime-service contract? | [`RUNTIME_SERVICE.md`](RUNTIME_SERVICE.md) |
| What has landed in the v0.3 storage/VFS work? | [`V03_IMPLEMENTATION_STATUS.md`](V03_IMPLEMENTATION_STATUS.md) |

## Document responsibilities

### `README.md`

The repository landing page. Keep it short enough to orient a new reader:

- product identity;
- honest current status;
- major implemented capabilities;
- major limitations;
- build and run instructions;
- links to deeper documentation.

Do not place workflow-run histories, investigation timelines, or complete roadmap
cuts in the README.

### `CURRENT_STATE.md`

The operational source of truth for current `main`:

- implemented subsystems;
- current release classification;
- current fixed limits;
- current incomplete work;
- currently valid verification commands.

Do not preserve stale branch names, draft PRs, or superseded investigation text.
Avoid embedding a commit hash that becomes false as soon as documentation itself
is updated. Exact release identities belong in a release record.

### `ARCHITECTURE.md`

Describes how the implemented system works:

- component boundaries;
- dependency direction;
- ownership and lifecycle;
- privilege and execution model;
- public/private interfaces;
- implemented invariants.

It must not describe unimplemented ideas as present. Future designs belong in the
roadmap or a focused proposal.

### `ROADMAP.md`

Contains only future ordering and exit criteria. It may summarize landed
foundations only to establish the starting point of a phase.

When a cut lands:

1. move its detailed behavior to architecture/current state/reference docs;
2. mark the roadmap item landed or remove it from the remaining list;
3. leave only the next incomplete boundary.

### `TECHNICAL_RISKS.md`

Contains active risks and a compact closed-risk summary.

A live risk needs:

- severity;
- precise failure or limitation;
- current mitigation/foundation;
- concrete exit criteria.

Long experiment logs, workflow IDs, and abandoned hypotheses belong in issues,
PR discussions, or historical release records rather than the live register.

### Reference documents

Files such as `SYSCALLS.md`, `PUBLIC_ABI.md`, `LIBKARM.md`, `RUNTIME_SERVICE.md`,
and storage-specific references describe exact contracts. They must change in the
same commit as the corresponding code contract.

### Historical documents

Historical audits and release evidence should be immutable or clearly marked as
historical. They must not compete with `CURRENT_STATE.md` as the description of
current `main`.

A future cleanup may group these records under `docs/history/` after all incoming
links are audited.

## Source-of-truth hierarchy

When documents disagree, use this order:

1. current code and permanent tests;
2. public headers for ABI values and layouts;
3. focused implementation/reference document;
4. `ARCHITECTURE.md`;
5. `CURRENT_STATE.md`;
6. `ROADMAP.md` for future intent;
7. historical audits, issues, PRs, and release notes for provenance only.

A contradiction is a documentation bug. Do not solve it by adding another banner
to every file; correct or rewrite the stale source.

## Update discipline

Every behavior-changing change should consider:

- public ABI headers and `SYSCALLS.md`;
- `ARCHITECTURE.md` when a boundary or ownership rule changes;
- `CURRENT_STATE.md` when user-visible capability or a major limit changes;
- `ROADMAP.md` when a planned cut lands or ordering changes;
- `TECHNICAL_RISKS.md` when a risk opens, closes, or changes boundary;
- focused reference docs and tests.

Documentation-only changes must not upgrade an implementation or verification
claim. Code existing on a branch or closed draft is not part of `main`.

## Style rules

- Describe facts in the present tense.
- Label future work explicitly.
- Prefer stable component names over PR numbers.
- Keep exact evidence identities in release records or the issue that owns them.
- Use absolute capability statements: implemented, partial, unsupported, or
  unverified.
- Do not call QEMU evidence physical-hardware evidence.
- Do not call a build-only backend runtime support.
- Do not infer durability from successful writes without a reboot test.
- Do not infer optional capability from ABI version alone.

## Current documentation state

The repository landing page and the canonical current-state, architecture,
roadmap, and technical-risk documents were structurally rewritten from the
current `main` implementation. Focused references remain authoritative for their
narrow contracts and should be reviewed incrementally as those subsystems change.
