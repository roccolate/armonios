#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${repo_root}/build-process-parent-wait-test"
binary="${build_dir}/process_parent_wait_test"

rm -rf "${build_dir}"
mkdir -p "${build_dir}"

${HOST_CC:-cc}     -std=c11 -Wall -Wextra -Werror -DARMONIOS_TEST     -I"${repo_root}" -I"${repo_root}/drivers"     "${repo_root}/tests/process_parent_wait_test.c"     "${repo_root}/kernel/process.c"     "${repo_root}/kernel/process_lifecycle.c"     -o "${binary}"

"${binary}"
