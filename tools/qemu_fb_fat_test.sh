#!/usr/bin/env bash

# Boots the visible-desktop kernel with the FAT32 virtio block image attached
# and asserts that both the GPU window surface and the /fat mount are wired
# before `panel: ready`. This is the deterministic half of RISK-003: it
# proves the visible desktop target can present a populated /fat filesystem to
# the panel. The interactive create/edit/save/rename/reopen/delete workflow
# it presents still requires a named human tester on a real QEMU display
# (see docs/TECHNICAL_RISKS.md RISK-003 exit criteria).

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
KERNEL_BIN="${KERNEL_BIN:-$ROOT_DIR/build/kernel.bin}"
BLK_IMG="${BLK_IMG:-$ROOT_DIR/build/virtio-blk.img}"
LOG="${FB_FAT_LOG:-$ROOT_DIR/build/qemu-fb-fat-test.log}"
TIMEOUT="${QEMU_TEST_TIMEOUT:-25s}"
QEMU="${QEMU_SYSTEM_AARCH64:-qemu-system-aarch64}"

fail() {
    printf 'FAIL: %s\n' "$*" >&2
    if [[ -f "$LOG" ]]; then
        cat "$LOG" >&2 || true
    fi
    exit 1
}

command -v timeout >/dev/null 2>&1 || fail "required command not found: timeout"
command -v "$QEMU" >/dev/null 2>&1 || fail "required command not found: $QEMU"
[[ -f "$KERNEL_BIN" ]] || fail "kernel image not found: $KERNEL_BIN"
[[ -f "$BLK_IMG" ]] || fail "block image not found: $BLK_IMG (run make first)"

mkdir -p "$(dirname "$LOG")"
rm -f "$LOG"
status=0

timeout "$TIMEOUT" "$QEMU" \
    -machine virt -cpu cortex-a72 -m 128M \
    -display none -serial "file:$LOG" -monitor none \
    -global virtio-mmio.force-legacy=false \
    -kernel "$KERNEL_BIN" \
    -drive file="$BLK_IMG",if=none,format=raw,id=hd0 \
    -device virtio-blk-device,drive=hd0 \
    -device virtio-gpu-device,xres=640,yres=480 \
    >/dev/null 2>&1 || status=$?

if [[ "$status" -ne 0 && "$status" -ne 124 ]]; then
    fail "QEMU exited with status $status"
fi

[[ -s "$LOG" ]] || fail "QEMU produced no serial log"

grep -Fq "FAT32: mounted" "$LOG" || fail "FAT32 was not mounted in the visible-desktop target"
grep -Fq "FAT32 root: mounted" "$LOG" || fail "FAT32 root was not exposed under /fat"
grep -Fq "display: windows" "$LOG" || fail "display window surface did not initialise"
grep -Fq "panel: ready" "$LOG" || fail "panel never reported ready"

printf 'PASS: visible-desktop wiring (FAT + GPU + panel): %s\n' "$LOG"
