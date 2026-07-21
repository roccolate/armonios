#!/usr/bin/env python3

from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    file_path = Path(path)
    text = file_path.read_text()
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected one match, found {count}")
    file_path.write_text(text.replace(old, new, 1))


replace_once(
    "Makefile",
    """ifeq ($(RPI4_EMMC2_PROBE),1)
BOARD_CFLAGS += -DARMONIOS_RPI4_EMMC2_PROBE=1
endif
""",
    """ifeq ($(RPI4_EMMC2_PROBE),1)
BOARD_CFLAGS += -DARMONIOS_RPI4_EMMC2_PROBE=1
STORAGE_DEV += \\
    $(BUILD_DIR)/drivers/storage/block_view.o \\
    $(BUILD_DIR)/drivers/storage/mbr.o
endif
""",
)

replace_once(
    "drivers/boards/rpi4/board.c",
    """#if defined(ARMONIOS_RPI4_EMMC2_PROBE)
#include "boards/rpi4/emmc2_probe_diag.h"
#include "storage/emmc.h"
#endif
""",
    """#if defined(ARMONIOS_RPI4_EMMC2_PROBE)
#include "boards/rpi4/emmc2_probe_diag.h"
#include "kernel/fat32.h"
#include "storage/block_view.h"
#include "storage/emmc.h"
#include "storage/mbr.h"
#endif
""",
)

replace_once(
    "drivers/boards/rpi4/board.c",
    """static emmc_device_t g_emmc2_probe_device;
static rpi4_emmc2_probe_diag_t g_emmc2_probe_diag;
static uint8_t g_emmc2_sector0[EMMC_BLKSZ] __attribute__((aligned(16)));
""",
    """static emmc_device_t g_emmc2_probe_device;
static rpi4_emmc2_probe_diag_t g_emmc2_probe_diag;
static fat32_fs_t g_emmc2_probe_fat32;
static block_view_t g_emmc2_probe_view;
static uint8_t g_emmc2_sector0[EMMC_BLKSZ] __attribute__((aligned(16)));
""",
)

replace_once(
    "drivers/boards/rpi4/board.c",
    """static void rpi4_emmc2_probe_print_diag(void) {
""",
    """static int rpi4_emmc2_probe_read_sector(void *context, uint32_t lba,
                                                uint8_t *buffer) {
    return emmc_read_sector((emmc_device_t *)context, lba, 1U, buffer);
}

static int rpi4_emmc2_probe_mount_fat32(uint32_t base_lba,
                                         uint32_t sector_count) {
    if (block_view_init(&g_emmc2_probe_view,
                        rpi4_emmc2_probe_read_sector, 0,
                        &g_emmc2_probe_device, base_lba,
                        sector_count) != 0 ||
        fat32_mount(&g_emmc2_probe_fat32,
                    block_view_read_sector,
                    &g_emmc2_probe_view) != 0 ||
        g_emmc2_probe_fat32.total_sectors > sector_count) {
        g_emmc2_probe_fat32.mounted = 0U;
        return -1;
    }
    return 0;
}

static void rpi4_emmc2_probe_print_fat32(void) {
    mbr_partition_t partition;
    uint32_t layout = 0U;
    uint32_t base_lba = 0U;
    uint8_t type = 0U;

    if (rpi4_emmc2_probe_mount_fat32(0U, UINT32_MAX) != 0) {
        if (mbr_find_fat32_partition(g_emmc2_sector0, &partition) != 0) {
            uart_puts("EMMC2 fat none\\n");
            return;
        }
        layout = 1U;
        base_lba = partition.start_lba;
        type = partition.type;
        if (rpi4_emmc2_probe_mount_fat32(partition.start_lba,
                                          partition.sector_count) != 0) {
            uart_puts("EMMC2 fat bad ");
            print_dec64(base_lba);
            uart_puts("\\n");
            return;
        }
    }

    uart_puts("EMMC2 fat ");
    print_dec64(layout);
    uart_puts(" ");
    print_hex8(type);
    uart_puts(" ");
    print_dec64(base_lba);
    uart_puts(" ");
    print_dec64(g_emmc2_probe_fat32.sectors_per_cluster);
    uart_puts(" ");
    print_dec64(g_emmc2_probe_fat32.sectors_per_fat);
    uart_puts(" ");
    print_dec64(g_emmc2_probe_fat32.root_cluster);
    uart_puts(" ");
    print_dec64(g_emmc2_probe_fat32.total_sectors);
    uart_puts("\\n");
}

static void rpi4_emmc2_probe_print_diag(void) {
""",
)

replace_once(
    "drivers/boards/rpi4/board.c",
    """    uart_puts("EMMC2 probe: first16 ");
    for (uint32_t i = 0; i < 16U; i++) {
        print_hex8(g_emmc2_sector0[i]);
    }
    uart_puts("\\n");
    uart_puts("EMMC2 probe: signature ");
    print_hex8(g_emmc2_sector0[510]);
    print_hex8(g_emmc2_sector0[511]);
    uart_puts("\\n");
""",
    """    uart_puts("EMMC2 s0 ");
    for (uint32_t i = 0; i < 16U; i++) {
        print_hex8(g_emmc2_sector0[i]);
    }
    uart_puts(" ");
    print_hex8(g_emmc2_sector0[510]);
    print_hex8(g_emmc2_sector0[511]);
    uart_puts("\\n");
    rpi4_emmc2_probe_print_fat32();
""",
)

replace_once(
    "tools/verify.sh",
    """run_gate rpi4-emmc2-diag-host bash tests/run_rpi4_emmc2_probe_diag_test.sh
run_gate rpi-mailbox-host bash tests/run_rpi_mailbox_test.sh
""",
    """run_gate rpi4-emmc2-diag-host bash tests/run_rpi4_emmc2_probe_diag_test.sh
run_gate mbr-fat32-host bash tests/run_mbr_fat32_test.sh
run_gate block-view-fat32-host bash tests/run_block_view_fat32_test.sh
run_gate rpi-mailbox-host bash tests/run_rpi_mailbox_test.sh
""",
)

replace_once(
    ".github/workflows/verify.yml",
    """      - name: Test RPi4 EMMC2 probe telemetry
        run: bash tests/run_rpi4_emmc2_probe_diag_test.sh

      - name: Upload RPi4 EMMC2 probe package
""",
    """      - name: Test RPi4 EMMC2 probe telemetry
        run: bash tests/run_rpi4_emmc2_probe_diag_test.sh

      - name: Test FAT32 MBR discovery
        run: bash tests/run_mbr_fat32_test.sh

      - name: Test partition block views
        run: bash tests/run_block_view_fat32_test.sh

      - name: Upload RPi4 EMMC2 probe package
""",
)

replace_once(
    "deploy/rpi4-emmc2-probe/README.md",
    """- The probe reads exactly sector 0 and prints a bounded summary.
""",
    """- The probe reads sector 0 and, only when needed, one FAT32 boot sector.
- It validates geometry read-only and never lists files or mounts the volume.
""",
)

replace_once(
    "deploy/rpi4-emmc2-probe/README.md",
    """EMMC2 probe: read0 0
EMMC2 probe: first16 <32 hexadecimal digits>
EMMC2 probe: signature <4 hexadecimal digits>
""",
    """EMMC2 probe: read0 0
EMMC2 s0 <32 hexadecimal digits> <4 hexadecimal digits>
EMMC2 fat <layout> <type> <base-lba> <sectors-per-cluster> <sectors-per-fat> <root-cluster> <total-sectors>
""",
)

replace_once(
    "deploy/rpi4-emmc2-probe/README.md",
    """`55AA` is common for an MBR or boot sector signature, but the probe records the
actual bytes and does not treat a different value as a driver failure.
""",
    """`EMMC2 s0` records the first 16 bytes and bytes 510-511. `55AA` is common
for an MBR or FAT boot sector, but the raw bytes remain evidence rather than a
success condition.

`EMMC2 fat` uses positional fields to save image space. `layout` is `0` for a
superfloppy BPB at LBA 0 and `1` for a primary MBR partition. `type` is `00` for
superfloppy or the MBR type byte (`0B`, `0C`, `1B`, or `1C`). The remaining
values come from the existing FAT32 mount validator. `EMMC2 fat none` means no
valid superfloppy BPB or supported primary FAT32 partition was found;
`EMMC2 fat bad <lba>` means MBR discovery succeeded but that partition's BPB or
geometry was rejected.
""",
)

replace_once(
    "docs/RPI4_EMMC2.md",
    """- **RPi4 probe failure telemetry:** IMPLEMENTED; HOST-VERIFIED.
- **EL2 to EL1 boot entry:** IMPLEMENTED; QEMU-VERIFIED.
""",
    """- **RPi4 probe failure telemetry:** IMPLEMENTED; HOST-VERIFIED.
- **Primary FAT32 MBR discovery:** IMPLEMENTED; HOST-VERIFIED.
- **Bounded partition block views:** IMPLEMENTED; HOST-VERIFIED.
- **Read-only FAT layout probe:** IMPLEMENTED; BUILD-VERIFIED.
- **EL2 to EL1 boot entry:** IMPLEMENTED; QEMU-VERIFIED.
""",
)

replace_once(
    "docs/RPI4_EMMC2.md",
    """4. read exactly sector 0;
5. print compact MMIO telemetry on failure;
6. print the first 16 bytes and bytes 510-511 on success.
""",
    """4. read sector 0 and print a compact raw summary;
5. try the existing FAT32 validator at LBA 0;
6. otherwise locate the first supported primary FAT32 MBR partition;
7. apply a bounded block view and validate that partition's BPB;
8. print compact MMIO telemetry on I/O failure or FAT geometry on success.
""",
)

replace_once(
    "docs/RPI4_EMMC2.md",
    """`bash tests/run_rpi_mailbox_test.sh` builds the production mailbox code against
""",
    """`bash tests/run_mbr_fat32_test.sh` verifies MBR signature handling, supported
primary FAT32 type bytes, boot flags, non-zero ranges, and 32-bit overflow
rejection. `bash tests/run_block_view_fat32_test.sh` mounts a synthetic FAT32
volume at a non-zero physical LBA and proves that root-directory reads stay
relative to the bounded view.

`bash tests/run_rpi_mailbox_test.sh` builds the production mailbox code against
""",
)

replace_once(
    "docs/RPI4_EMMC2.md",
    """4. parse an MBR or superfloppy FAT boot sector read-only;
5. expose `BOARD_CAP_STORAGE` only after repeatable physical reads;
6. add four-bit mode and CMD18/CMD12 after single-block reads are stable;
7. implement writes only after read-only FAT mounting and disposable-card tests
""",
    """4. validate the reported superfloppy or MBR FAT geometry on physical media;
5. expose `BOARD_CAP_STORAGE` only after repeatable physical reads;
6. add four-bit mode and CMD18/CMD12 after single-block reads are stable;
7. implement writes only after read-only FAT mounting and disposable-card tests
""",
)

Path("tools/apply_rpi4_fat_probe_patch.py").unlink()
Path(".github/workflows/apply-rpi4-fat-probe-patch.yml").unlink()
