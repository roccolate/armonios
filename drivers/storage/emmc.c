#include "storage/emmc.h"

#include <stddef.h>
#include <stdint.h>

#define SDHCI_BLOCK_SIZE_COUNT       0x04U
#define SDHCI_ARGUMENT               0x08U
#define SDHCI_TRANSFER_COMMAND       0x0cU
#define SDHCI_RESPONSE0              0x10U
#define SDHCI_BUFFER                 0x20U
#define SDHCI_PRESENT_STATE          0x24U
#define SDHCI_HOST_POWER             0x28U
#define SDHCI_CLOCK_RESET            0x2cU
#define SDHCI_INT_STATUS             0x30U
#define SDHCI_INT_ENABLE             0x34U
#define SDHCI_SIGNAL_ENABLE          0x38U

#define SDHCI_CMD_INHIBIT            0x00000001U
#define SDHCI_DATA_INHIBIT           0x00000002U
#define SDHCI_CARD_PRESENT           0x00010000U
#define SDHCI_CARD_STABLE            0x00020000U

#define SDHCI_POWER_ON               0x01U
#define SDHCI_POWER_330              0x0eU

#define SDHCI_CLOCK_INT_EN           0x0001U
#define SDHCI_CLOCK_INT_STABLE       0x0002U
#define SDHCI_CLOCK_CARD_EN          0x0004U
#define SDHCI_DIVIDER_SHIFT          8U
#define SDHCI_DIVIDER_HI_SHIFT       6U
#define SDHCI_DIV_MASK               0x00ffU
#define SDHCI_DIV_HI_MASK            0x0300U

#define SDHCI_RESET_ALL              0x01U
#define SDHCI_RESET_CMD              0x02U
#define SDHCI_RESET_DATA             0x04U

#define SDHCI_INT_RESPONSE           0x00000001U
#define SDHCI_INT_DATA_END           0x00000002U
#define SDHCI_INT_DATA_AVAIL         0x00000020U
#define SDHCI_INT_TIMEOUT            0x00010000U
#define SDHCI_INT_ERROR_MASK         0xffff8000U
#define SDHCI_INT_ALL                0xffffffffU
#define SDHCI_INT_POLL_MASK          (SDHCI_INT_RESPONSE | SDHCI_INT_DATA_END | \
                                      SDHCI_INT_DATA_AVAIL | \
                                      SDHCI_INT_ERROR_MASK)

#define SDHCI_TRNS_BLK_CNT_EN        0x0002U
#define SDHCI_TRNS_READ              0x0010U

#define SDHCI_CMD_RESP_NONE          0x00U
#define SDHCI_CMD_RESP_LONG          0x01U
#define SDHCI_CMD_RESP_SHORT         0x02U
#define SDHCI_CMD_RESP_SHORT_BUSY    0x03U
#define SDHCI_CMD_CRC                0x08U
#define SDHCI_CMD_INDEX              0x10U
#define SDHCI_CMD_DATA               0x20U

#define SD_CMD_GO_IDLE               0U
#define SD_CMD_ALL_SEND_CID          2U
#define SD_CMD_SEND_RELATIVE_ADDR    3U
#define SD_CMD_SELECT_CARD           7U
#define SD_CMD_SEND_IF_COND          8U
#define SD_CMD_SEND_CSD              9U
#define SD_CMD_SET_BLOCKLEN          16U
#define SD_CMD_READ_SINGLE           17U
#define SD_CMD_APP_CMD               55U
#define SD_ACMD_SEND_OP_COND         41U

#define SD_IF_COND_ARG               0x000001aaU
#define SD_IF_COND_MASK              0x00000fffU
#define SD_OCR_VOLTAGE_WINDOW        0x00ff8000U
#define SD_OCR_HIGH_CAPACITY         (1U << 30)
#define SD_OCR_READY                 (1U << 31)

#define EMMC_RESET_TIMEOUT_US        100000U
#define EMMC_CLOCK_TIMEOUT_US        150000U
#define EMMC_COMMAND_TIMEOUT_US      1000000U
#define EMMC_DATA_TIMEOUT_US         1000000U
#define EMMC_CARD_TIMEOUT_US         100000U
#define EMMC_OP_COND_RETRIES         1000U

static uint32_t emmc_mmio_read32(void *context, uint32_t offset) {
    uintptr_t base = (uintptr_t)context;
    return *(volatile uint32_t *)(base + offset);
}

static void emmc_barrier(void) {
#if defined(__aarch64__)
    __asm__ volatile("dmb sy" ::: "memory");
#else
    __sync_synchronize();
#endif
}

static void emmc_mmio_write32(void *context, uint32_t offset, uint32_t value) {
    uintptr_t base = (uintptr_t)context;
    *(volatile uint32_t *)(base + offset) = value;
    emmc_barrier();
}

static void emmc_default_delay_us(void *context, uint32_t usec) {
    (void)context;
#if defined(__aarch64__)
    uint64_t frequency;
    uint64_t start;
    uint64_t now;
    uint64_t ticks;

    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(frequency));
    __asm__ volatile("mrs %0, cntpct_el0" : "=r"(start));
    ticks = (frequency * usec + 999999U) / 1000000U;
    do {
        __asm__ volatile("mrs %0, cntpct_el0" : "=r"(now));
    } while (now - start < ticks);
#else
    for (volatile uint32_t i = 0; i < usec * 16U; i++) {
        __asm__ volatile("" ::: "memory");
    }
#endif
}

static uint32_t emmc_read32(const emmc_device_t *dev, uint32_t offset) {
    return dev->io.read32(dev->io.context, offset);
}

static void emmc_delay(const emmc_device_t *dev, uint32_t usec) {
    dev->io.delay_us(dev->io.context, usec);
}

static void emmc_write32(const emmc_device_t *dev, uint32_t offset,
                         uint32_t value) {
    dev->io.write32(dev->io.context, offset, value);
    if (dev->actual_clock_hz != 0U &&
        dev->actual_clock_hz <= EMMC_IDENT_CLOCK_HZ) {
        uint32_t delay = (4000000U + dev->actual_clock_hz - 1U) /
                         dev->actual_clock_hz;
        emmc_delay(dev, delay);
    }
}

static uint8_t emmc_read8(const emmc_device_t *dev, uint32_t offset) {
    uint32_t shift = (offset & 3U) * 8U;
    return (uint8_t)(emmc_read32(dev, offset & ~3U) >> shift);
}

static void emmc_write8(const emmc_device_t *dev, uint32_t offset,
                        uint8_t value) {
    uint32_t aligned = offset & ~3U;
    uint32_t shift = (offset & 3U) * 8U;
    uint32_t current = emmc_read32(dev, aligned);
    uint32_t mask = 0xffU << shift;
    emmc_write32(dev, aligned, (current & ~mask) | ((uint32_t)value << shift));
}

static void emmc_write16(const emmc_device_t *dev, uint32_t offset,
                         uint16_t value) {
    uint32_t aligned = offset & ~3U;
    uint32_t shift = (offset & 2U) * 8U;
    uint32_t current = emmc_read32(dev, aligned);
    uint32_t mask = 0xffffU << shift;
    emmc_write32(dev, aligned, (current & ~mask) | ((uint32_t)value << shift));
}

static int emmc_wait_bits(const emmc_device_t *dev, uint32_t offset,
                          uint32_t mask, uint32_t expected,
                          uint32_t timeout_us) {
    for (uint32_t elapsed = 0; elapsed < timeout_us; elapsed++) {
        if ((emmc_read32(dev, offset) & mask) == expected) {
            return EMMC_OK;
        }
        emmc_delay(dev, 1U);
    }
    return EMMC_ERR_TIMEOUT;
}

static int emmc_reset_lines(const emmc_device_t *dev, uint8_t reset_mask) {
    uint8_t reset = emmc_read8(dev, SDHCI_CLOCK_RESET + 3U);
    emmc_write8(dev, SDHCI_CLOCK_RESET + 3U,
                (uint8_t)(reset | reset_mask));

    for (uint32_t elapsed = 0; elapsed < EMMC_RESET_TIMEOUT_US; elapsed++) {
        if ((emmc_read8(dev, SDHCI_CLOCK_RESET + 3U) & reset_mask) == 0U) {
            return EMMC_OK;
        }
        emmc_delay(dev, 1U);
    }
    return EMMC_ERR_TIMEOUT;
}

static uint16_t emmc_clock_value(uint32_t base_clock_hz,
                                 uint32_t target_clock_hz,
                                 uint32_t *actual_clock_hz) {
    uint32_t real_divisor;
    uint32_t encoded_divisor;
    uint16_t value;

    if (base_clock_hz == 0U || target_clock_hz == 0U) {
        if (actual_clock_hz != NULL) {
            *actual_clock_hz = 0U;
        }
        return 0U;
    }

    if (base_clock_hz <= target_clock_hz) {
        real_divisor = 1U;
    } else {
        real_divisor = 2U;
        while (real_divisor < 2046U &&
               base_clock_hz / real_divisor > target_clock_hz) {
            real_divisor += 2U;
        }
    }

    encoded_divisor = real_divisor >> 1U;
    value = (uint16_t)((encoded_divisor & SDHCI_DIV_MASK)
                       << SDHCI_DIVIDER_SHIFT);
    value |= (uint16_t)(((encoded_divisor & SDHCI_DIV_HI_MASK) >> 8U)
                        << SDHCI_DIVIDER_HI_SHIFT);
    if (actual_clock_hz != NULL) {
        *actual_clock_hz = base_clock_hz / real_divisor;
    }
    return value;
}

static int emmc_set_clock(emmc_device_t *dev, uint32_t target_clock_hz) {
    uint32_t actual_clock_hz;
    uint16_t clock;

    emmc_write16(dev, SDHCI_CLOCK_RESET, 0U);
    dev->actual_clock_hz = 0U;
    if (target_clock_hz == 0U) {
        return EMMC_OK;
    }

    clock = emmc_clock_value(dev->base_clock_hz, target_clock_hz,
                             &actual_clock_hz);
    if (clock == 0U && actual_clock_hz == 0U) {
        return EMMC_ERR_INVAL;
    }

    emmc_write16(dev, SDHCI_CLOCK_RESET,
                 (uint16_t)(clock | SDHCI_CLOCK_INT_EN));
    if (emmc_wait_bits(dev, SDHCI_CLOCK_RESET,
                       SDHCI_CLOCK_INT_STABLE,
                       SDHCI_CLOCK_INT_STABLE,
                       EMMC_CLOCK_TIMEOUT_US) != EMMC_OK) {
        return EMMC_ERR_TIMEOUT;
    }

    emmc_write16(dev, SDHCI_CLOCK_RESET,
                 (uint16_t)(clock | SDHCI_CLOCK_INT_EN |
                            SDHCI_CLOCK_CARD_EN));
    dev->actual_clock_hz = actual_clock_hz;
    return EMMC_OK;
}

static uint16_t emmc_make_command(uint32_t index, uint8_t flags) {
    return (uint16_t)(((index & 0x3fU) << 8U) | flags);
}

static int emmc_wait_interrupt(const emmc_device_t *dev, uint32_t wanted,
                               uint32_t timeout_us, uint32_t *status_out) {
    if (status_out != NULL) {
        *status_out = 0U;
    }
    for (uint32_t elapsed = 0; elapsed < timeout_us; elapsed++) {
        uint32_t status = emmc_read32(dev, SDHCI_INT_STATUS);
        if ((status & SDHCI_INT_ERROR_MASK) != 0U) {
            if (status_out != NULL) {
                *status_out = status;
            }
            return (status & SDHCI_INT_TIMEOUT) != 0U
                       ? EMMC_ERR_TIMEOUT
                       : EMMC_ERR_COMMAND;
        }
        if ((status & wanted) != 0U) {
            if (status_out != NULL) {
                *status_out = status;
            }
            return EMMC_OK;
        }
        emmc_delay(dev, 1U);
    }
    return EMMC_ERR_TIMEOUT;
}

static int emmc_send_command(emmc_device_t *dev, uint32_t index,
                             uint32_t argument, uint8_t flags,
                             uint16_t transfer_mode,
                             uint32_t response[4]) {
    uint32_t inhibit = SDHCI_CMD_INHIBIT;
    uint32_t status;
    int result;

    if ((flags & SDHCI_CMD_DATA) != 0U) {
        inhibit |= SDHCI_DATA_INHIBIT;
    }
    if (emmc_wait_bits(dev, SDHCI_PRESENT_STATE, inhibit, 0U,
                       EMMC_COMMAND_TIMEOUT_US) != EMMC_OK) {
        return EMMC_ERR_TIMEOUT;
    }

    emmc_write32(dev, SDHCI_INT_STATUS, SDHCI_INT_ALL);
    emmc_write32(dev, SDHCI_ARGUMENT, argument);
    emmc_write32(dev, SDHCI_TRANSFER_COMMAND,
                 (uint32_t)transfer_mode |
                 ((uint32_t)emmc_make_command(index, flags) << 16U));

    result = emmc_wait_interrupt(dev, SDHCI_INT_RESPONSE,
                                 EMMC_COMMAND_TIMEOUT_US, &status);
    if (result != EMMC_OK) {
        (void)emmc_reset_lines(dev, SDHCI_RESET_CMD);
        emmc_write32(dev, SDHCI_INT_STATUS, status & SDHCI_INT_ERROR_MASK);
        return result;
    }

    emmc_write32(dev, SDHCI_INT_STATUS, SDHCI_INT_RESPONSE);
    if (response != NULL) {
        response[0] = emmc_read32(dev, SDHCI_RESPONSE0 + 0U);
        response[1] = emmc_read32(dev, SDHCI_RESPONSE0 + 4U);
        response[2] = emmc_read32(dev, SDHCI_RESPONSE0 + 8U);
        response[3] = emmc_read32(dev, SDHCI_RESPONSE0 + 12U);
    }

    if ((flags & 0x03U) == SDHCI_CMD_RESP_SHORT_BUSY &&
        emmc_wait_bits(dev, SDHCI_PRESENT_STATE, SDHCI_DATA_INHIBIT, 0U,
                       EMMC_DATA_TIMEOUT_US) != EMMC_OK) {
        (void)emmc_reset_lines(dev, SDHCI_RESET_DATA);
        return EMMC_ERR_TIMEOUT;
    }

    return EMMC_OK;
}

static int emmc_send_app_command(emmc_device_t *dev, uint32_t rca,
                                 uint32_t app_index, uint32_t argument,
                                 uint8_t app_flags, uint32_t response[4]) {
    uint32_t app_response[4];
    int result;

    result = emmc_send_command(dev, SD_CMD_APP_CMD, rca << 16U,
                               SDHCI_CMD_RESP_SHORT | SDHCI_CMD_CRC |
                                   SDHCI_CMD_INDEX,
                               0U, app_response);
    if (result != EMMC_OK) {
        return result;
    }

    return emmc_send_command(dev, app_index, argument, app_flags, 0U,
                             response);
}

static void emmc_clear_device(emmc_device_t *dev) {
    dev->base = 0U;
    dev->base_clock_hz = 0U;
    dev->actual_clock_hz = 0U;
    dev->rca = 0U;
    dev->ocr = 0U;
    for (uint32_t i = 0; i < 4U; i++) {
        dev->cid[i] = 0U;
        dev->csd[i] = 0U;
    }
    dev->ready = 0U;
    dev->high_capacity = 0U;
    dev->read_only = 1U;
}

int emmc_init_with_io(emmc_device_t *dev, const emmc_io_t *io,
                      uint32_t base_clock_hz) {
    uint32_t response[4];
    uint32_t op_cond_arg = SD_OCR_VOLTAGE_WINDOW;
    int result;
    int v2_card = 1;

    if (dev == NULL || io == NULL || io->read32 == NULL ||
        io->write32 == NULL || io->delay_us == NULL ||
        base_clock_hz == 0U) {
        return EMMC_ERR_INVAL;
    }

    emmc_clear_device(dev);
    dev->io = *io;
    dev->base_clock_hz = base_clock_hz;

    result = emmc_reset_lines(dev, SDHCI_RESET_ALL);
    if (result != EMMC_OK) {
        return result;
    }

    emmc_write8(dev, SDHCI_HOST_POWER + 1U, 0U);
    emmc_delay(dev, 1000U);
    emmc_write8(dev, SDHCI_HOST_POWER + 1U,
                SDHCI_POWER_330 | SDHCI_POWER_ON);
    emmc_delay(dev, 10000U);

    emmc_write8(dev, SDHCI_CLOCK_RESET + 2U, 0x0eU);
    emmc_write32(dev, SDHCI_INT_ENABLE, SDHCI_INT_POLL_MASK);
    emmc_write32(dev, SDHCI_SIGNAL_ENABLE, 0U);
    emmc_write32(dev, SDHCI_INT_STATUS, SDHCI_INT_ALL);

    result = emmc_set_clock(dev, EMMC_IDENT_CLOCK_HZ);
    if (result != EMMC_OK) {
        return result;
    }

    if (emmc_wait_bits(dev, SDHCI_PRESENT_STATE,
                       SDHCI_CARD_PRESENT | SDHCI_CARD_STABLE,
                       SDHCI_CARD_PRESENT | SDHCI_CARD_STABLE,
                       EMMC_CARD_TIMEOUT_US) != EMMC_OK) {
        return EMMC_ERR_TIMEOUT;
    }

    result = emmc_send_command(dev, SD_CMD_GO_IDLE, 0U,
                               SDHCI_CMD_RESP_NONE, 0U, NULL);
    if (result != EMMC_OK) {
        return result;
    }
    emmc_delay(dev, 2000U);

    result = emmc_send_command(dev, SD_CMD_SEND_IF_COND, SD_IF_COND_ARG,
                               SDHCI_CMD_RESP_SHORT | SDHCI_CMD_CRC |
                                   SDHCI_CMD_INDEX,
                               0U, response);
    if (result == EMMC_ERR_TIMEOUT) {
        v2_card = 0;
    } else if (result != EMMC_OK ||
               (response[0] & SD_IF_COND_MASK) != SD_IF_COND_ARG) {
        return EMMC_ERR_UNSUPPORTED;
    }

    if (v2_card != 0) {
        op_cond_arg |= SD_OCR_HIGH_CAPACITY;
    }

    for (uint32_t attempt = 0; attempt < EMMC_OP_COND_RETRIES; attempt++) {
        result = emmc_send_app_command(dev, 0U, SD_ACMD_SEND_OP_COND,
                                       op_cond_arg,
                                       SDHCI_CMD_RESP_SHORT, response);
        if (result != EMMC_OK) {
            return result;
        }
        dev->ocr = response[0];
        if ((dev->ocr & SD_OCR_READY) != 0U) {
            break;
        }
        emmc_delay(dev, 1000U);
    }
    if ((dev->ocr & SD_OCR_READY) == 0U) {
        return EMMC_ERR_TIMEOUT;
    }
    dev->high_capacity = (dev->ocr & SD_OCR_HIGH_CAPACITY) != 0U;

    result = emmc_send_command(dev, SD_CMD_ALL_SEND_CID, 0U,
                               SDHCI_CMD_RESP_LONG | SDHCI_CMD_CRC,
                               0U, dev->cid);
    if (result != EMMC_OK) {
        return result;
    }

    result = emmc_send_command(dev, SD_CMD_SEND_RELATIVE_ADDR, 0U,
                               SDHCI_CMD_RESP_SHORT | SDHCI_CMD_CRC |
                                   SDHCI_CMD_INDEX,
                               0U, response);
    if (result != EMMC_OK) {
        return result;
    }
    dev->rca = response[0] >> 16U;
    if (dev->rca == 0U) {
        return EMMC_ERR_COMMAND;
    }

    result = emmc_send_command(dev, SD_CMD_SEND_CSD, dev->rca << 16U,
                               SDHCI_CMD_RESP_LONG | SDHCI_CMD_CRC,
                               0U, dev->csd);
    if (result != EMMC_OK) {
        return result;
    }

    result = emmc_send_command(dev, SD_CMD_SELECT_CARD, dev->rca << 16U,
                               SDHCI_CMD_RESP_SHORT_BUSY | SDHCI_CMD_CRC |
                                   SDHCI_CMD_INDEX,
                               0U, response);
    if (result != EMMC_OK) {
        return result;
    }

    if (dev->high_capacity == 0U) {
        result = emmc_send_command(dev, SD_CMD_SET_BLOCKLEN, EMMC_BLKSZ,
                                   SDHCI_CMD_RESP_SHORT | SDHCI_CMD_CRC |
                                       SDHCI_CMD_INDEX,
                                   0U, response);
        if (result != EMMC_OK) {
            return result;
        }
    }

    result = emmc_set_clock(dev, EMMC_TRANSFER_CLOCK_HZ);
    if (result != EMMC_OK) {
        return result;
    }

    dev->ready = 1U;
    return EMMC_OK;
}

int emmc_init(emmc_device_t *dev, uint64_t base, uint32_t base_clock_hz) {
    emmc_io_t io;
    int result;

    if (dev == NULL || base == 0U) {
        return EMMC_ERR_INVAL;
    }

    io.read32 = emmc_mmio_read32;
    io.write32 = emmc_mmio_write32;
    io.delay_us = emmc_default_delay_us;
    io.context = (void *)(uintptr_t)base;
    result = emmc_init_with_io(dev, &io, base_clock_hz);
    if (result == EMMC_OK) {
        dev->base = base;
    }
    return result;
}

static int emmc_read_one(emmc_device_t *dev, uint32_t lba,
                         uint8_t *buffer) {
    uint32_t response[4];
    uint32_t argument;
    uint32_t status;
    int result;

    if (dev->high_capacity != 0U) {
        argument = lba;
    } else {
        if (lba > UINT32_MAX / EMMC_BLKSZ) {
            return EMMC_ERR_INVAL;
        }
        argument = lba * EMMC_BLKSZ;
    }

    emmc_write32(dev, SDHCI_BLOCK_SIZE_COUNT,
                 (1U << 16U) | EMMC_BLKSZ);
    result = emmc_send_command(dev, SD_CMD_READ_SINGLE, argument,
                               SDHCI_CMD_RESP_SHORT | SDHCI_CMD_CRC |
                                   SDHCI_CMD_INDEX | SDHCI_CMD_DATA,
                               SDHCI_TRNS_BLK_CNT_EN | SDHCI_TRNS_READ,
                               response);
    if (result != EMMC_OK) {
        return result;
    }

    result = emmc_wait_interrupt(dev, SDHCI_INT_DATA_AVAIL,
                                 EMMC_DATA_TIMEOUT_US, &status);
    if (result != EMMC_OK) {
        (void)emmc_reset_lines(dev, SDHCI_RESET_DATA);
        emmc_write32(dev, SDHCI_INT_STATUS, status & SDHCI_INT_ERROR_MASK);
        return EMMC_ERR_DATA;
    }

    for (uint32_t word_index = 0; word_index < EMMC_BLKSZ / 4U;
         word_index++) {
        uint32_t word = emmc_read32(dev, SDHCI_BUFFER);
        buffer[word_index * 4U + 0U] = (uint8_t)word;
        buffer[word_index * 4U + 1U] = (uint8_t)(word >> 8U);
        buffer[word_index * 4U + 2U] = (uint8_t)(word >> 16U);
        buffer[word_index * 4U + 3U] = (uint8_t)(word >> 24U);
    }
    emmc_write32(dev, SDHCI_INT_STATUS, SDHCI_INT_DATA_AVAIL);

    result = emmc_wait_interrupt(dev, SDHCI_INT_DATA_END,
                                 EMMC_DATA_TIMEOUT_US, &status);
    if (result != EMMC_OK) {
        (void)emmc_reset_lines(dev, SDHCI_RESET_DATA);
        emmc_write32(dev, SDHCI_INT_STATUS, status & SDHCI_INT_ERROR_MASK);
        return EMMC_ERR_DATA;
    }
    emmc_write32(dev, SDHCI_INT_STATUS, SDHCI_INT_DATA_END);
    return EMMC_OK;
}

int emmc_read_sector(emmc_device_t *dev, uint32_t lba, uint32_t count,
                     void *buffer) {
    uint8_t *bytes = (uint8_t *)buffer;

    if (dev == NULL || dev->ready == 0U || buffer == NULL || count == 0U ||
        count > UINT32_MAX / EMMC_BLKSZ ||
        lba > UINT32_MAX - (count - 1U)) {
        return EMMC_ERR_INVAL;
    }

    for (uint32_t sector = 0; sector < count; sector++) {
        int result = emmc_read_one(dev, lba + sector,
                                   bytes + sector * EMMC_BLKSZ);
        if (result != EMMC_OK) {
            return result;
        }
    }
    return EMMC_OK;
}

int emmc_write_sector(emmc_device_t *dev, uint32_t lba, uint32_t count,
                      const void *buffer) {
    (void)lba;
    (void)count;
    (void)buffer;

    if (dev == NULL || dev->ready == 0U) {
        return EMMC_ERR_INVAL;
    }
    return EMMC_ERR_READ_ONLY;
}
