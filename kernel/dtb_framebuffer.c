#include "kernel/dtb.h"

#include <stddef.h>
#include <stdint.h>

#include "kernel/dtb_reader.h"

static int compatible_has(const char *value, uint32_t len, const char *needle) {
    uint32_t start = 0;

    for (uint32_t i = 0; i < len; i++) {
        if (value[i] == '\0') {
            if (fdt_streq(value + start, needle)) {
                return 1;
            }

            start = i + 1U;
        }
    }

    return 0;
}

int dtb_get_framebuffer(uint64_t dtb_addr, dtb_framebuffer_t *framebuffer) {
    fdt_view_t view;
    int depth = -1;
    int in_framebuffer = 0;
    int have_reg = 0;
    int have_width = 0;
    int have_height = 0;
    int have_stride = 0;
    uint32_t root_address_cells = 2;
    uint32_t root_size_cells = 1;

    if (framebuffer == NULL || fdt_view_init(dtb_addr, &view) != 0) {
        return -1;
    }

    while (view.cursor < view.struct_end) {
        fdt_token_t token;

        if (fdt_next_token(&view, &depth, &token) != 0) {
            return -1;
        }

        if (token.token == FDT_BEGIN_NODE) {
            if (token.depth == 1) {
                in_framebuffer = fdt_starts_with(token.name, "framebuffer");
                have_reg = 0;
                have_width = 0;
                have_height = 0;
                have_stride = 0;
            }
        } else if (token.token == FDT_END_NODE) {
            if (token.depth == 1 && in_framebuffer && have_reg != 0 && have_width != 0 &&
                have_height != 0 && have_stride != 0) {
                return 0;
            }

            if (token.depth == 1) {
                in_framebuffer = 0;
            }
        } else if (token.token == FDT_PROP) {
            if (depth == 0 && fdt_streq(token.prop_name, "#address-cells") &&
                token.len >= sizeof(uint32_t)) {
                root_address_cells = fdt_be32(token.value);
            } else if (depth == 0 && fdt_streq(token.prop_name, "#size-cells") &&
                       token.len >= sizeof(uint32_t)) {
                root_size_cells = fdt_be32(token.value);
            } else if (depth == 1 && fdt_streq(token.prop_name, "compatible") &&
                       compatible_has((const char *)token.value, token.len,
                                      "simple-framebuffer")) {
                in_framebuffer = 1;
            } else if (depth == 1 && in_framebuffer &&
                       fdt_streq(token.prop_name, "reg")) {
                uint32_t needed = (root_address_cells + root_size_cells) * sizeof(uint32_t);

                if (token.len < needed || root_address_cells > 2 || root_size_cells > 2) {
                    return -1;
                }

                framebuffer->base = fdt_read_cells(token.value, root_address_cells);
                framebuffer->size = fdt_read_cells(token.value + root_address_cells,
                                                   root_size_cells);
                have_reg = 1;
            } else if (depth == 1 && in_framebuffer &&
                       fdt_streq(token.prop_name, "width") &&
                       token.len >= sizeof(uint32_t)) {
                framebuffer->width = fdt_be32(token.value);
                have_width = 1;
            } else if (depth == 1 && in_framebuffer &&
                       fdt_streq(token.prop_name, "height") &&
                       token.len >= sizeof(uint32_t)) {
                framebuffer->height = fdt_be32(token.value);
                have_height = 1;
            } else if (depth == 1 && in_framebuffer &&
                       fdt_streq(token.prop_name, "stride") &&
                       token.len >= sizeof(uint32_t)) {
                framebuffer->stride_bytes = fdt_be32(token.value);
                have_stride = 1;
            }
        } else if (token.token == FDT_NOP) {
            continue;
        } else if (token.token == FDT_END) {
            break;
        }
    }

    return -1;
}
