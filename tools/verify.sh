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

# Establish the QEMU virt tree first so size, stack-check, and every QEMU
# gate operate on a consistent kernel. The board-rpi4 gate runs in its own
# build-rpi4/ directory so its artefact never clobbers the qemu one.
run_gate build make BOARD=qemu_virt
run_gate size make BOARD=qemu_virt size
run_gate board-rpi4 bash tests/run_board_build_test.sh
run_gate rpi4-probe-package bash tools/package_rpi4_emmc2_probe.sh
run_gate emmc-sdhci-host bash tests/run_emmc_sdhci_test.sh
run_gate rpi4-emmc2-diag-host bash tests/run_rpi4_emmc2_probe_diag_test.sh
run_gate rpi-mailbox-host bash tests/run_rpi_mailbox_test.sh
run_gate host-tests make -C tests test
run_gate process-fd-isolation bash tests/run_vfs_process_fd_test.sh
run_gate usercopy-host bash tests/run_user_copy_permissions_test.sh
run_gate kli1-contract bash tests/run_kli1_contract_test.sh
run_gate stack-check make stack-check
run_gate qemu-fs-test make qemu-fs-test
run_gate usercopy-qemu bash tools/qemu_usercopy_test.sh
run_gate qemu-focus bash tools/qemu_focus_test.sh
run_gate qemu-el2-entry bash tools/qemu_el2_entry_test.sh
run_gate qemu-markers bash tools/qemu_marker_test.sh all
run_gate qemu-fb-fat bash tools/qemu_fb_fat_test.sh

printf '\nALL AUTOMATED BASELINE GATES PASSED\n'
printf 'Manual desktop verification is required before promoting a new baseline: make qemu-fb-visible\n'
