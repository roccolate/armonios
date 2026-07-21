#ifndef ARMONIOS_DRIVERS_STORAGE_MBR_H
#define ARMONIOS_DRIVERS_STORAGE_MBR_H

#include <stdint.h>

#define MBR_SECTOR_SIZE 512U

typedef struct {
    uint32_t start_lba;
    uint32_t sector_count;
    uint8_t type;
} mbr_partition_t;

/*
 * Find the first primary FAT32 partition in a conventional MBR sector.
 *
 * Supported type bytes are FAT32 CHS/LBA and their hidden variants:
 * 0x0b, 0x0c, 0x1b, and 0x1c. GPT, extended partitions, and FAT12/16 are
 * deliberately outside this small contract.
 */
int mbr_find_fat32_partition(const uint8_t sector[MBR_SECTOR_SIZE],
                             mbr_partition_t *partition);

#endif
