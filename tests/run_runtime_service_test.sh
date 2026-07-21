#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${repo_root}/build-runtime-service-test"
binary="${build_dir}/runtime_service_test"
timer_source="${repo_root}/kernel/timer/timer.c"
network_source="${repo_root}/drivers/net/virtio_net.c"
usb_hid_source="${repo_root}/drivers/usb/hid_driver.c"

rm -rf "${build_dir}"
mkdir -p "${build_dir}"

${HOST_CC:-cc} \
    -std=c11 -Wall -Wextra -Werror -DARMONIOS_TEST \
    -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -I"${repo_root}" -I"${repo_root}/drivers" \
    "${repo_root}/tests/runtime_service_test.c" \
    "${repo_root}/kernel/irq.c" \
    -o "${binary}"

"${binary}"

if grep -Eq 'uart_pump_input|kernel_on_timer_tick|kernel_io_poll_|board_input_poll|usb_hid_poll_all|gui_|net_poll' "${timer_source}"; then
    echo "timer IRQ contains forbidden runtime work" >&2
    exit 1
fi

grep -q 'runtime_service_request(RUNTIME_WORK_PERIODIC)' "${timer_source}"
grep -q 'runtime_service_report_metric(RUNTIME_METRIC_NETWORK_FRAMES, 1U)' "${network_source}"
grep -q 'runtime_service_report_metric(RUNTIME_METRIC_DEVICE_POLLS, 1U)' "${usb_hid_source}"
echo "timer IRQ deferred-work boundary and device/network metric wiring: ok"
