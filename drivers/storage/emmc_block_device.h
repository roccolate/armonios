#ifndef ARMONIOS_DRIVERS_STORAGE_EMMC_BLOCK_DEVICE_H
#define ARMONIOS_DRIVERS_STORAGE_EMMC_BLOCK_DEVICE_H

#include <stdint.h>

#include "storage/block_device.h"
#include "storage/emmc.h"

static inline void emmc_block_device_normalize_csd(
    const uint32_t raw_response[4], uint32_t normalized[4]) {
    normalized[0] = (raw_response[3] << 8U) | (raw_response[2] >> 24U);
    normalized[1] = (raw_response[2] << 8U) | (raw_response[1] >> 24U);
    normalized[2] = (raw_response[1] << 8U) | (raw_response[0] >> 24U);
    normalized[3] = raw_response[0] << 8U;
}

static inline uint32_t emmc_block_device_csd_bits(
    const uint32_t response[4], uint32_t start, uint32_t size) {
    uint32_t offset = 3U - start / 32U;
    uint32_t shift = start & 31U;
    uint32_t mask = size == 32U ? UINT32_MAX : ((1U << size) - 1U);
    uint32_t result = response[offset] >> shift;

    if (size + shift > 32U) {
        result |= response[offset - 1U] << (32U - shift);
    }
    return result & mask;
}

static inline int emmc_block_device_capacity_from_csd(
    const uint32_t raw_response[4], uint64_t *capacity_sectors) {
    uint32_t response[4];
    uint32_t structure;
    uint64_t sectors;

    if (raw_response == 0 || capacity_sectors == 0) {
        return -1;
    }

    emmc_block_device_normalize_csd(raw_response, response);
    structure = emmc_block_device_csd_bits(response, 126U, 2U);

    if (structure == 0U) {
        uint32_t read_block_len =
            emmc_block_device_csd_bits(response, 80U, 4U);
        uint32_t c_size = emmc_block_device_csd_bits(response, 62U, 12U);
        uint32_t c_size_mult =
            emmc_block_device_csd_bits(response, 47U, 3U);
        uint64_t block_bytes;
        uint64_t block_count;
        uint64_t byte_count;

        if (read_block_len >= 32U) {
            return -1;
        }
        block_bytes = 1ULL << read_block_len;
        block_count = ((uint64_t)c_size + 1ULL) << (c_size_mult + 2U);
        if (block_count > UINT64_MAX / block_bytes) {
            return -1;
        }
        byte_count = block_count * block_bytes;
        if (byte_count == 0U || (byte_count & (EMMC_BLKSZ - 1U)) != 0U) {
            return -1;
        }
        sectors = byte_count / EMMC_BLKSZ;
    } else if (structure == 1U || structure == 2U) {
        uint32_t c_size_bits = structure == 1U ? 22U : 28U;
        uint64_t c_size =
            emmc_block_device_csd_bits(response, 48U, c_size_bits);

        sectors = (c_size + 1ULL) << 10U;
    } else {
        return -1;
    }

    if (sectors == 0U) {
        return -1;
    }
    *capacity_sectors = sectors;
    return 0;
}

static inline int emmc_block_device_read(void *context,
                                         uint64_t first_block,
                                         uint32_t block_count,
                                         void *buffer) {
    emmc_device_t *device = (emmc_device_t *)context;

    if (device == 0 || device->ready == 0U || buffer == 0 ||
        block_count == 0U || first_block > UINT32_MAX ||
        first_block + (uint64_t)block_count - 1ULL > UINT32_MAX) {
        return -1;
    }

    return emmc_read_sector(device, (uint32_t)first_block, block_count, buffer);
}

static inline int emmc_block_device_init(block_device_t *out,
                                         emmc_device_t *device) {
    uint64_t capacity_sectors;

    if (out == 0 || device == 0 || device->ready == 0U ||
        emmc_block_device_capacity_from_csd(device->csd,
                                            &capacity_sectors) != 0) {
        return -1;
    }

    out->read = emmc_block_device_read;
    out->write = 0;
    out->flush = 0;
    out->context = device;
    out->block_count = capacity_sectors;
    out->block_size = EMMC_BLKSZ;
    out->flags = BLOCK_DEVICE_FLAG_READ_ONLY;
    return 0;
}

#endif
