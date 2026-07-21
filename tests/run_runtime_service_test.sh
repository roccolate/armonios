#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${repo_root}/build-runtime-service-test"
binary="${build_dir}/runtime_service_test"
input_binary="${build_dir}/runtime_input_budget_test"
timer_source="${repo_root}/kernel/timer/timer.c"
runtime_source="${repo_root}/kernel/irq.c"
io_service_source="${repo_root}/kernel/io_service.h"
network_source="${repo_root}/drivers/net/virtio_net.c"
usb_hid_source="${repo_root}/drivers/usb/hid_driver.c"
display_source="${repo_root}/drivers/boards/qemu_virt/board.c"

rm -rf "${build_dir}"
mkdir -p "${build_dir}"

common_flags=(
    -std=c11 -Wall -Wextra -Werror -DARMONIOS_TEST
    -ffunction-sections -fdata-sections -Wl,--gc-sections
    -I"${repo_root}" -I"${repo_root}/drivers"
)

${HOST_CC:-cc} "${common_flags[@]}" \
    "${repo_root}/tests/runtime_service_test.c" \
    "${repo_root}/kernel/irq.c" \
    -o "${binary}"

${HOST_CC:-cc} "${common_flags[@]}" \
    "${repo_root}/tests/runtime_input_budget_test.c" \
    "${repo_root}/kernel/irq.c" \
    -o "${input_binary}"

"${binary}"
"${input_binary}"

if grep -Eq 'uart_pump_input|kernel_on_timer_tick|kernel_io_poll_|board_input_poll|usb_hid_poll_all|gui_|net_poll' "${timer_source}"; then
    echo "timer IRQ contains forbidden runtime work" >&2
    exit 1
fi

grep -Fq 'RUNTIME_WORK_PERIODIC | RUNTIME_WORK_INPUT' "${timer_source}"
grep -Fq 'RUNTIME_WORK_NETWORK' "${timer_source}"
grep -Fq '#define input_queue_poll runtime_service_input_poll' "${io_service_source}"
grep -Fq 'runtime_service_request(RUNTIME_WORK_INPUT)' "${runtime_source}"
grep -Fq 'RUNTIME_INPUT_EVENT_BUDGET' "${runtime_source}"
grep -Fq 'runtime_service_request(RUNTIME_WORK_NETWORK)' "${runtime_source}"
grep -Fq 'RUNTIME_NETWORK_FRAME_BUDGET' "${runtime_source}"
grep -Fq 'runtime_service_report_metric(RUNTIME_METRIC_NETWORK_FRAMES, 1U)' "${network_source}"
grep -Fq 'runtime_service_report_metric(RUNTIME_METRIC_DEVICE_POLLS, 1U)' "${usb_hid_source}"
grep -Fq 'runtime_service_report_redraw()' "${display_source}"
grep -Fq 'RUNTIME_METRIC_DAMAGE_ITEMS' "${runtime_source}"
grep -Fq 'RUNTIME_METRIC_FULL_REDRAWS' "${runtime_source}"
echo "timer IRQ boundary, input/network budgets, and runtime metric wiring: ok"
