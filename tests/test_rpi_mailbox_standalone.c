#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "firmware/rpi_mailbox.h"

#define REG_READ        0x00U
#define REG_STATUS      0x18U
#define REG_WRITE       0x20U
#define STATUS_FULL     0x80000000U
#define STATUS_EMPTY    0x40000000U
#define CHANNEL_PROPERTY 8U
#define TAG_GET_CLOCK_RATE 0x00030002U
#define TEST_BUS_ALIAS  0xc0000000U

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
    uint32_t *message;
    uint32_t expected_address;
    uint32_t written;
    uint32_t queued_response;
    uint32_t barriers;
    uint8_t full_forever;
    uint8_t empty_forever;
    uint8_t request_valid;
} fake_mailbox_t;

static uint32_t fake_read32(void *context, uint32_t offset) {
    fake_mailbox_t *fake = (fake_mailbox_t *)context;

    if (offset == REG_STATUS) {
        if (fake->full_forever != 0U) {
            return STATUS_FULL | STATUS_EMPTY;
        }
        if (fake->empty_forever != 0U || fake->queued_response == 0U) {
            return STATUS_EMPTY;
        }
        return 0U;
    }
    if (offset == REG_READ) {
        uint32_t value = fake->queued_response;
        fake->queued_response = 0U;
        return value;
    }
    return 0U;
}

static void fake_write32(void *context, uint32_t offset, uint32_t value) {
    fake_mailbox_t *fake = (fake_mailbox_t *)context;

    if (offset != REG_WRITE) {
        return;
    }
    fake->written = value;
    if (value != (fake->expected_address | CHANNEL_PROPERTY)) {
        return;
    }

    fake->request_valid =
        fake->message[0] == 8U * sizeof(uint32_t) &&
        fake->message[1] == 0U &&
        fake->message[2] == TAG_GET_CLOCK_RATE &&
        fake->message[3] == 8U &&
        fake->message[4] == 0U &&
        fake->message[5] == RPI_FIRMWARE_CLOCK_EMMC2 &&
        fake->message[6] == 0U &&
        fake->message[7] == 0U;
    if (fake->request_valid == 0U) {
        return;
    }

    fake->message[1] = 0x80000000U;
    fake->message[4] = 0x80000008U;
    fake->message[6] = 200000000U;
    fake->queued_response = value;
}

static void fake_barrier(void *context) {
    fake_mailbox_t *fake = (fake_mailbox_t *)context;
    fake->barriers++;
}

static int test_clock_rate_request(void) {
    uint32_t message[8] __attribute__((aligned(16)));
    fake_mailbox_t fake;
    rpi_mailbox_io_t io;
    uint32_t rate = 0U;
    uintptr_t arm_address = 0x00102030U;

    memset(message, 0, sizeof(message));
    memset(&fake, 0, sizeof(fake));
    fake.message = message;
    fake.expected_address = TEST_BUS_ALIAS + (uint32_t)arm_address;
    io.read32 = fake_read32;
    io.write32 = fake_write32;
    io.barrier = fake_barrier;
    io.context = &fake;

    ASSERT_EQ_U32(RPI_MAILBOX_OK,
                  rpi_mailbox_get_clock_rate_with_io(
                      &io, arm_address, TEST_BUS_ALIAS, message,
                      RPI_FIRMWARE_CLOCK_EMMC2, &rate));
    ASSERT_EQ_U32(200000000U, rate);
    ASSERT_EQ_U32(fake.expected_address | CHANNEL_PROPERTY, fake.written);
    ASSERT_EQ_U32(2U, fake.barriers);
    ASSERT_TRUE(fake.request_valid != 0U);
    return 0;
}

static int test_timeout_and_validation(void) {
    uint32_t message[8] __attribute__((aligned(16)));
    fake_mailbox_t fake;
    rpi_mailbox_io_t io;
    uint32_t rate = 0U;
    uintptr_t arm_address = 0x00102030U;

    memset(message, 0, sizeof(message));
    memset(&fake, 0, sizeof(fake));
    fake.message = message;
    fake.expected_address = TEST_BUS_ALIAS + (uint32_t)arm_address;
    io.read32 = fake_read32;
    io.write32 = fake_write32;
    io.barrier = fake_barrier;
    io.context = &fake;

    fake.full_forever = 1U;
    ASSERT_EQ_U32(RPI_MAILBOX_ERR_TIMEOUT,
                  rpi_mailbox_get_clock_rate_with_io(
                      &io, arm_address, TEST_BUS_ALIAS, message,
                      RPI_FIRMWARE_CLOCK_EMMC2, &rate));

    fake.full_forever = 0U;
    fake.empty_forever = 1U;
    ASSERT_EQ_U32(RPI_MAILBOX_ERR_TIMEOUT,
                  rpi_mailbox_get_clock_rate_with_io(
                      &io, arm_address, TEST_BUS_ALIAS, message,
                      RPI_FIRMWARE_CLOCK_EMMC2, &rate));

    ASSERT_EQ_U32(RPI_MAILBOX_ERR_INVAL,
                  rpi_mailbox_get_clock_rate_with_io(
                      &io, arm_address + 1U, TEST_BUS_ALIAS, message,
                      RPI_FIRMWARE_CLOCK_EMMC2, &rate));
    ASSERT_EQ_U32(RPI_MAILBOX_ERR_INVAL,
                  rpi_mailbox_get_clock_rate_with_io(
                      &io, arm_address, TEST_BUS_ALIAS + 1U, message,
                      RPI_FIRMWARE_CLOCK_EMMC2, &rate));
    ASSERT_EQ_U32(RPI_MAILBOX_ERR_INVAL,
                  rpi_mailbox_get_clock_rate_with_io(
                      &io, 0x40000000U, TEST_BUS_ALIAS, message,
                      RPI_FIRMWARE_CLOCK_EMMC2, &rate));
    ASSERT_EQ_U32(RPI_MAILBOX_ERR_INVAL,
                  rpi_mailbox_get_clock_rate_with_io(
                      &io, arm_address, TEST_BUS_ALIAS, message, 0U, &rate));
    return 0;
}

int main(void) {
    if (test_clock_rate_request() != 0) {
        return 1;
    }
    if (test_timeout_and_validation() != 0) {
        return 1;
    }
    puts("rpi-mailbox-test: PASS");
    return 0;
}
