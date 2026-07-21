#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "boards/rpi4/emmc2_probe_diag.h"

#define ASSERT_TRUE(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "assert failed: %s:%d: %s\n", \
                __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

#define ASSERT_EQ_U32(expected, actual) do { \
    uint32_t e_ = (uint32_t)(expected); \
    uint32_t a_ = (uint32_t)(actual); \
    if (e_ != a_) { \
        fprintf(stderr, \
                "assert failed: %s:%d: expected 0x%08x got 0x%08x\n", \
                __FILE__, __LINE__, e_, a_); \
        return 1; \
    } \
} while (0)

static int test_broken_cd_and_command_capture(void) {
    uint32_t regs[64];
    rpi4_emmc2_probe_diag_t diag;
    uint32_t value;

    memset(regs, 0, sizeof(regs));
    rpi4_emmc2_probe_diag_init(&diag, (uintptr_t)regs);

    ASSERT_EQ_U32(RPI4_EMMC2_DIAG_NO_COMMAND, diag.last_command);

    regs[RPI4_EMMC2_DIAG_PRESENT_STATE / 4U] = 0x00000001U;
    value = rpi4_emmc2_probe_diag_read32(
        &diag, RPI4_EMMC2_DIAG_PRESENT_STATE);
    ASSERT_EQ_U32(0x00030001U, value);
    ASSERT_EQ_U32(0x00000001U, diag.present_state);
    ASSERT_EQ_U32(RPI4_EMMC2_DIAG_PRESENT_STATE, diag.last_read_offset);
    ASSERT_EQ_U32(0x00000001U,
                  regs[RPI4_EMMC2_DIAG_PRESENT_STATE / 4U]);

    rpi4_emmc2_probe_diag_write32(
        &diag, RPI4_EMMC2_DIAG_ARGUMENT, 0x11223344U);
    rpi4_emmc2_probe_diag_write32(
        &diag, RPI4_EMMC2_DIAG_TRANSFER_COMMAND,
        (17U << 24U) | 0x003a0012U);

    ASSERT_EQ_U32(17U, diag.last_command);
    ASSERT_EQ_U32(0x11223344U, diag.last_argument);
    ASSERT_EQ_U32(0x11223344U,
                  regs[RPI4_EMMC2_DIAG_ARGUMENT / 4U]);
    ASSERT_EQ_U32((17U << 24U) | 0x003a0012U,
                  regs[RPI4_EMMC2_DIAG_TRANSFER_COMMAND / 4U]);
    return 0;
}

static int test_error_snapshot_survives_ack(void) {
    uint32_t regs[64];
    rpi4_emmc2_probe_diag_t diag;

    memset(regs, 0, sizeof(regs));
    rpi4_emmc2_probe_diag_init(&diag, (uintptr_t)regs);

    regs[RPI4_EMMC2_DIAG_PRESENT_STATE / 4U] = 0x00f00003U;
    regs[RPI4_EMMC2_DIAG_CLOCK_RESET / 4U] = 0x0e07000fU;
    regs[RPI4_EMMC2_DIAG_HOST_POWER / 4U] = 0x00000f00U;
    regs[RPI4_EMMC2_DIAG_INT_STATUS / 4U] = 0x00200000U;

    ASSERT_EQ_U32(0x00200000U,
                  rpi4_emmc2_probe_diag_read32(
                      &diag, RPI4_EMMC2_DIAG_INT_STATUS));
    ASSERT_EQ_U32(0x00200000U, diag.last_nonzero_interrupt_status);

    regs[RPI4_EMMC2_DIAG_INT_STATUS / 4U] = 0U;
    rpi4_emmc2_probe_diag_refresh(&diag);

    ASSERT_EQ_U32(0x00f00003U, diag.present_state);
    ASSERT_EQ_U32(0x0e07000fU, diag.clock_reset);
    ASSERT_EQ_U32(0x00000f00U, diag.host_power);
    ASSERT_EQ_U32(0x00200000U, diag.last_nonzero_interrupt_status);
    ASSERT_TRUE(diag.base == (uintptr_t)regs);
    return 0;
}

int main(void) {
    if (test_broken_cd_and_command_capture() != 0) {
        return 1;
    }
    if (test_error_snapshot_survives_ack() != 0) {
        return 1;
    }
    puts("rpi4-emmc2-probe-diag-test: PASS");
    return 0;
}
