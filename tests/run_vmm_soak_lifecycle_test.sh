#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TEST_DIR="$ROOT_DIR/build/tests/vmm-soak-lifecycle"
ARTIFACT_DIR="$TEST_DIR/artifacts"
OUTPUT_DIR="$TEST_DIR/output"
BIN_DIR="$TEST_DIR/bin"
RUN_LOG="$TEST_DIR/run.log"

rm -rf "$TEST_DIR"
mkdir -p "$ARTIFACT_DIR" "$BIN_DIR"
: >"$ARTIFACT_DIR/kernel.elf"
: >"$ARTIFACT_DIR/kernel.bin"
: >"$ARTIFACT_DIR/virtio-blk.img"

cat >"$BIN_DIR/fake-qemu" <<'PY'
#!/usr/bin/env python3
import os
import socket
import sys
import time

if "--version" in sys.argv:
    print("QEMU emulator version lifecycle-test")
    raise SystemExit(0)

serial_arg = sys.argv[sys.argv.index("-serial") + 1]
monitor_arg = sys.argv[sys.argv.index("-monitor") + 1]
serial_path = serial_arg.removeprefix("file:")
monitor_path = monitor_arg.split(",", 1)[0].removeprefix("unix:")

with open(serial_path, "w", encoding="utf-8") as stream:
    stream.write("storage: initialized\n")
    stream.write("FAT32 root: mounted\n")
    stream.write("panel: starting\n")

try:
    os.unlink(monitor_path)
except FileNotFoundError:
    pass

server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
server.bind(monitor_path)
server.listen(1)
connection, _ = server.accept()
connection.recv(1024)  # Deliberately ignore the monitor quit command.
while True:
    time.sleep(1)
PY

cat >"$BIN_DIR/fake-gdb" <<'EOF_GDB'
#!/usr/bin/env bash
exit 0
EOF_GDB

cat >"$BIN_DIR/fake-addr2line" <<'EOF_ADDR'
#!/usr/bin/env bash
exit 0
EOF_ADDR

chmod +x "$BIN_DIR/fake-qemu" "$BIN_DIR/fake-gdb" \
    "$BIN_DIR/fake-addr2line"

if ! VMM_SOAK_KERNEL_BUILD_DIR="$ARTIFACT_DIR" \
     VMM_SOAK_OUTPUT_DIR="$OUTPUT_DIR" \
     VMM_SOAK_BOOT_COUNT=2 \
     VMM_SOAK_BOOT_SECONDS=1 \
     QEMU_SYSTEM_AARCH64="$BIN_DIR/fake-qemu" \
     GDB_MULTIARCH="$BIN_DIR/fake-gdb" \
     AARCH64_ADDR2LINE="$BIN_DIR/fake-addr2line" \
     bash "$ROOT_DIR/tools/qemu_fat32_vmm_soak_test.sh" \
     >"$RUN_LOG" 2>&1; then
    cat "$RUN_LOG" >&2
    exit 1
fi

grep -Fq "PASS: FAT32 VMM soak boot 1/2" "$RUN_LOG"
grep -Fq "PASS: FAT32 VMM soak boot 2/2" "$RUN_LOG"
grep -Fq "PASS: 2 repeated production FAT32 boots" "$RUN_LOG"
[[ -f "$OUTPUT_DIR/teardown-01.forced" ]]
[[ -f "$OUTPUT_DIR/teardown-02.forced" ]]
[[ -s "$OUTPUT_DIR/boot-01.log" ]]
[[ -s "$OUTPUT_DIR/boot-02.log" ]]

printf 'vmm-soak-lifecycle-test: PASS\n'
