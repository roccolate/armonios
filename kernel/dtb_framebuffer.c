#include "kernel/dtb.h"

#include <stddef.h>
#include <stdint.h>

#define FDT_MAGIC       0xd00dfeedU
#define FDT_BEGIN_NODE  0x00000001U
#define FDT_END_NODE    0x00000002U
#define FDT_PROP        0x00000003U
#define FDT_NOP         0x00000004U
#define FDT_END         0x00000009U

typedef struct {
    uint32_t magic;
    uint32_t totalsize;
    uint32_t off_dt_struct;
    uint32_t off_dt_strings;
    uint32_t off_mem_rsvmap;
    uint32_t version;
    uint32_t last_comp_version;
    uint32_t boot_cpuid_phys;
    uint32_t size_dt_strings;
    uint32_t size_dt_struct;
} fdt_header_t;

static uint32_t be32(const void *ptr) {
    const uint8_t *bytes = ptr;

    return ((uint32_t)bytes[0] << 24) |
           ((uint32_t)bytes[1] << 16) |
           ((uint32_t)bytes[2] << 8) |
           (uint32_t)bytes[3];
}

static uint64_t read_cells(const uint32_t *cells, uint32_t count) {
    uint64_t value = 0;

    for (uint32_t i = 0; i < count; i++) {
        value = (value << 32) | be32(&cells[i]);
    }

    return value;
}

static uintptr_t align4(uintptr_t value) {
    return (value + 3U) & ~(uintptr_t)3U;
}

static int streq(const char *a, const char *b) {
    while (*a == *b) {
        if (*a == '\0') {
            return 1;
        }

        a++;
        b++;
    }

    return 0;
}

static int starts_with(const char *value, const char *prefix) {
    while (*prefix != '\0') {
        if (*value != *prefix) {
            return 0;
        }

        value++;
        prefix++;
    }

    return 1;
}

static int compatible_has(const char *value, uint32_t len, const char *needle) {
    uint32_t start = 0;

    for (uint32_t i = 0; i < len; i++) {
        if (value[i] == '\0') {
            if (streq(value + start, needle)) {
                return 1;
            }

            start = i + 1U;
        }
    }

    return 0;
}

int dtb_get_framebuffer(uint64_t dtb_addr, dtb_framebuffer_t *framebuffer) {
    const fdt_header_t *header = (const fdt_header_t *)(uintptr_t)dtb_addr;
    const uint8_t *base = (const uint8_t *)(uintptr_t)dtb_addr;
    const uint8_t *struct_block;
    const char *strings;
    uintptr_t cursor;
    uintptr_t struct_end;
    int depth = -1;
    int in_framebuffer = 0;
    int have_reg = 0;
    int have_width = 0;
    int have_height = 0;
    int have_stride = 0;
    uint32_t root_address_cells = 2;
    uint32_t root_size_cells = 1;

    if (dtb_addr == 0 || framebuffer == NULL || be32(&header->magic) != FDT_MAGIC) {
        return -1;
    }

    struct_block = base + be32(&header->off_dt_struct);
    strings = (const char *)(base + be32(&header->off_dt_strings));
    cursor = (uintptr_t)struct_block;
    struct_end = cursor + be32(&header->size_dt_struct);

    while (cursor < struct_end) {
        uint32_t token = be32((const void *)cursor);
        cursor += sizeof(uint32_t);

        if (token == FDT_BEGIN_NODE) {
            const char *name = (const char *)cursor;

            depth++;
            if (depth == 1) {
                in_framebuffer = starts_with(name, "framebuffer");
                have_reg = 0;
                have_width = 0;
                have_height = 0;
                have_stride = 0;
            }

            while (*(const char *)cursor != '\0') {
                cursor++;
            }
            cursor = align4(cursor + 1U);
        } else if (token == FDT_END_NODE) {
            if (depth == 1 && in_framebuffer && have_reg != 0 && have_width != 0 &&
                have_height != 0 && have_stride != 0) {
                return 0;
            }

            if (depth == 1) {
                in_framebuffer = 0;
            }

            depth--;
        } else if (token == FDT_PROP) {
            uint32_t len = be32((const void *)cursor);
            uint32_t nameoff = be32((const void *)(cursor + sizeof(uint32_t)));
            const char *name = strings + nameoff;
            const uint32_t *value = (const uint32_t *)(cursor + sizeof(uint32_t) * 2U);

            cursor += sizeof(uint32_t) * 2U;

            if (depth == 0 && streq(name, "#address-cells") && len >= sizeof(uint32_t)) {
                root_address_cells = be32(value);
            } else if (depth == 0 && streq(name, "#size-cells") && len >= sizeof(uint32_t)) {
                root_size_cells = be32(value);
            } else if (depth == 1 && streq(name, "compatible") &&
                       compatible_has((const char *)value, len, "simple-framebuffer")) {
                in_framebuffer = 1;
            } else if (depth == 1 && in_framebuffer && streq(name, "reg")) {
                uint32_t needed = (root_address_cells + root_size_cells) * sizeof(uint32_t);

                if (len < needed || root_address_cells > 2 || root_size_cells > 2) {
                    return -1;
                }

                framebuffer->base = read_cells(value, root_address_cells);
                framebuffer->size = read_cells(value + root_address_cells, root_size_cells);
                have_reg = 1;
            } else if (depth == 1 && in_framebuffer && streq(name, "width") &&
                       len >= sizeof(uint32_t)) {
                framebuffer->width = be32(value);
                have_width = 1;
            } else if (depth == 1 && in_framebuffer && streq(name, "height") &&
                       len >= sizeof(uint32_t)) {
                framebuffer->height = be32(value);
                have_height = 1;
            } else if (depth == 1 && in_framebuffer && streq(name, "stride") &&
                       len >= sizeof(uint32_t)) {
                framebuffer->stride_bytes = be32(value);
                have_stride = 1;
            }

            cursor = align4(cursor + len);
        } else if (token == FDT_NOP) {
            continue;
        } else if (token == FDT_END) {
            break;
        } else {
            return -1;
        }
    }

    return -1;
}
