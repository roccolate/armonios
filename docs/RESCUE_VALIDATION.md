# Rescue validation runbook

This runbook is the decision procedure for the two v1.0 P0 fixes:

- PR #16 — writable EL0 output-buffer enforcement;
- PR #14 — process-local VFS descriptors and exit cleanup.

Do not merge either P0 from static inspection alone.

## One-command validation

From an up-to-date checkout of `main` with the AArch64 toolchain and QEMU
installed:

```sh
git switch main
git pull --ff-only
bash tools/rescue_validate_prs.sh
```

The command fetches the exact current PR heads. It does not trust stale local
branches and it does not modify the current checkout.

## What the runner does

The runner creates temporary isolated worktrees and performs these phases:

1. PR #14 focused descriptor-isolation tests;
2. PR #14 complete automated baseline;
3. PR #16 focused user-copy permission tests;
4. PR #16 complete automated baseline;
5. PR #16 real EL0 read-only-output QEMU regression;
6. temporary integration of PR #16 followed by PR #14;
7. automatic resolution of only the known adjacent `tools/verify.sh` gate
   insertion;
8. rejection of any other integration conflict;
9. both focused tests on the combined tree;
10. combined automated baseline;
11. combined EL0 user-copy QEMU regression;
12. framebuffer, USB, and DHCP serial-marker gates.

The temporary worktrees are deleted when the command ends. The logs remain in:

```text
build/rescue-validation/<UTC timestamp>/
```

## Automated pass criteria

The command succeeds only when all of the following are true:

- the kernel and all shipping apps build;
- `make size` remains below the configured binary limit;
- the full host suite passes;
- the user-copy permission regression passes;
- the descriptor isolation and process-exit cleanup regressions pass;
- stack usage remains within the configured limit;
- the FAT32 QEMU smoke test passes;
- at least two EL0 processes receive `ERR_PERM` when using RX image memory as
  an output destination;
- the panel remains alive after the user-copy probe;
- another process reaches `clock: starting`;
- framebuffer markers reach GPU windows and panel readiness;
- USB reaches controller initialization, enumeration, and two HID devices;
- network initialization reaches a DHCP ACK.

A timeout without the required marker is a failure.

## Manual visible gate

After the automated runner passes, execute the combined candidate in visible
QEMU and verify:

1. the desktop and panel appear;
2. `files` exposes `/fat`;
3. create an 8.3 file such as `TEST.TXT`;
4. open it in `editor`;
5. type immediately without clicking the editor first;
6. save with `Ctrl-S`;
7. close the editor;
8. rename the file;
9. reopen it and confirm the contents;
10. delete it and confirm it disappears;
11. open `monitor` and confirm the desktop remains responsive;
12. confirm there was no EL1 fault, scheduler stall, or blank compositor frame.

Record the tester name, exact commit, date, and result.

## Merge order after a complete pass

1. Mark PR #16 ready and merge it with squash.
2. Synchronize PR #14 with the new `main`.
3. Preserve both focused lines in `tools/verify.sh`:

```sh
run_gate user-copy-permissions bash tests/run_user_copy_permissions_test.sh
run_gate process-fd-isolation bash tests/run_vfs_process_fd_test.sh
```

4. Rerun the focused descriptor test and the full baseline on the synchronized
   PR #14 head.
5. Merge PR #14 with squash.
6. Run `bash tools/verify.sh`, `bash tools/qemu_usercopy_test.sh`, and
   `bash tools/verify_qemu.sh` on final `main`.
7. Repeat the manual visible gate on final `main`.
8. Only then move RISK-001 and RISK-002 to closed and prepare `v1.0-rc1`.

## Failure handling

Do not weaken a marker or remove a regression to obtain a pass.

- Focused-test failure: fix the owning P0 branch.
- Build or host-suite failure: treat the PR as blocked.
- FAT32 smoke failure: preserve the serial log and diagnose storage/VFS.
- EL0 probe failure: preserve `qemu-usercopy-test.log`; do not merge PR #16.
- Framebuffer, USB, or DHCP marker failure: preserve the subsystem log and keep
  RISK-005 open.
- Unexpected integration conflict: the runner stops instead of guessing a
  resolution.
- Visible workflow failure: keep RISK-003 or RISK-004 open as applicable.

## Evidence template

Post this information to issue #6 and the affected P0 issue:

```text
Tester:
UTC date:
Main commit used to start:
PR #16 head:
PR #14 head:
Temporary integration commit:

rescue_validate_prs.sh: PASS / FAIL
manual visible workflow: PASS / FAIL

Log directory:
First failing gate, if any:
Observed limitation:
```

Code presence, a mergeable PR, or a synthetic test is not release evidence.
Only the exact commands and recorded runtime results can promote the project to
`v1.0-rc1`.
