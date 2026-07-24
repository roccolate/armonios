#!/usr/bin/env bash

# Build the SDK example, place it in a separate FAT32 image, and prove the
# operation neither rebuilds nor changes kernel.bin. The image is then parsed
# independently and HELLO.KLI is compared byte-for-byte with the SDK artifact.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="$ROOT_DIR/build/tests/external-kli-fat"
BUILD_DIR="$ROOT_DIR/build"
SDK_DIR="$BUILD_DIR/sdk"
HELLO_KLI="$SDK_DIR/examples/hello-console/build/HELLO.KLI"
FAT_IMAGE="$OUT_DIR/external-kli.img"
MKFAT="$OUT_DIR/mkfat32_image"
KERNEL_BIN="$BUILD_DIR/kernel.bin"
SHELL_BIN="$BUILD_DIR/programs/apps/shell.bin"
HOST_CC="${HOST_CC:-cc}"

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

make -C "$ROOT_DIR" BOARD=qemu_virt
make -C "$ROOT_DIR" sdk
make -C "$SDK_DIR/examples/hello-console" clean all \
    SDK="$SDK_DIR"

if [[ ! -f "$KERNEL_BIN" || ! -f "$SHELL_BIN" || ! -f "$HELLO_KLI" ]]; then
    echo 'FAIL: required kernel, Shell, or SDK KLI artifact is missing' >&2
    exit 1
fi

kernel_before="$(sha256sum "$KERNEL_BIN" | awk '{print $1}')"
kernel_mtime_before="$(stat -c %Y "$KERNEL_BIN")"

"$HOST_CC" -std=c11 -Wall -Wextra -Werror -O2 \
    "$ROOT_DIR/tools/mkfat32_image.c" -o "$MKFAT"
"$MKFAT" "$FAT_IMAGE" "$SHELL_BIN" "$HELLO_KLI"

kernel_after="$(sha256sum "$KERNEL_BIN" | awk '{print $1}')"
kernel_mtime_after="$(stat -c %Y "$KERNEL_BIN")"
if [[ "$kernel_before" != "$kernel_after" ||
      "$kernel_mtime_before" != "$kernel_mtime_after" ]]; then
    echo 'FAIL: creating the external-app FAT image changed kernel.bin' >&2
    exit 1
fi

python3 - "$FAT_IMAGE" "$HELLO_KLI" <<'PY'
from pathlib import Path
import struct
import sys

image_path = Path(sys.argv[1])
kli_path = Path(sys.argv[2])
image = image_path.read_bytes()
expected = kli_path.read_bytes()

if len(image) < 512 or image[510:512] != b"\x55\xaa":
    raise SystemExit("FAIL: generated image has no valid boot signature")

bps = struct.unpack_from("<H", image, 11)[0]
spc = image[13]
reserved = struct.unpack_from("<H", image, 14)[0]
fats = image[16]
fat_sectors = struct.unpack_from("<I", image, 36)[0]
root_cluster = struct.unpack_from("<I", image, 44)[0]

if bps != 512 or spc == 0 or fats == 0 or fat_sectors == 0:
    raise SystemExit("FAIL: unsupported generated FAT32 geometry")

data_start = reserved + fats * fat_sectors
fat_offset = reserved * bps
root_sector = data_start + (root_cluster - 2) * spc
root_offset = root_sector * bps
root_size = spc * bps
root = image[root_offset:root_offset + root_size]

wanted = b"HELLO   KLI"
entry = None
for offset in range(0, len(root), 32):
    candidate = root[offset:offset + 32]
    if len(candidate) < 32 or candidate[0] in (0x00, 0xE5):
        continue
    if candidate[:11] == wanted:
        entry = candidate
        break

if entry is None:
    raise SystemExit("FAIL: HELLO.KLI is absent from FAT32 root")

first_cluster = (
    struct.unpack_from("<H", entry, 20)[0] << 16
) | struct.unpack_from("<H", entry, 26)[0]
size = struct.unpack_from("<I", entry, 28)[0]
if size != len(expected) or first_cluster < 2:
    raise SystemExit("FAIL: HELLO.KLI directory metadata is incorrect")

payload = bytearray()
cluster = first_cluster
seen = set()
while len(payload) < size:
    if cluster in seen or cluster < 2:
        raise SystemExit("FAIL: invalid or cyclic HELLO.KLI cluster chain")
    seen.add(cluster)
    sector = data_start + (cluster - 2) * spc
    start = sector * bps
    payload.extend(image[start:start + spc * bps])
    next_value = struct.unpack_from("<I", image, fat_offset + cluster * 4)[0]
    next_value &= 0x0FFFFFFF
    if next_value >= 0x0FFFFFF8:
        break
    cluster = next_value

actual = bytes(payload[:size])
if actual != expected:
    raise SystemExit("FAIL: HELLO.KLI FAT payload differs from SDK artifact")

print(f"PASS: FAT32 contains byte-identical HELLO.KLI ({size} bytes)")
PY

printf 'PASS: external FAT image created without rebuilding kernel.bin\n'
