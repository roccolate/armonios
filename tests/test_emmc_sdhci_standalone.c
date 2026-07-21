#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "storage/emmc.h"

#define REG_BLOCK                0x04U
#define REG_ARGUMENT             0x08U
#define REG_TRANSFER_COMMAND     0x0cU
#define REG_RESPONSE0            0x10U
#define REG_BUFFER               0x20U
#define REG_PRESENT_STATE        0x24U
#define REG_CLOCK_RESET          0x2cU
#define REG_INT_STATUS           0x30U
#define REG_INT_ENABLE           0x34U
#define REG_SIGNAL_ENABLE        0x38U

#define PRESENT_CARD             0x00010000U
#define PRESENT_STABLE           0x00020000U
#define CLOCK_STABLE             0x0002U
#define RESET_ALL                0x01000000U
#define RESET_CMD                0x02000000U
#define RESET_DATA               0x04000000U
#define INT_RESPONSE             0x00000001U
#define INT_DATA_END             0x00000002U
#define INT_DATA_AVAIL           0x00000020U
#define INT_ERROR_MASK           0xffff8000U
#define INT_POLL_MASK            (INT_RESPONSE | INT_DATA_END | \
                                  INT_DATA_AVAIL | INT_ERROR_MASK)

#define CMD_GO_IDLE              0U
#define CMD_ALL_SEND_CID         2U
#define CMD_SEND_REL_ADDR        3U
#define CMD_SELECT               7U
#define CMD_IF_COND              8U
#define CMD_SEND_CSD             9U
#define CMD_SET_BLOCKLEN         16U
#define CMD_READ_SINGLE          17U
#define CMD_APP                  55U
#define ACMD_OP_COND             41U

#define OCR_READY                (1U << 31)
#define OCR_CCS                  (1U << 30)

#define ASSERT_TRUE(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "assert failed: %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

#define ASSERT_EQ_U32(expected, actual) do { \
    uint32_t e_ = (uint32_t)(expected); \
    uint32_t a_ = (uint32_t)(actual); \
    if (e_ != a_) { \
        fprintf(stderr, "assert failed: %s:%d: expected 0x%08x got 0x%08x\n", \
                __FILE__, __LINE__, e_, a_); \
        return 1; \
    } \
} while (0)

typedef struct {
    uint32_t regs[64];
    uint32_t command_log[32];
    uint32_t argument_log[32];
    uint32_t command_count;
    uint32_t data_word;
    uint8_t force_if_cond_timeout;
} fake_sdhci_t;

static uint32_t fake_read32(void *context, uint32_t offset) {
    fake_sdhci_t *fake = (fake_sdhci_t *)context;
    if (offset == REG_BUFFER) {
        uint32_t first = fake->data_word * 4U;
        uint32_t value = (uint8_t)first |
                         ((uint32_t)(uint8_t)(first + 1U) << 8U) |
                         ((uint32_t)(uint8_t)(first + 2U) << 16U) |
                         ((uint32_t)(uint8_t)(first + 3U) << 24U);
        fake->data_word++;
        return value;
    }
    return fake->regs[offset / 4U];
}

static void fake_issue_command(fake_sdhci_t *fake, uint32_t value) {
    uint32_t command = (value >> 24U) & 0x3fU;
    uint32_t argument = fake->regs[REG_ARGUMENT / 4U];
    uint32_t slot = fake->command_count++;

    fake->command_log[slot] = command;
    fake->argument_log[slot] = argument;
    fake->regs[REG_RESPONSE0 / 4U] = 0U;

    switch (command) {
    case CMD_GO_IDLE:
        break;
    case CMD_IF_COND:
        if (fake->force_if_cond_timeout != 0U) {
            fake->regs[REG_INT_STATUS / 4U] |= 0x00010000U;
            return;
        }
        fake->regs[REG_RESPONSE0 / 4U] = 0x000001aaU;
        break;
    case CMD_APP:
        fake->regs[REG_RESPONSE0 / 4U] = 0x20U;
        break;
    case ACMD_OP_COND:
        fake->regs[REG_RESPONSE0 / 4U] = OCR_READY | 0x00ff8000U;
        if (fake->force_if_cond_timeout == 0U) {
            fake->regs[REG_RESPONSE0 / 4U] |= OCR_CCS;
        }
        break;
    case CMD_ALL_SEND_CID:
        fake->regs[REG_RESPONSE0 / 4U + 0U] = 0x11111111U;
        fake->regs[REG_RESPONSE0 / 4U + 1U] = 0x22222222U;
        fake->regs[REG_RESPONSE0 / 4U + 2U] = 0x33333333U;
        fake->regs[REG_RESPONSE0 / 4U + 3U] = 0x44444444U;
        break;
    case CMD_SEND_REL_ADDR:
        fake->regs[REG_RESPONSE0 / 4U] = 0x12340000U;
        break;
    case CMD_SEND_CSD:
        fake->regs[REG_RESPONSE0 / 4U + 0U] = 0xaaaa0001U;
        fake->regs[REG_RESPONSE0 / 4U + 1U] = 0xbbbb0002U;
        fake->regs[REG_RESPONSE0 / 4U + 2U] = 0xcccc0003U;
        fake->regs[REG_RESPONSE0 / 4U + 3U] = 0xdddd0004U;
        break;
    case CMD_SELECT:
    case CMD_SET_BLOCKLEN:
        break;
    case CMD_READ_SINGLE:
        fake->data_word = 0U;
        fake->regs[REG_INT_STATUS / 4U] |= INT_DATA_AVAIL | INT_DATA_END;
        break;
    default:
        break;
    }
    fake->regs[REG_INT_STATUS / 4U] |= INT_RESPONSE;
}

static void fake_write32(void *context, uint32_t offset, uint32_t value) {
    fake_sdhci_t *fake = (fake_sdhci_t *)context;

    if (offset == REG_INT_STATUS) {
        fake->regs[offset / 4U] &= ~value;
        return;
    }
    if (offset == REG_CLOCK_RESET) {
        uint32_t stored = value;
        if ((stored & 0x0000ffffU) != 0U) {
            stored |= CLOCK_STABLE;
        }
        stored &= ~(RESET_ALL | RESET_CMD | RESET_DATA);
        fake->regs[offset / 4U] = stored;
        return;
    }

    fake->regs[offset / 4U] = value;
    if (offset == REG_TRANSFER_COMMAND) {
        fake_issue_command(fake, value);
    }
}

static void fake_delay(void *context, uint32_t usec) {
    (void)context;
    (void)usec;
}

static int test_init_and_read(void) {
    fake_sdhci_t fake;
    emmc_device_t dev;
    emmc_io_t io;
    uint8_t sector[EMMC_BLKSZ];
    uint32_t read_command_index;

    memset(&fake, 0, sizeof(fake));
    memset(&dev, 0, sizeof(dev));
    fake.regs[REG_PRESENT_STATE / 4U] = PRESENT_CARD | PRESENT_STABLE;
    io.read32 = fake_read32;
    io.write32 = fake_write32;
    io.delay_us = fake_delay;
    io.context = &fake;

    ASSERT_EQ_U32(EMMC_OK, emmc_init_with_io(&dev, &io, 100000000U));
    ASSERT_TRUE(dev.ready != 0U);
    ASSERT_TRUE(dev.read_only != 0U);
    ASSERT_TRUE(dev.high_capacity != 0U);
    ASSERT_EQ_U32(0x1234U, dev.rca);
    ASSERT_TRUE(dev.actual_clock_hz <= EMMC_TRANSFER_CLOCK_HZ);
    ASSERT_TRUE(dev.actual_clock_hz > 0U);
    ASSERT_EQ_U32(INT_POLL_MASK, fake.regs[REG_INT_ENABLE / 4U]);
    ASSERT_EQ_U32(0U, fake.regs[REG_SIGNAL_ENABLE / 4U]);

    ASSERT_EQ_U32(CMD_GO_IDLE, fake.command_log[0]);
    ASSERT_EQ_U32(CMD_IF_COND, fake.command_log[1]);
    ASSERT_EQ_U32(CMD_APP, fake.command_log[2]);
    ASSERT_EQ_U32(ACMD_OP_COND, fake.command_log[3]);
    ASSERT_EQ_U32(CMD_ALL_SEND_CID, fake.command_log[4]);
    ASSERT_EQ_U32(CMD_SEND_REL_ADDR, fake.command_log[5]);
    ASSERT_EQ_U32(CMD_SEND_CSD, fake.command_log[6]);
    ASSERT_EQ_U32(CMD_SELECT, fake.command_log[7]);

    ASSERT_EQ_U32(EMMC_OK, emmc_read_sector(&dev, 7U, 1U, sector));
    read_command_index = fake.command_count - 1U;
    ASSERT_EQ_U32(CMD_READ_SINGLE, fake.command_log[read_command_index]);
    ASSERT_EQ_U32(7U, fake.argument_log[read_command_index]);
    for (uint32_t i = 0; i < EMMC_BLKSZ; i++) {
        ASSERT_EQ_U32((uint8_t)i, sector[i]);
    }

    {
        uint8_t two_sectors[EMMC_BLKSZ * 2U];
        uint32_t before = fake.command_count;
        ASSERT_EQ_U32(EMMC_OK,
                      emmc_read_sector(&dev, 20U, 2U, two_sectors));
        ASSERT_EQ_U32(CMD_READ_SINGLE, fake.command_log[before]);
        ASSERT_EQ_U32(20U, fake.argument_log[before]);
        ASSERT_EQ_U32(CMD_READ_SINGLE, fake.command_log[before + 1U]);
        ASSERT_EQ_U32(21U, fake.argument_log[before + 1U]);
        ASSERT_EQ_U32(EMMC_ERR_INVAL,
                      emmc_read_sector(&dev, UINT32_MAX, 2U, two_sectors));
        ASSERT_EQ_U32(before + 2U, fake.command_count);
    }

    ASSERT_EQ_U32(EMMC_ERR_INVAL,
                  emmc_read_sector(&dev, 0U, 0U, sector));
    ASSERT_EQ_U32(EMMC_ERR_READ_ONLY,
                  emmc_write_sector(&dev, 7U, 1U, sector));
    return 0;
}

static int test_old_card_uses_byte_address(void) {
    fake_sdhci_t fake;
    emmc_device_t dev;
    emmc_io_t io;
    uint8_t sector[EMMC_BLKSZ];
    uint32_t read_command_index;

    memset(&fake, 0, sizeof(fake));
    memset(&dev, 0, sizeof(dev));
    fake.force_if_cond_timeout = 1U;
    fake.regs[REG_PRESENT_STATE / 4U] = PRESENT_CARD | PRESENT_STABLE;
    io.read32 = fake_read32;
    io.write32 = fake_write32;
    io.delay_us = fake_delay;
    io.context = &fake;

    ASSERT_EQ_U32(EMMC_OK, emmc_init_with_io(&dev, &io, 100000000U));
    ASSERT_TRUE(dev.high_capacity == 0U);
    ASSERT_EQ_U32(CMD_SET_BLOCKLEN, fake.command_log[8]);
    ASSERT_EQ_U32(EMMC_OK, emmc_read_sector(&dev, 3U, 1U, sector));
    read_command_index = fake.command_count - 1U;
    ASSERT_EQ_U32(3U * EMMC_BLKSZ, fake.argument_log[read_command_index]);
    return 0;
}

int main(void) {
    if (test_init_and_read() != 0) {
        return 1;
    }
    if (test_old_card_uses_byte_address() != 0) {
        return 1;
    }
    puts("emmc-sdhci-test: PASS");
    return 0;
}
