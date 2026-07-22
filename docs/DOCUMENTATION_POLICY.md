# Documentation Policy

This document defines how ArmoniOS documentation earns and keeps trust.

Code presence is implementation evidence. It does not by itself prove that a
target builds, boots, remains responsive, preserves data, works visibly, or works
on physical hardware.

## Canonical document roles

When documents disagree, use this order:

1. `CURRENT_STATE.md` — audited operational status and exact promoted evidence;
2. `TECHNICAL_RISKS.md` — open correctness, stability, security, responsiveness,
   reproducibility, and portability risks;
3. `ROADMAP.md` — ordered future work and release exit criteria;
4. implemented technical contracts:
   - `ARCHITECTURE.md`;
   - `RUNTIME_SERVICE.md`;
   - `MEMORY_MAP.md`;
   - `SYSCALLS.md`;
   - `GUI_ABI_NOTES.md`;
   - `PORTING.md`;
5. operating guidance derived from the sources above:
   - `DEVELOPMENT_GUIDE.md`;
   - `CONTRIBUTING.md`;
   - repository-root `AGENTS.md`;
6. repository-root `README.md` — public summary derived from verified documents;
7. issues, PR descriptions, comments, external reviews, and chat notes — working
   context only until promoted into the canonical documents.

Do not create a second current-state, handoff, audit-summary, latest-status,
release-status, or progress-log document. Correct the canonical source instead.

## Evidence labels

Use these labels exactly when recording subsystem status:

| Label | Meaning |
|---|---|
| `IMPLEMENTED` | Relevant code exists and was inspected. No build or runtime claim is implied. |
| `HOST-VERIFIED` | Native host tests passed for the stated pure-C or mocked behavior. |
| `BUILD-VERIFIED` | The stated target compiled and linked successfully. |
| `QEMU-VERIFIED` | A QEMU run reached explicit expected markers or assertions. |
| `CI-VERIFIED` | A hosted CI run completed the stated command and retained enough identity/log information to audit it. |
| `MANUAL-VERIFIED` | A named person completed a documented visible or hardware workflow. |
| `UNVERIFIED` | Code or intent exists, but required evidence is missing. |
| `KNOWN-BROKEN` | A reproducible defect or contradiction prevents the claim. |
| `PLANNED` | No current implementation claim. |

A stronger label does not automatically prove adjacent claims:

- host tests do not prove MMIO;
- a successful build does not prove boot;
- a QEMU serial marker does not prove visible layout or interaction;
- EOI ordering does not prove bounded exception-return latency;
- a consumption counter does not prove absence of device loss;
- one successful boot does not prove repeatability;
- repeated non-reproduction does not identify a root cause;
- a physical serial boot does not prove storage writes;
- one filesystem fixture does not prove broad interoperability.

## Promoted versus investigation evidence

Evidence belongs to one of two operational categories.

### Promoted evidence

Promoted evidence supports a claim in `CURRENT_STATE.md`. It must identify the
exact tree and have completed all required checks for that claim.

### Investigation evidence

Investigation evidence comes from an issue branch, draft PR, ad hoc diagnostic
image, partial workflow, failed run, rerun, or experiment. It may:

- narrow hypotheses;
- reproduce a defect;
- demonstrate non-reproduction under stated conditions;
- validate a proposed test harness;
- improve future diagnostics.

It may not upgrade the promoted `main` claim until the final tree is merged or
otherwise selected for promotion, the required checks complete, and
`CURRENT_STATE.md` records the evidence.

An in-progress, queued, cancelled, or timed-out workflow is not passing evidence.
A cancelled run may still contain useful investigation artifacts, but its scope
must be described accurately.

A clean soak is evidence of non-reproduction for the tested sample and conditions.
It is not proof that an unexplained intermittent correctness observation cannot
occur.

## Evidence identity

Every promoted automated result must identify the exact code tree it exercised.
Record:

- commit or PR head SHA;
- merge commit when different;
- workflow/run ID or local command;
- environment when material;
- asserted behavior or required markers;
- artifacts/logs where available;
- checks not run;
- evidence boundary.

### Pull-request and merge evidence

GitHub Actions may run against a PR head or a synthetic merge ref.

When a PR is merged:

- the successful run validates the tree it actually checked;
- if the final merge commit has the same resulting content, the PR evidence may be
  cited for that content-equivalent merge tree;
- record both identifiers when the PR head and merge commit differ;
- do not claim the merge commit was independently rerun unless a workflow actually
  exercised it;
- if merge conflict resolution or another content difference changes the tree,
  rerun the required promotion gate.

Empty-history commits that add and then remove the same content may be documented
as content-equivalent only after an exact tree/file comparison confirms no net
file difference.

A green badge is not a substitute for exact run IDs in `CURRENT_STATE.md`.

## Required status metadata

Every substantial `CURRENT_STATE.md` update must include:

- audit date;
- audited implementation tree;
- relevant merge and PR head identities;
- documentation branch/PR when different;
- exact commands or hosted workflow runs;
- manual tester and workflow where applicable;
- checks not run;
- known limitations;
- whether evidence applies to main, PR head, synthetic merge, local tree, QEMU,
  manual visible use, or physical hardware.

Do not use `latest`, `stable`, `complete`, `supported`, `working`, `done`, or
`release candidate` without identifying evidence and scope.

## Claim rules

### Static inspection

Static inspection may justify `IMPLEMENTED`, `KNOWN-BROKEN`, an architectural
limit, or a risk entry. It may not justify build, QEMU, CI, manual, or hardware
verification.

### Host tests

A host test proves only the pure-C or mocked contract it asserts. It does not
prove real exception timing, MMIO ordering, QEMU integration, or hardware.

### Build tests

A successful compile/link proves symbol and build-contract compatibility. It does
not prove boot or runtime behavior.

### QEMU tests

A QEMU test must assert deterministic guest output or another explicit result. A
target that merely launches until timeout is not a passing test.

Documentation must name the markers or behavior checked. “QEMU boots” is too broad
when a gate reaches only a storage, DHCP, focus, or panel marker.

### Stress and soak tests

A stress or soak claim must record:

- workload generation;
- duration or iteration count;
- stop/failure conditions;
- progress/liveness signal;
- loss/overflow/panic observability;
- maximum or aggregate metrics where relevant;
- what cannot be observed.

A test-only image may validate a contract but must not be presented as the release
artifact. Test instrumentation that forces a condition must be distinguished from
natural production-threshold behavior.

### Manual visible tests

Record:

- tester;
- date;
- exact commit and image where available;
- launch command;
- display/input configuration;
- workflow steps;
- result;
- observed limitations.

Do not carry old manual evidence forward as if it were rerun on every automated
baseline.

### Physical hardware

A board may be called supported only after all of the following are recorded:

1. target builds and links;
2. kernel reaches an explicit serial milestone on the physical board;
3. exact board revision, firmware, boot files, and reproduction steps exist;
4. each claimed subsystem has separate evidence;
5. unsupported capabilities fail closed;
6. destructive storage operations use an explicit safety plan and disposable
   media where applicable.

Files under `drivers/boards/<board>/` do not constitute hardware support.

### Documentation-only changes

Documentation-only changes may:

- correct stale identifiers;
- align contracts with already-promoted implementation;
- clarify architecture;
- downgrade unsupported language;
- record investigation state;
- add a risk discovered during validation;
- reorganize roadmap dependencies or operating guidance.

They may not upgrade runtime evidence. Newly discovered correctness, release, or
maintenance risks belong in `TECHNICAL_RISKS.md`.

## Update workflow

For every behavior-changing change:

1. identify the affected public claim and risk;
2. add or update the smallest relevant automated test;
3. run focused checks;
4. run `bash tools/verify.sh` before promotion;
5. update `TECHNICAL_RISKS.md` when risk state changes;
6. update `CURRENT_STATE.md` with exact evidence;
7. update architecture, runtime, memory, ABI, porting, or roadmap documents only
   where their contract or ordering changed;
8. update `DEVELOPMENT_GUIDE.md`, `CONTRIBUTING.md`, and `AGENTS.md` when operating
   rules changed;
9. update README last.

The author of a behavior-changing change owns its tests and documentation. No
separate documentation team is assumed.

## Closing or accepting a risk

### Closing

A risk may move to `CLOSED` only when:

- its exit criteria are satisfied;
- exact evidence is named;
- the affected release claim is updated;
- follow-up boundaries remain explicit.

“Fixed”, “looks good”, “rerun passed”, or “all tests pass” is insufficient without
scope and identity.

### Accepting for a release

A P1 residual may be marked `ACCEPTED FOR <release>` only when:

- the remaining behavior is understood well enough to bound the claim;
- the release impact and affected users/workloads are stated;
- the rationale for accepting instead of fixing is recorded;
- detection/mitigation exists where practical;
- a follow-up issue or milestone is named when work remains;
- the release notes repeat the limitation.

Acceptance is not closure. The risk remains traceable.

Closed and accepted risks stay in the register. Do not create a dated archive by
default.

## Release claims

A version may be called a **release candidate** only when:

- every mandatory gate has deterministic pass/fail behavior;
- all P0 risks for that release are closed;
- every P1 risk is closed or explicitly accepted with rationale;
- required visible/manual workflows have dated results;
- README, current state, risks, roadmap, architecture, runtime, and ABI documents
  agree;
- the exact tested tree and release artifact are identified;
- unsupported hardware/product claims remain explicit non-claims.

Until those conditions are met, use `alpha`, `experimental`, `baseline`,
`hardening candidate`, or `promotion candidate` as appropriate.

## Verification record template

```text
Date:
Implementation commit/tree:
PR head and merge commit (if applicable):
Documentation branch/PR (if different):
Environment:

Commands or hosted runs:
- command/run ID -> result and asserted marker/contract

Manual checks:
- workflow -> result, tester, exact commit/image

Investigation evidence:
- branch/run -> result, sample/workload, evidence boundary

Not run:
- command/check -> reason

Known limitations:
- limitation and affected scope

Evidence notes:
- main, PR head, synthetic merge, local tree, QEMU, test image, manual, hardware
```
