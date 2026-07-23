#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/tests/vfs-metadata-abi"
TARGET="${BUILD_DIR}/vfs_metadata_abi_test"

mkdir -p "${BUILD_DIR}"

${HOST_CC:-gcc} \
    -std=c11 -Wall -Wextra -Werror \
    -I"${ROOT_DIR}" -I"${ROOT_DIR}/drivers" \
    "${ROOT_DIR}/tests/test_vfs_metadata_abi.c" \
    "${ROOT_DIR}/kernel/vfs_metadata.c" \
    -o "${TARGET}"

"${TARGET}"
