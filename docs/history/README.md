# Historical documentation

This directory preserves release evidence, audit provenance, and milestone records
that should not compete with live documentation.

Use the current documents for present-tense questions:

- `../CURRENT_STATE.md` — current `main` capability;
- `../ARCHITECTURE.md` — implemented design;
- `../ROADMAP.md` — remaining work;
- `../TECHNICAL_RISKS.md` — active risks;
- focused references such as `../SYSCALLS.md` and `../RUNTIME_SERVICE.md` — current contracts.

Historical records may contain commit hashes, workflow IDs, PR numbers, measured
sizes, and dated observations. Those facts apply only to the exact tree and test
scope named by the record.

## Records

- `V02_RUNTIME_EVIDENCE.md` — bounded runtime-service implementation and stress
  evidence used to close the v0.2 runtime work;
- `V03_FOUNDATION_PROVENANCE.md` — provenance for the storage/VFS foundations
  summarized by the live v0.3 status document.

## Rules

- Historical records are not silently rewritten to describe current `main`.
- Corrections should preserve the original scope and explain the correction.
- Current capability changes belong in live documents, not in a new progress log.
- Release-specific records should identify exact code, commands or workflow runs,
  environment, result, limitations, and checks not run.
