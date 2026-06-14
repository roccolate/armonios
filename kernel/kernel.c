#include <stdint.h>

#include "board.h"
#include "gpu/virtio_gpu.h"
#include "kernel/dtb.h"
#include "kernel/exceptions.h"
#include "kernel/irq.h"
#include "kernel/mm/kheap.h"
#include "kernel/mm/mmu.h"
#include "kernel/mm/pmm.h"
#include "kernel/mm/vmm.h"
#include "kernel/process.h"
#include "kernel/sched/sched.h"
#include "kernel/timer/timer.h"
#include "kernel/user_demo.h"
#include "storage/virtio_blk.h"
#include "uart/pl011.h"

extern char __kernel_end[];

void kernel_main(uint64_t dtb_addr);

static void print_hex64(uint64_t value) {
    static const char digits[] = "0123456789abcdef";

    uart_puts("0x");
    for (int shift = 60; shift >= 0; shift -= 4) {
        uart_putc(digits[(value >> shift) & 0xf]);
    }
}

static void demo_thread(void *arg) {
    const char *name = arg;

    for (uint64_t i = 0; i < 3; i++) {
        uart_puts("THREAD ");
        uart_puts(name);
        uart_puts(" step: ");
        print_hex64(i);
        uart_puts("\n");
        sched_yield();
    }
}

static void console_input_thread(void *arg) {
    (void)arg;

    for (;;) {
        int c = uart_getc_nonblock();

        if (c >= 0) {
            uart_puts("KEY ");
            uart_putc((char)c);
            uart_puts("\n");
        } else {
            __asm__ volatile("wfe");
        }

        sched_yield();
    }
}

static void init_memory_manager(const dtb_memory_t *memory, uint64_t dtb_addr) {
    pmm_init(memory->base, memory->size);
    pmm_reserve_range(memory->base,
                      (uint64_t)((uintptr_t)__kernel_end - memory->base));
    pmm_reserve_range(dtb_addr, dtb_total_size(dtb_addr));
}

static void run_pmm_smoke(void) {
    uint64_t page_a = pmm_alloc_page();
    uint64_t page_b = pmm_alloc_page();

    pmm_free_page(page_a);
    pmm_free_page(page_b);
}

static void run_kheap_smoke(void) {
    kheap_init();

    void *heap_a = kmalloc(64);
    void *heap_b = kmalloc(128);

    kfree(heap_a);
    void *heap_c = kmalloc(32);

    kfree(heap_b);
    kfree(heap_c);
}

static void run_user_demo_smoke(const dtb_memory_t *memory) {
    uint64_t user_exit_code = user_demo_run(memory->base, memory->size,
                                            board_map_mmio);

    uart_puts("USER demo exit code: ");
    print_hex64(user_exit_code);
    uart_puts("\n");
}

static void probe_storage(void) {
    virtio_blk_info_t blk;
    virtio_blk_device_t blk_dev;
    uint8_t sector[512] __attribute__((aligned(8)));
    uint64_t blk_base;
    int read_status;

    if (virtio_blk_probe_range(board_virtio_mmio_base(),
                               board_virtio_mmio_size(),
                               board_virtio_mmio_stride(), &blk_base,
                               &blk) != 0) {
        uart_puts("VIRTIO blk: absent\n");
        return;
    }

    uart_puts("VIRTIO blk base: ");
    print_hex64(blk_base);
    uart_puts("\n");
    uart_puts("VIRTIO blk sectors: ");
    print_hex64(blk.capacity_sectors);
    uart_puts("\n");

    read_status = virtio_blk_init(&blk_dev, blk_base);
    if (read_status == 0) {
        read_status = virtio_blk_read_sector(&blk_dev, 0, sector);
    }

    if (read_status == 0) {
        uint32_t word = (uint32_t)sector[0] |
                        ((uint32_t)sector[1] << 8) |
                        ((uint32_t)sector[2] << 16) |
                        ((uint32_t)sector[3] << 24);

        uart_puts("VIRTIO blk sector0: ");
        print_hex64(word);
        uart_puts("\n");
    } else {
        uart_puts("VIRTIO read err: ");
        print_hex64((uint64_t)(uint32_t)read_status);
        uart_puts("\n");
    }
}

static void init_display(void) {
    uint64_t gpu_base;

    if (virtio_gpu_probe_range(board_virtio_mmio_base(),
                               board_virtio_mmio_size(),
                               board_virtio_mmio_stride(), &gpu_base) != 0) {
        return;
    }

    if (virtio_gpu_draw_test_pattern(gpu_base) == 0) {
        uart_puts("VIRTIO gpu: rect\n");
    } else {
        uart_puts("VIRTIO gpu: failed\n");
    }
}

static int enable_identity_mmu(const dtb_memory_t *memory, uint64_t dtb_addr) {
    uint64_t *kernel_pgd = vmm_new_table();
    int vmm_ok = 0;

    (void)dtb_addr;

    if (kernel_pgd != 0) {
        vmm_ok = vmm_map_range(kernel_pgd, memory->base, memory->base,
                               memory->size,
                               VMM_FLAG_READ | VMM_FLAG_WRITE | VMM_FLAG_EXEC);
    } else {
        vmm_ok = -1;
    }

    if (vmm_ok == 0) {
        vmm_ok = board_map_mmio(kernel_pgd);
    }

    if (vmm_ok != 0) {
        uart_puts("VMM smoke: failed\n");
        return -1;
    }

    mmu_enable_identity(kernel_pgd);

    uart_puts("MMU identity map: active\n");

    return 0;
}

static void init_timer_irq_demo(void) {
    board_irq_init();
    (void)irq_register_handler(TIMER_IRQ, timer_handle_irq, 0);
    (void)irq_register_handler(board_uart0_irq(), uart_irq_handler, 0);
    board_irq_enable(TIMER_IRQ);
    board_irq_enable(board_uart0_irq());
    uart_enable_rx_irq();
    sched_init(5);
    timer_init(100);
    irq_enable();
}

static void start_scheduler_demo(void) {
    (void)sched_create_kernel_thread(demo_thread, "A", "thread-a");
    (void)sched_create_kernel_thread(demo_thread, "B", "thread-b");
    (void)sched_create_kernel_thread(console_input_thread, 0, "console-input");

    sched_start();
}

void kernel_main(uint64_t dtb_addr) {
    dtb_memory_t memory;

    board_early_init();

    uart_puts("\nKolibriARM ");
    uart_puts(board_name());
    uart_puts("\n");

    if (dtb_get_memory(dtb_addr, &memory) == 0) {
        init_memory_manager(&memory, dtb_addr);
        process_table_init();
        run_pmm_smoke();
        run_kheap_smoke();

        if (user_demo_prepare_images() != 0) {
            uart_puts("USER image load: failed\n");
        } else if (enable_identity_mmu(&memory, dtb_addr) == 0) {
            init_timer_irq_demo();
            probe_storage();
            init_display();
            run_user_demo_smoke(&memory);
            start_scheduler_demo();
        }
    } else {
        uart_puts("RAM map: unavailable\n");
    }

    for (;;) {
        __asm__ volatile("wfe");
    }
}
