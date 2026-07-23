#!/usr/bin/env python3
"""Apply the one-time block-device migration to kernel/kernel.c.

The script is intentionally bounded by stable function/state anchors. It refuses
partial or repeated application and changes no code outside the storage
orchestration region.
"""

from pathlib import Path

PATH = Path("kernel/kernel.c")
text = PATH.read_text()

if "static block_device_view_t g_fat32_partition_view;" in text:
    raise SystemExit("storage migration already applied")

include_anchor = '#include "kernel/vfs.h"\n'
if text.count(include_anchor) != 1:
    raise SystemExit("kernel/vfs.h include anchor is not unique")
text = text.replace(
    include_anchor,
    include_anchor + '#include "storage/mbr.h"\n',
    1,
)

state_anchor = "static fat32_fs_t g_fat32_fs;\n"
if text.count(state_anchor) != 1:
    raise SystemExit("FAT32 state anchor is not unique")
text = text.replace(
    state_anchor,
    state_anchor + "static block_device_view_t g_fat32_partition_view;\n",
    1,
)

start_marker = "static int fat32_read_storage("
end_marker = "static uint8_t g_display_ready;"
if text.count(start_marker) != 1 or text.count(end_marker) != 1:
    raise SystemExit("storage orchestration markers are not unique")
start = text.index(start_marker)
end = text.index(end_marker, start)

replacement = r'''static int probe_fat32(const block_device_t *storage) {
    mbr_partition_t partition;
    const block_device_t *fat_device = storage;
    vfs_stat_t stat;

    fat32_vfs_reset();
    if (fat32_mount_device(&g_fat32_fs, fat_device) != 0) {
        if (mbr_open_fat32_partition(storage, &g_fat32_partition_view,
                                     &partition) != 0) {
            uart_puts("FAT32: absent\n");
            return -1;
        }
        fat_device = block_device_view_device(&g_fat32_partition_view);
        if (fat32_mount_device(&g_fat32_fs, fat_device) != 0) {
            uart_puts("FAT32: absent\n");
            return -1;
        }
        uart_puts("FAT32 partition: ");
        print_dec64(partition.start_lba);
        uart_puts("\n");
    }

    uart_puts("FAT32: mounted\n");
    if (fat32_mount_vfs_root(&g_fat32_fs, "/fat") == 0) {
        uart_puts("FAT32 root: mounted\n");
    } else {
        uart_puts("FAT32 root: no\n");
    }

    if (fat32_mount_vfs_file(&g_fat32_fs, "/fat/shell.bin",
                             "SHELL.BIN") != 0 ||
        vfs_stat("/fat/shell.bin", &stat) != 0) {
        uart_puts("FAT32 shell: no\n");
        return -1;
    }

    uart_puts("FAT32 shell bytes: ");
    print_hex64(stat.size);
    uart_puts("\n");

    if (fat32_mount_vfs_file(&g_fat32_fs, "/fat/edit.txt",
                             "EDIT.TXT") == 0) {
        uart_puts("FAT32 edit file: mounted\n");
    } else {
        uart_puts("FAT32 edit: no\n");
    }
    return 0;
}

/*
 * Storage is optional at this stage. A valid FAT32 image lets apps load from
 * /fat, but bootfs remains the fallback when the block device or filesystem is
 * absent.
 */
static init_status_t probe_storage(void) {
    const block_device_t *storage;
    uint8_t sector[FAT32_SECTOR_SIZE] KERNEL_ALIGNED(8);
    int read_status;

    if (board_storage_init() != 0 ||
        (storage = board_storage_device()) == 0 ||
        storage->block_size != FAT32_SECTOR_SIZE) {
        uart_puts("storage: init failed\n");
        return INIT_STATUS_WARN;
    }

    uart_puts("storage: initialized\n");
    uart_puts("storage sectors: ");
    print_dec64(storage->block_count);
    uart_puts("\n");

    read_status = block_device_read(storage, 0U, 1U, sector);
    if (read_status == 0) {
        uint32_t word = (uint32_t)sector[0] |
                        ((uint32_t)sector[1] << 8) |
                        ((uint32_t)sector[2] << 16) |
                        ((uint32_t)sector[3] << 24);

        uart_puts("storage s0: ");
        print_hex64(word);
        uart_puts("\n");
        return probe_fat32(storage) == 0
                   ? INIT_STATUS_OK
                   : INIT_STATUS_WARN;
    }

    uart_puts("storage err: ");
    print_hex64((uint64_t)(uint32_t)read_status);
    uart_puts("\n");
    return INIT_STATUS_WARN;
}

'''

text = text[:start] + replacement + text[end:]
PATH.write_text(text)
