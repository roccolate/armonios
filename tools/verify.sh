#!/usr/bin/env bash

set -euo pipefail

run_gate() {
    local name="$1"
    shift

    printf '\n==> %s\n' "$name"
    "$@"
    printf 'PASS: %s\n' "$name"
}

printf 'ArmoniOS verification\n'
printf 'commit: %s\n' "$(git rev-parse --verify HEAD 2>/dev/null || printf unknown)"
printf 'date: %s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)"

run_gate build make
run_gate size make size
run_gate host-tests make -C tests test
run_gate process-fd-isolation bash tests/run_vfs_process_fd_test.sh
run_gate usercopy-host bash tests/run_user_copy_permissions_test.sh
run_gate stack-check make stack-check
run_gate qemu-fs-test make qemu-fs-test
run_gate usercopy-qemu bash tools/qemu_usercopy_test.sh

printf '\nALL AUTOMATED BASELINE GATES PASSED\n'
printf 'Manual desktop verification is still required with: make qemu-fb-visible\n'
