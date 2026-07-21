#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${repo_root}/build-input-queue-stats-test"
binary="${build_dir}/input_queue_stats_test"

rm -rf "${build_dir}"
mkdir -p "${build_dir}"

${HOST_CC:-cc} \
    -std=c11 -Wall -Wextra -Werror -DARMONIOS_TEST \
    -I"${repo_root}" -I"${repo_root}/drivers" \
    "${repo_root}/tests/input_queue_stats_test.c" \
    "${repo_root}/drivers/input/input.c" \
    "${repo_root}/drivers/input/keymap.c" \
    -o "${binary}"

"${binary}"
