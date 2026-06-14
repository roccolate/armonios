#include "irq/gicv2.h"

#include <stdint.h>

#define GICD_CTLR       0x000
#define GICD_ISENABLER  0x100
#define GICD_IPRIORITYR 0x400

#define GICC_CTLR 0x000
#define GICC_PMR  0x004
#define GICC_IAR  0x00c
#define GICC_EOIR 0x010

static uint64_t g_gicd_base;
static uint64_t g_gicc_base;

static volatile uint32_t *gicd_reg(uint32_t offset) {
    return (volatile uint32_t *)(g_gicd_base + offset);
}

static volatile uint32_t *gicc_reg(uint32_t offset) {
    return (volatile uint32_t *)(g_gicc_base + offset);
}

static volatile uint8_t *gicd_reg8(uint32_t offset) {
    return (volatile uint8_t *)(g_gicd_base + offset);
}

void gicv2_init(uint64_t distributor_base, uint64_t cpu_base) {
    g_gicd_base = distributor_base;
    g_gicc_base = cpu_base;

    *gicd_reg(GICD_CTLR) = 0;
    *gicc_reg(GICC_CTLR) = 0;

    *gicc_reg(GICC_PMR) = 0xff;

    *gicd_reg(GICD_CTLR) = 1;
    *gicc_reg(GICC_CTLR) = 1;
}

void gicv2_enable_irq(uint32_t irq) {
    uint32_t reg = GICD_ISENABLER + (irq / 32U) * sizeof(uint32_t);
    uint32_t bit = irq % 32U;

    *gicd_reg8(GICD_IPRIORITYR + irq) = 0x80;
    *gicd_reg(reg) = 1U << bit;
}

uint32_t gicv2_ack_irq(void) {
    return *gicc_reg(GICC_IAR) & 0x3ffU;
}

void gicv2_end_irq(uint32_t irq) {
    *gicc_reg(GICC_EOIR) = irq;
}
