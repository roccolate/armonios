#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(git rev-parse --show-toplevel 2>/dev/null)" || {
    printf 'FAIL: run this command inside the ArmoniOS repository\n' >&2
    exit 1
}
cd "$ROOT_DIR"

STAMP="$(date -u +%Y%m%dT%H%M%SZ)"
LOG_DIR="${RESCUE_LOG_DIR:-$ROOT_DIR/build/rescue-validation/$STAMP}"
WORK_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/armonios-rescue.XXXXXX")"
PR14_DIR="$WORK_ROOT/pr14-fd-isolation"
PR16_DIR="$WORK_ROOT/pr16-usercopy"
INTEGRATION_DIR="$WORK_ROOT/integration"
INTEGRATION_BRANCH="rescue/integration-$STAMP-$$"

cleanup() {
    git -C "$ROOT_DIR" worktree remove --force "$PR14_DIR" >/dev/null 2>&1 || true
    git -C "$ROOT_DIR" worktree remove --force "$PR16_DIR" >/dev/null 2>&1 || true
    git -C "$ROOT_DIR" worktree remove --force "$INTEGRATION_DIR" >/dev/null 2>&1 || true
    rm -rf "$WORK_ROOT"
}
trap cleanup EXIT INT TERM

mkdir -p "$LOG_DIR"

run_logged() {
    local name="$1"
    local directory="$2"
    shift 2

    printf '\n==> %s\n' "$name"
    (
        cd "$directory"
        set -o pipefail
        "$@" 2>&1 | tee "$LOG_DIR/$name.log"
    )
    printf 'PASS: %s\n' "$name"
}

fail() {
    printf 'FAIL: %s\n' "$*" >&2
    printf 'logs: %s\n' "$LOG_DIR" >&2
    exit 1
}

require_command() {
    command -v "$1" >/dev/null 2>&1 || fail "required command not found: $1"
}

merge_into_integration() {
    local ref="$1"
    local label="$2"

    if git -C "$INTEGRATION_DIR" merge --no-ff --no-edit "$ref"; then
        return 0
    fi

    mapfile -t conflicts < <(
        git -C "$INTEGRATION_DIR" diff --name-only --diff-filter=U
    )

    if [[ "${#conflicts[@]}" -ne 1 || "${conflicts[0]}" != "tools/verify.sh" ]]; then
        printf 'Unexpected conflicts while merging %s:\n' "$label" >&2
        printf '  %s\n' "${conflicts[@]:-<none>}" >&2
        git -C "$INTEGRATION_DIR" merge --abort >/dev/null 2>&1 || true
        fail "integration merge requires manual review"
    fi

    git -C "$INTEGRATION_DIR" checkout --ours -- tools/verify.sh
    if ! grep -Fq \
        'run_gate user-copy-permissions bash tests/run_user_copy_permissions_test.sh' \
        "$INTEGRATION_DIR/tools/verify.sh"; then
        sed -i \
            '/run_gate host-tests make -C tests test/a run_gate user-copy-permissions bash tests/run_user_copy_permissions_test.sh' \
            "$INTEGRATION_DIR/tools/verify.sh"
    fi
    if ! grep -Fq \
        'run_gate process-fd-isolation bash tests/run_vfs_process_fd_test.sh' \
        "$INTEGRATION_DIR/tools/verify.sh"; then
        sed -i \
            '/run_gate user-copy-permissions bash tests\/run_user_copy_permissions_test.sh/a run_gate process-fd-isolation bash tests/run_vfs_process_fd_test.sh' \
            "$INTEGRATION_DIR/tools/verify.sh"
    fi

    grep -Fq \
        'run_gate user-copy-permissions bash tests/run_user_copy_permissions_test.sh' \
        "$INTEGRATION_DIR/tools/verify.sh" || \
        fail "combined verify script lost the user-copy gate"
    grep -Fq \
        'run_gate process-fd-isolation bash tests/run_vfs_process_fd_test.sh' \
        "$INTEGRATION_DIR/tools/verify.sh" || \
        fail "combined verify script lost the descriptor gate"

    git -C "$INTEGRATION_DIR" add tools/verify.sh
    git -C "$INTEGRATION_DIR" commit --no-edit
}

require_command git
require_command bash
require_command make
require_command tee

printf 'ArmoniOS rescue validation\n'
printf 'started: %s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
printf 'logs: %s\n' "$LOG_DIR"

# Fetch the exact open PR heads rather than trusting stale local branches.
git fetch origin \
    +refs/heads/main:refs/remotes/origin/main \
    +refs/pull/14/head:refs/remotes/origin/rescue-pr-14 \
    +refs/pull/16/head:refs/remotes/origin/rescue-pr-16

git worktree add --detach "$PR14_DIR" refs/remotes/origin/rescue-pr-14
git worktree add --detach "$PR16_DIR" refs/remotes/origin/rescue-pr-16

run_logged pr14-focused "$PR14_DIR" \
    bash tests/run_vfs_process_fd_test.sh
run_logged pr14-baseline "$PR14_DIR" bash tools/verify.sh

run_logged pr16-focused "$PR16_DIR" \
    bash tests/run_user_copy_permissions_test.sh
run_logged pr16-baseline "$PR16_DIR" bash tools/verify.sh
run_logged pr16-el0-qemu "$PR16_DIR" bash tools/qemu_usercopy_test.sh

# Integrate permission enforcement first, then descriptor isolation. The only
# expected textual conflict is the adjacent gate insertion in tools/verify.sh.
git worktree add -b "$INTEGRATION_BRANCH" \
    "$INTEGRATION_DIR" refs/remotes/origin/main
git -C "$INTEGRATION_DIR" config user.name "ArmoniOS Rescue Validator"
git -C "$INTEGRATION_DIR" config user.email "rescue-validator@localhost"

merge_into_integration refs/remotes/origin/rescue-pr-16 "PR #16"
merge_into_integration refs/remotes/origin/rescue-pr-14 "PR #14"

run_logged integration-focused-usercopy "$INTEGRATION_DIR" \
    bash tests/run_user_copy_permissions_test.sh
run_logged integration-focused-fds "$INTEGRATION_DIR" \
    bash tests/run_vfs_process_fd_test.sh
run_logged integration-baseline "$INTEGRATION_DIR" bash tools/verify.sh
run_logged integration-el0-qemu "$INTEGRATION_DIR" \
    bash tools/qemu_usercopy_test.sh
run_logged integration-qemu-markers "$INTEGRATION_DIR" \
    bash tools/verify_qemu.sh

INTEGRATION_SHA="$(git -C "$INTEGRATION_DIR" rev-parse HEAD)"
printf '\nALL AUTOMATED RESCUE GATES PASSED\n'
printf 'integration commit: %s\n' "$INTEGRATION_SHA"
printf 'logs: %s\n' "$LOG_DIR"
printf 'remaining manual gate: make qemu-fb-visible\n'
printf 'workflow: files -> create -> editor -> save -> rename -> reopen -> delete\n'
