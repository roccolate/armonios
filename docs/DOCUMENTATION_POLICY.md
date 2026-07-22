# Documentation Policy

This document defines how ArmoniOS documentation earns and keeps trust.

Code is implementation evidence, but code presence alone does not prove that a
target builds, boots, remains responsive, preserves data, or works on physical
hardware. Documentation must distinguish those states precisely.

## Source-of-truth order

When documents disagree, use this order:

1. `docs/CURRENT_STATE.md` — audited operational status and exact verification
   evidence;
2. `docs/TECHNICAL_RISKS.md` — open correctness, stability, security,
   responsiveness, and portability risks;
3. `docs/ROADMAP.md` — ordered future work and release exit criteria;
4. current technical contracts:
   - `docs/ARCHITECTURE.md`;
   - `docs/RUNTIME_SERVICE.md`;
   - `docs/MEMORY_MAP.md`;
   - `docs/SYSCALLS.md`;
   - `docs/GUI_ABI_NOTES.md`;
   - `docs/PORTING.md`;
5. `AGENTS.md` and `docs/CONTRIBUTING.md` — operating rules derived from the
   sources above;
6. `README.md` — public summary derived from verified documents;
7. issues, PR descriptions, comments, external reviews, and chat notes — context
   only until promoted into the canonical documents.

Do not create a second current-state, handoff, audit-summary, or “latest status”
document. Correct the canonical source instead.

## Evidence labels

Use these labels exactly when recording subsystem status:

| Label | Meaning |
|---|---|
| `IMPLEMENTED` | Relevant code exists and was inspected. No build or runtime claim is implied. |
| `HOST-VERIFIED` | Native host tests passed for the stated behavior. |
| `BUILD-VERIFIED` | The stated target compiled and linked successfully. |
| `QEMU-VERIFIED` | A QEMU run reached explicit expected markers or assertions. |
| `CI-VERIFIED` | A hosted CI run completed the stated command and retained enough logs/artifacts to audit it. |
| `MANUAL-VERIFIED` | A named person completed a documented visible or hardware workflow. |
| `UNVERIFIED` | Code or intent exists, but required evidence is missing. |
| `KNOWN-BROKEN` | A reproducible defect or contradiction prevents the claim. |
| `PLANNED` | No current implementation claim. |

A stronger label does not automatically prove adjacent claims. Examples:

- host tests do not prove device MMIO;
- a successful build does not prove boot;
- a QEMU serial marker does not prove visible layout or interaction;
- EOI ordering does not prove bounded exception-return latency;
- a physical serial boot does not prove storage writes;
- one filesystem fixture does not prove broad interoperability.

## Evidence identity

Every promoted automated result must identify the exact code tree it exercised.
Record:

- commit or PR head SHA;
- workflow/run ID or local command;
- environment when material;
- asserted behavior or required markers;
- artifacts or logs where available.

### Pull-request and merge evidence

GitHub Actions may run against a PR head or a synthetic PR merge ref. When a PR
is later merged:

- the successful PR run validates the code tree it actually checked;
- if the final merge commit has the same resulting content, the PR evidence may
  be cited as evidence for the merged code tree;
- do not claim the final merge commit was independently rerun unless a workflow
  actually ran against that commit;
- record both the validated PR head/run and the final merge commit when they are
  different identifiers;
- if the merge introduced conflict resolution or another content difference,
  rerun the required promotion gate before upgrading the merged claim.

A green branch or PR badge is not a substitute for exact run IDs in
`CURRENT_STATE.md`.

## Required status metadata

Every substantial update to `CURRENT_STATE.md` must include:

- audit date;
- audited implementation commit or tree;
- documentation-only branch/commit when relevant;
- exact commands or hosted workflow runs;
- who performed manual or hardware checks;
- checks not run;
- known limitations discovered;
- whether evidence applies to a PR tree, merge commit, local tree, QEMU, or
  physical hardware.

Do not use `latest`, `stable`, `complete`, `supported`, `working`, `done`, or
`release candidate` without identifying evidence and scope.

## Claim rules

### Static inspection

Static inspection may justify `IMPLEMENTED`, `KNOWN-BROKEN`, an architectural
limit, or a risk entry. It may not justify `BUILD-VERIFIED`, `QEMU-VERIFIED`,
`CI-VERIFIED`, `MANUAL-VERIFIED`, or physical support.

### Host tests

A host test proves only the mocked or pure-C contract it asserts. It does not
prove real exception timing, MMIO ordering, QEMU integration, or hardware.

### Build tests

A successful compile/link proves symbol and build-contract compatibility. It
does not prove boot or runtime behavior.

### QEMU tests

A QEMU test must assert deterministic guest output or another explicit result. A
target that only launches until timeout is not a passing test.

The documentation must name the markers or behavior checked. “QEMU boots” is too
broad when a test only reaches one storage or panel marker.

### Manual visible tests

Record:

- tester;
- date;
- exact commit/image;
- launch command;
- input/display configuration;
- workflow steps;
- observed limitations.

Do not silently carry old manual evidence forward as if it were rerun on every
new automated baseline.

### Physical hardware

A board may be called supported only after all of the following are recorded:

1. target builds and links;
2. kernel reaches an explicit serial milestone on the physical board;
3. exact board revision, firmware, boot files, and reproduction steps exist;
4. each claimed subsystem has its own evidence;
5. unsupported capabilities fail closed.

Files under `drivers/boards/<board>/` do not constitute hardware support.

### Documentation-only changes

Documentation-only changes may:

- correct stale identifiers;
- clarify architecture;
- downgrade an unsupported claim;
- record newly discovered risks;
- reorganize roadmap dependencies.

They may not upgrade runtime evidence. Newly discovered correctness, release, or
maintenance risks must be added to `TECHNICAL_RISKS.md`.

## Update workflow

For every code or build change:

1. identify the public claim and risk affected;
2. add or update the smallest relevant automated test;
3. run focused checks;
4. run `bash tools/verify.sh` before promotion;
5. update `TECHNICAL_RISKS.md` when risk state changes;
6. update `CURRENT_STATE.md` with exact evidence;
7. update architecture, runtime, memory, ABI, porting, and roadmap documents only
   where their contract changed;
8. update `AGENTS.md` and `CONTRIBUTING.md` when operating rules changed;
9. update README last.

The author of a behavior-changing change owns the matching documentation. There
is no separate documentation team assumed by the process.

## Closing a risk

A risk may move to `CLOSED` only when:

- its exit criteria are satisfied;
- exact evidence is named;
- the affected release claim is updated;
- follow-up limitations are preserved rather than silently deleted.

“Fixed”, “looks good”, or “tests pass” is insufficient without scope.

Closed risks remain in the active register for traceability. Do not create dated
archive or handoff files by default.

## Release claims

A version may be called a release candidate only when:

- every mandatory gate has deterministic pass/fail behavior;
- all P0 risks for that release are closed;
- every P1 risk is closed or explicitly accepted with rationale;
- required visible/manual workflows have dated results;
- README, current state, risks, roadmap, architecture, and ABI documents agree;
- the tested commit/tree and release artifact are identified;
- unsupported hardware or product claims remain explicit non-claims.

Until those conditions are met, use `alpha`, `experimental`, or `baseline`.

## Verification record template

```text
Date:
Implementation commit/tree:
Documentation commit/branch (if different):
Environment:

Commands or hosted runs:
- command/run ID -> result and asserted marker/contract

Manual checks:
- workflow -> result, performed by, exact commit/image

Not run:
- command/check -> reason

Known limitations:
- limitation and affected scope

Evidence notes:
- PR head, synthetic merge ref, final merge commit, local tree, QEMU, or hardware
```
