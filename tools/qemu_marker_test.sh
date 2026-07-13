#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
KERNEL_BIN="${KERNEL_BIN:-$ROOT_DIR/build/kernel.bin}"
TIMEOUT="${QEMU_TEST_TIMEOUT:-25s}"
QEMU="${QEMU_SYSTEM_AARCH64:-qemu-system-aarch64}"

fail() {
    printf 'FAIL: %s\n' "$*" >&2
    exit 1
}

require_command() {
    command -v "$1" >/dev/null 2>&1 || fail "required command not found: $1"
}

require_marker() {
    local log="$1"
    local marker="$2"

    if ! grep -Fq "$marker" "$log"; then
        printf 'missing marker: %s\n' "$marker" >&2
        cat "$log" >&2 || true
        exit 1
    fi
}

run_vm() {
    local name="$1"
    shift
    local log="$ROOT_DIR/build/qemu-${name}-test.log"
    local status=0

    mkdir -p "$(dirname "$log")"
    rm -f "$log"

    timeout "$TIMEOUT" "$QEMU" \
        -machine virt -cpu cortex-a72 -m 128M \
        -display none -serial "file:$log" -monitor none \
        -global virtio-mmio.force-legacy=false \
        -kernel "$KERNEL_BIN" \
        "$@" \
        >/dev/null 2>&1 || status=$?

    if [[ "$status" -ne 0 && "$status" -ne 124 ]]; then
        cat "$log" >&2 || true
        fail "$name QEMU exited with status $status"
    fi

    [[ -s "$log" ]] || fail "$name produced no serial log"
    printf '%s\n' "$log"
}

run_fb() {
    local log
    log="$(run_vm fb -device virtio-gpu-device,xres=640,yres=480)"
    require_marker "$log" "VIRTIO gpu: windows"
    require_marker "$log" "panel: ready"
    printf 'PASS: qemu-fb markers (%s)\n' "$log"
}

run_usb() {
    local log
    log="$(run_vm usb \
        -device qemu-xhci,id=xhci \
        -device usb-kbd,bus=xhci.0 \
        -device usb-mouse,bus=xhci.0)"
    require_marker "$log" "USB: controller initialized"
    require_marker "$log" "USB: enumeration ok"
    require_marker "$log" "USB HID: 2 devices"
    printf 'PASS: qemu-usb markers (%s)\n' "$log"
}

run_net() {
    local log
    log="$(run_vm net \
        -netdev user,id=net0 \
        -device virtio-net-device,netdev=net0)"
    require_marker "$log" "network: initialized"
    require_marker "$log" "[net] DHCP ack:"
    printf 'PASS: qemu-net markers (%s)\n' "$log"
}

require_command timeout
require_command "$QEMU"
[[ -f "$KERNEL_BIN" ]] || fail "kernel image not found: $KERNEL_BIN (run make first)"

case "${1:-}" in
    fb)
        run_fb
        ;;
    usb)
        run_usb
        ;;
    net)
        run_net
        ;;
    all)
        run_fb
        run_usb
        run_net
        ;;
    *)
        printf 'usage: %s {fb|usb|net|all}\n' "$0" >&2
        exit 2
        ;;
esac
