# Documentation policy

ArmoniOS documentation separates implementation, verification, future intent,
active risk, and historical evidence. Code presence alone does not prove that a
target builds, boots, remains responsive, preserves data, works visibly, or works
on physical hardware.

The documentation map is maintained in `README.md` inside this directory.

## Canonical roles

| Question | Source |
|---|---|
| What exists on current `main`? | `CURRENT_STATE.md` |
| How is it implemented? | `ARCHITECTURE.md` and focused reference documents |
| What remains to be built? | `ROADMAP.md` |
| What risks are active? | `TECHNICAL_RISKS.md` |
| How should development work proceed? | `DEVELOPMENT_GUIDE.md` |
| What was proven on an exact historical tree? | `history/` or a release record |
| What should a new reader know? | repository-root `README.md` |

Issues, pull requests, reviews, and chat notes are working context. They become
canonical only after their durable facts are incorporated into the appropriate
live or historical document.

Do not create competing “latest status”, handoff, progress-log, or second
current-state documents. Correct the canonical source.

## Source-of-truth order

When claims conflict, inspect in this order:

1. current code and permanent tests;
2. public ABI headers for public numbers and layouts;
3. focused implementation/reference documents;
4. `ARCHITECTURE.md`;
5. `CURRENT_STATE.md`;
6. `ROADMAP.md` for future intent;
7. historical records, issues, PRs, and external notes for provenance only.

A contradiction is a documentation defect. Do not hide it under another dated
banner.

## Evidence labels

| Label | Meaning |
|---|---|
| `IMPLEMENTED` | Relevant code exists and was inspected. |
| `HOST-VERIFIED` | A native host test passed for pure or mocked behavior. |
| `BUILD-VERIFIED` | The named target compiled and linked. |
| `QEMU-VERIFIED` | QEMU reached explicit asserted behavior or markers. |
| `CI-VERIFIED` | A hosted run completed the named checks with auditable identity. |
| `MANUAL-VERIFIED` | A named person completed a documented visible or hardware workflow. |
| `UNVERIFIED` | Implementation or intent exists without required evidence. |
| `KNOWN-BROKEN` | A reproducible defect prevents the claim. |
| `PLANNED` | No implementation claim. |

Evidence does not automatically expand to adjacent claims:

- host tests do not prove MMIO;
- a build does not prove boot;
- a serial marker does not prove visible layout or interaction;
- EOI ordering does not prove bounded exception-return latency;
- a consumption counter does not prove absence of device loss;
- one successful boot does not prove repeatability;
- repeated non-reproduction does not identify a root cause;
- QEMU evidence does not prove physical hardware;
- successful writes do not prove reboot durability;
- one FAT fixture does not prove broad interoperability.

## Live versus historical evidence

### Live documents

Live documents describe the current contract or current capability. They should
prefer stable component names and present-tense facts. They should not carry:

- branch names;
- draft PR status;
- obsolete investigations;
- a measured image size that changes with later commits;
- long workflow histories;
- exact release identities that belong to one historical tree.

### Historical records

Historical records may preserve:

- exact commits and PR heads;
- workflow IDs;
- measured sizes;
- dated manual observations;
- experiment scope;
- accepted limitations;
- evidence boundaries.

A historical record applies only to the exact tree and conditions it names. It
must not be treated as proof for later `main` without content equivalence or a new
run.

## Automated evidence identity

A release or promoted historical record should identify:

- exact implementation commit or tested tree;
- PR head and merge commit when relevant;
- command or workflow/run ID;
- environment when material;
- asserted behavior or required markers;
- artifacts or logs where available;
- checks not run;
- known limitations and unobservable behavior.

A green badge without the tested tree and scope is not a durable release record.
Queued, cancelled, or timed-out runs are not passing evidence.

## Claim rules

### Static inspection

Static inspection can establish implementation, an architectural boundary, or a
risk. It cannot establish build, QEMU, CI, manual, or hardware verification.

### Host tests

A host test proves the pure-C or mocked behavior it asserts. It does not prove real
exception timing, device ordering, integration, or hardware.

### Build tests

A successful compile/link proves build-contract compatibility. It does not prove
boot or runtime behavior.

### QEMU tests

A QEMU test must assert deterministic guest output or another explicit result.
Launching until timeout is not a passing test. The claim should name the marker or
workflow actually checked.

### Stress and soak tests

Record:

- workload generation;
- duration or iteration count;
- failure and stop conditions;
- progress/liveness signal;
- overflow, loss, panic, or corruption observability;
- maximum or aggregate metrics where relevant;
- what the test cannot observe.

Forced instrumentation must be distinguished from natural production-threshold
behavior. A test image is not automatically the release artifact.

### Manual visible tests

Record tester, date, exact tree/image, launch command, setup, steps, result, and
observed limitations. Historical manual evidence is not silently carried forward
to every new tree.

### Physical hardware

A board support claim requires:

1. a reproducible build;
2. an explicit physical serial milestone;
3. exact board revision, firmware, boot files, and steps;
4. separate evidence for each claimed subsystem;
5. fail-closed behavior for unsupported capabilities;
6. an explicit safety plan for destructive storage operations.

Source files or a cross-build alone do not establish physical support.

## Behavior-changing workflow

For a behavior-changing cut:

1. identify the affected claim and risk;
2. add or update the smallest focused test;
3. run focused checks;
4. run the full verification matrix before promotion;
5. update public headers and focused reference documents with the code contract;
6. update architecture when ownership or subsystem boundaries change;
7. update current state when user-visible capability or a major limit changes;
8. update roadmap when a planned cut lands or ordering changes;
9. update risks when severity, mitigation, or exit criteria change;
10. update README last.

The author of a behavior change owns its tests and documentation.

## Documentation-only changes

Documentation-only changes may:

- correct stale facts;
- align text with already-merged implementation;
- clarify architecture and ownership;
- downgrade unsupported wording;
- separate historical evidence from live contracts;
- add a risk discovered by inspection;
- reorganize future dependencies.

They may not upgrade runtime evidence or claim a test was run when it was not.

## Risks

A live risk entry needs:

- severity;
- precise failure or limitation;
- current mitigation or foundation;
- concrete exit criteria.

A risk closes only when its exit criteria and evidence are recorded. A bounded
residual may be accepted for one release with rationale, mitigation, release
impact, and follow-up. Acceptance is not closure.

The live register should retain active risks and a compact closed-risk summary.
Long experiment timelines belong in issues or historical records.

## Release language

A tree may be called a release candidate only when:

- mandatory gates have deterministic pass/fail behavior;
- release-blocking P0 risks are closed;
- each release-blocking P1 risk is closed or explicitly accepted;
- required manual workflows have dated results;
- live documents agree;
- the exact tested tree and artifact are identified;
- unsupported product and hardware claims remain explicit.

Until then use language such as alpha, experimental, baseline, or promotion
candidate with clear scope.

## Historical record template

```text
Title and scope:
Date:
Implementation tree:
PR head / merge identity, when relevant:
Environment:

Commands or hosted runs:
- command/run -> result and asserted behavior

Manual checks:
- tester, exact tree/image, workflow, result

Not run:
- check -> reason

Known limitations:
- limitation and affected scope

Evidence boundary:
- what this record proves and does not prove
```
