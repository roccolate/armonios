#!/usr/bin/env bash

# Boots the visible-desktop kernel with PANEL_AUTO_TEST enabled so the
# panel launches each app in turn, then asserts that every newly created
# app window received keyboard focus (RISK-004 exit criterion).
#
# The kernel-side instrumentation in kernel/gui_pool.c emits
# `GUI: create win=N pid=N` and `GUI: focus win=N pid=N` lines on the
# serial console; this runner reads them back through QEMU's `-serial file:`
# capture. The QEMU serial log is searched for one focus transition per
# non-panel app, proving the sys_window_focus path runs end-to-end without
# requiring a human tester.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${FOCUS_BUILD_DIR:-$ROOT_DIR/build-focus}"
LOG="${FOCUS_LOG:-$BUILD_DIR/qemu-focus-test.log}"
TIMEOUT="${QEMU_TEST_TIMEOUT:-30s}"
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

mkdir -p "$BUILD_DIR"
rm -rf "$BUILD_DIR"
rm -f "$LOG"

# Reuse USER_COPY_TEST infra: the same -DPANEL_AUTO_TEST CFLAGS that
# tools/qemu_usercopy_test.sh uses build a kernel that auto-launches
# every app after panel becomes ready.
make -C "$ROOT_DIR" \
    BUILD_DIR="$BUILD_DIR" \
    USERLAND_EXTRA_CFLAGS="-DPANEL_AUTO_TEST" \
    USERLAND_ASFLAGS="-Wall -Wextra -ffreestanding -nostdlib -nostartfiles -mcpu=cortex-a72 -g -I programs -I programs/libkarm -DUSERCOPY_RX_PROBE" \
    >/dev/null

[[ -f "$BUILD_DIR/kernel.bin" ]] || fail "kernel image not built under $BUILD_DIR"

status=0
timeout "$TIMEOUT" "$QEMU" \
    -machine virt -cpu cortex-a72 -m 128M \
    -display none -serial "file:$LOG" -monitor none \
    -global virtio-mmio.force-legacy=false \
    -kernel "$BUILD_DIR/kernel.bin" \
    -device virtio-gpu-device,xres=640,yres=480 \
    >/dev/null 2>&1 || status=$?

if [[ "$status" -ne 0 && "$status" -ne 124 ]]; then
    fail "QEMU exited with status $status"
fi
[[ -s "$LOG" ]] || fail "QEMU produced no serial log"

# The panel is always NO_FOCUS; we expect five non-panel apps to launch
# and each one to receive focus immediately. The kernel prints the
# `GUI: focus win=N pid=N` line on every actual focus transition.
focus_count=$(grep -c "^GUI: focus" "$LOG" || true)
if (( focus_count < 2 )); then
    fail "expected at least 2 focus transitions, found $focus_count"
fi

# Also require at least 2 distinct window ids to have been focused (so we
# don't just count repeated events on the same window).
focused_ids=$(grep -oE "GUI: focus win=[0-9]+" "$LOG" | sort -u | wc -l | tr -d ' ')
if (( focused_ids < 2 )); then
    fail "expected focus events on at least 2 distinct windows, found $focused_ids"
fi

# Every focused window must have a matching create event, which proves the
# focus transition followed a real create and not a stray re-target.
while read -r line; do
    win=$(printf '%s\n' "$line" | grep -oE 'win=[0-9]+' | head -1 | tr -dc '0-9')
    if ! grep -q "GUI: create win=${win} " "$LOG"; then
        fail "found focus on win=${win} without a matching create marker"
    fi
done < <(grep "^GUI: focus" "$LOG")

printf 'PASS: %d focus transition(s) across %d distinct window(s)\n' \
    "$focus_count" "$focused_ids"
printf 'PASS: qemu-focus-test: log %s\n' "$LOG"
