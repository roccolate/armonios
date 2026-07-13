#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

make
bash tools/qemu_marker_test.sh all

printf '\nALL QEMU MARKER GATES PASSED\n'
printf 'Manual desktop verification is still required with: make qemu-fb-visible\n'
