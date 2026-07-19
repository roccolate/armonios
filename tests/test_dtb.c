#include "unity/unity.h"

#include "kernel/dtb.h"

#include <stdint.h>
#include <string.h>

#define FDT_MAGIC       0xd00dfeedU
#define FDT_BEGIN_NODE  0x00000001U
#define FDT_END_NODE    0x00000002U
#define FDT_PROP        0x00000003U
#define FDT_END         0x00000009U

typedef struct {
    uint8_t bytes[512];
    uint32_t pos;
} fdt_builder_t;

static void put_be32(fdt_builder_t *fdt, uint32_t value) {
    fdt->bytes[fdt->pos++] = (uint8_t)(value >> 24);
    fdt->bytes[fdt->pos++] = (uint8_t)(value >> 16);
    fdt->bytes[fdt->pos++] = (uint8_t)(value >> 8);
    fdt->bytes[fdt->pos++] = (uint8_t)value;
}

static void align_builder(fdt_builder_t *fdt) {
    while ((fdt->pos & 3U) != 0) {
        fdt->bytes[fdt->pos++] = 0;
    }
}

static void begin_node(fdt_builder_t *fdt, const char *name) {
    uint32_t len = (uint32_t)strlen(name) + 1U;

    put_be32(fdt, FDT_BEGIN_NODE);
    memcpy(&fdt->bytes[fdt->pos], name, len);
    fdt->pos += len;
    align_builder(fdt);
}

static void prop(fdt_builder_t *fdt, uint32_t nameoff, const void *value, uint32_t len) {
    put_be32(fdt, FDT_PROP);
    put_be32(fdt, len);
    put_be32(fdt, nameoff);
    memcpy(&fdt->bytes[fdt->pos], value, len);
    fdt->pos += len;
    align_builder(fdt);
}

static void prop_u32(fdt_builder_t *fdt, uint32_t nameoff, uint32_t value) {
    uint8_t be_value[4] = {
        (uint8_t)(value >> 24),
        (uint8_t)(value >> 16),
        (uint8_t)(value >> 8),
        (uint8_t)value,
    };

    prop(fdt, nameoff, be_value, sizeof(be_value));
}

static uint64_t build_framebuffer_dtb(uint8_t *dtb, int include_stride) {
    static const char strings[] =
        "#address-cells\0"
        "#size-cells\0"
        "compatible\0"
        "reg\0"
        "width\0"
        "height\0"
        "stride\0";
    enum {
        OFF_ADDRESS_CELLS = 0,
        OFF_SIZE_CELLS = 15,
        OFF_COMPATIBLE = 27,
        OFF_REG = 38,
        OFF_WIDTH = 42,
        OFF_HEIGHT = 48,
        OFF_STRIDE = 55,
    };
    const uint32_t header_size = 40;
    const uint32_t struct_offset = header_size;
    fdt_builder_t fdt = { { 0 }, struct_offset };
    uint8_t reg[12] = {
        0x00, 0x00, 0x00, 0x00,
        0x4c, 0x00, 0x00, 0x00,
        0x00, 0x10, 0x00, 0x00,
    };
    const char compatible[] = "simple-framebuffer";
    uint32_t struct_size;
    uint32_t strings_offset;
    uint32_t total_size;

    begin_node(&fdt, "");
    prop_u32(&fdt, OFF_ADDRESS_CELLS, 2);
    prop_u32(&fdt, OFF_SIZE_CELLS, 1);
    begin_node(&fdt, "framebuffer@4c000000");
    prop(&fdt, OFF_COMPATIBLE, compatible, sizeof(compatible));
    prop(&fdt, OFF_REG, reg, sizeof(reg));
    prop_u32(&fdt, OFF_WIDTH, 640);
    prop_u32(&fdt, OFF_HEIGHT, 480);
    if (include_stride != 0) {
        prop_u32(&fdt, OFF_STRIDE, 2560);
    }
    put_be32(&fdt, FDT_END_NODE);
    put_be32(&fdt, FDT_END_NODE);
    put_be32(&fdt, FDT_END);

    struct_size = fdt.pos - struct_offset;
    strings_offset = fdt.pos;
    memcpy(&fdt.bytes[strings_offset], strings, sizeof(strings));
    fdt.pos += sizeof(strings);
    total_size = fdt.pos;

    fdt.pos = 0;
    put_be32(&fdt, FDT_MAGIC);
    put_be32(&fdt, total_size);
    put_be32(&fdt, struct_offset);
    put_be32(&fdt, strings_offset);
    put_be32(&fdt, 0);
    put_be32(&fdt, 17);
    put_be32(&fdt, 16);
    put_be32(&fdt, 0);
    put_be32(&fdt, sizeof(strings));
    put_be32(&fdt, struct_size);

    memcpy(dtb, fdt.bytes, total_size);
    return (uint64_t)(uintptr_t)dtb;
}

void test_dtb_get_framebuffer_reads_simple_framebuffer(void) {
    uint8_t dtb[512] = { 0 };
    dtb_framebuffer_t framebuffer = { 0 };

    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)dtb_get_framebuffer(
                                      build_framebuffer_dtb(dtb, 1),
                                      &framebuffer));
    TEST_ASSERT_EQUAL_UINT64(0x4c000000ULL, framebuffer.base);
    TEST_ASSERT_EQUAL_UINT64(0x00100000ULL, framebuffer.size);
    TEST_ASSERT_EQUAL_UINT64(640, framebuffer.width);
    TEST_ASSERT_EQUAL_UINT64(480, framebuffer.height);
    TEST_ASSERT_EQUAL_UINT64(2560, framebuffer.stride_bytes);
}

void test_dtb_get_framebuffer_rejects_incomplete_node(void) {
    uint8_t dtb[512] = { 0 };
    dtb_framebuffer_t framebuffer = { 0 };

    TEST_ASSERT_EQUAL_UINT64((uint64_t)-1,
                             (uint64_t)dtb_get_framebuffer(
                                 build_framebuffer_dtb(dtb, 0), &framebuffer));
}

void test_dtb_get_framebuffer_rejects_header_offsets_outside_blob(void) {
    uint8_t dtb[512] = { 0 };
    dtb_framebuffer_t framebuffer = { 0 };

    (void)build_framebuffer_dtb(dtb, 1);
    dtb[8] = 0xffU;
    dtb[9] = 0xffU;
    dtb[10] = 0xffU;
    dtb[11] = 0xf0U;

    TEST_ASSERT_EQUAL_UINT64((uint64_t)-1,
                             (uint64_t)dtb_get_framebuffer(
                                 (uint64_t)(uintptr_t)dtb, &framebuffer));
}
