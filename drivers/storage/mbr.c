#include "storage/mbr.h"

#include <stdint.h>

#define MBR_PARTITION_TABLE_OFFSET 446U
#define MBR_PARTITION_ENTRY_SIZE   16U
#define MBR_PARTITION_COUNT        4U

static uint32_t mbr_le32(const uint8_t *p) {
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8U) |
           ((uint32_t)p[2] << 16U) |
           ((uint32_t)p[3] << 24U);
}

static int mbr_type_is_fat32(uint8_t type) {
    return type == 0x0bU || type == 0x0cU ||
           type == 0x1bU || type == 0x1cU;
}

int mbr_find_fat32_partition(const uint8_t sector[MBR_SECTOR_SIZE],
                             mbr_partition_t *partition) {
    if (sector == 0 || partition == 0 ||
        sector[510] != 0x55U || sector[511] != 0xaaU) {
        return -1;
    }

    for (uint32_t index = 0; index < MBR_PARTITION_COUNT; index++) {
        const uint8_t *entry = &sector[MBR_PARTITION_TABLE_OFFSET +
                                       index * MBR_PARTITION_ENTRY_SIZE];
        uint8_t boot = entry[0];
        uint8_t type = entry[4];
        uint32_t start_lba;
        uint32_t sector_count;

        if ((boot != 0x00U && boot != 0x80U) ||
            !mbr_type_is_fat32(type)) {
            continue;
        }

        start_lba = mbr_le32(&entry[8]);
        sector_count = mbr_le32(&entry[12]);
        if (start_lba == 0U || sector_count == 0U ||
            start_lba > UINT32_MAX - (sector_count - 1U)) {
            continue;
        }

        partition->start_lba = start_lba;
        partition->sector_count = sector_count;
        partition->type = type;
        return 0;
    }

    return -1;
}
