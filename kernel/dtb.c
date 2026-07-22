#include "kernel/dtb.h"

#include <stddef.h>
#include <stdint.h>

#include "kernel/dtb_reader.h"

int dtb_get_memory(uint64_t dtb_addr, dtb_memory_t *memory) {
    fdt_view_t view;
    int depth = -1;
    int in_memory = 0;
    uint32_t root_address_cells = 2;
    uint32_t root_size_cells = 1;

    if (memory == NULL || fdt_view_init(dtb_addr, &view) != 0) {
        return -1;
    }

    while (view.cursor < view.struct_end) {
        fdt_token_t token;

        if (fdt_next_token(&view, &depth, &token) != 0) {
            return -1;
        }

        if (token.token == FDT_BEGIN_NODE) {
            in_memory = token.depth == 1 &&
                        fdt_starts_with(token.name, "memory");
        } else if (token.token == FDT_END_NODE) {
            if (in_memory && token.depth == 1) {
                in_memory = 0;
            }
        } else if (token.token == FDT_PROP) {
            if (depth == 0 && fdt_streq(token.prop_name, "#address-cells") &&
                token.len >= sizeof(uint32_t)) {
                root_address_cells = fdt_be32(token.value);
            } else if (depth == 0 && fdt_streq(token.prop_name, "#size-cells") &&
                       token.len >= sizeof(uint32_t)) {
                root_size_cells = fdt_be32(token.value);
            } else if (in_memory && fdt_streq(token.prop_name, "reg")) {
                uint32_t needed = (root_address_cells + root_size_cells) * sizeof(uint32_t);

                if (token.len < needed || root_address_cells > 2 || root_size_cells > 2) {
                    return -1;
                }

                memory->base = fdt_read_cells(token.value, root_address_cells);
                memory->size = fdt_read_cells(token.value + root_address_cells,
                                              root_size_cells);
                return 0;
            }
        } else if (token.token == FDT_NOP) {
            continue;
        } else if (token.token == FDT_END) {
            break;
        }
    }

    return -1;
}

uint32_t dtb_total_size(uint64_t dtb_addr) {
    fdt_view_t view;

    if (fdt_view_init(dtb_addr, &view) != 0) {
        return 0;
    }

    return view.total_size;
}
