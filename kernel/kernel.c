#include <stdint.h>

#include "board.h"
#include "gpu/virtio_gpu.h"
#include "kernel/bootfs.h"
#include "kernel/console.h"
#include "kernel/dtb.h"
#include "kernel/exceptions.h"
#include "kernel/fat32.h"
#include "kernel/gui.h"
#include "kernel/ipc.h"
#include "kernel/irq.h"
#include "kernel/mm/kheap.h"
#include "kernel/mm/mmu.h"
#include "kernel/mm/pmm.h"
#include "kernel/mm/vmm.h"
#include "kernel/process.h"
#include "kernel/sched/sched.h"
#include "kernel/timer/timer.h"
#include "kernel/tmpfs.h"
#include "kernel/user_demo.h"
#include "kernel/vfs.h"
#include "storage/virtio_blk.h"
#include "uart/pl011.h"

extern char __kernel_end[];

void kernel_main(uint64_t dtb_addr);

static virtio_blk_device_t g_blk_dev;
static fat32_fs_t g_fat32_fs;

static void print_hex64(uint64_t value) {
    static const char digits[] = "0123456789abcdef";

    uart_puts("0x");
    for (int shift = 60; shift >= 0; shift -= 4) {
        uart_putc(digits[(value >> shift) & 0xf]);
    }
}

static void console_input_thread(void *arg) {
    (void)arg;

    for (;;) {
        int c = uart_getc_nonblock();

        if (c >= 0) {
            (void)gui_demo_send_key((char)c);
            console_poll_char((char)c);
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

static void init_vfs(void) {
    static const uint8_t note[] = "KolibriARM tmpfs\n";
    uint8_t magic[4];
    uint64_t bytes_read = 0;
    uint64_t bytes_written = 0;

    vfs_reset();
    tmpfs_reset();

    if (bootfs_mount_vfs() == 0) {
        uart_puts("VFS bootfs: mounted\n");
    } else {
        uart_puts("VFS bootfs: failed\n");
    }

    if (tmpfs_create("note") == 0 &&
        tmpfs_write("note", 0, note, sizeof(note) - 1U, &bytes_written) == 0 &&
        tmpfs_mount_vfs("/tmp/note", "note") == 0) {
        uart_puts("VFS tmpfs: mounted\n");
    } else {
        uart_puts("VFS tmpfs: failed\n");
    }

    if (bootfs_read("user_demo", 0, magic, sizeof(magic),
                    &bytes_read) == 0 && bytes_read == sizeof(magic)) {
        uart_puts("bootfs read: ok\n");
    } else {
        uart_puts("bootfs read: failed\n");
    }

    bytes_read = 0;
    if (vfs_read("/boot/user_demo", 0, magic, sizeof(magic),
                 &bytes_read) == 0 && bytes_read == sizeof(magic)) {
        uart_puts("VFS read: ok\n");
    } else {
        uart_puts("VFS read: failed\n");
    }
}

static void run_user_demo_smoke(const dtb_memory_t *memory) {
    uint64_t user_exit_code = user_demo_run(memory->base, memory->size,
                                            board_map_mmio);

    uart_puts("USER demo exit code: ");
    print_hex64(user_exit_code);
    uart_puts("\n");
}

static int fat32_read_virtio_sector(void *context, uint32_t lba,
                                    uint8_t *buffer) {
    return virtio_blk_read_sector((virtio_blk_device_t *)context, lba, buffer);
}

static int fat32_write_virtio_sector(void *context, uint32_t lba,
                                     const uint8_t *buffer) {
    return virtio_blk_write_sector((virtio_blk_device_t *)context, lba,
                                   buffer);
}

static int probe_fat32(void) {
    vfs_stat_t stat;

    fat32_vfs_reset();
    if (fat32_mount(&g_fat32_fs, fat32_read_virtio_sector, &g_blk_dev) != 0) {
        uart_puts("FAT32: absent\n");
        return -1;
    }
    fat32_set_write_sector(&g_fat32_fs, fat32_write_virtio_sector);

    uart_puts("FAT32: mounted\n");
    if (fat32_mount_vfs_root(&g_fat32_fs, "/fat") == 0) {
        uart_puts("FAT32 root: mounted\n");
    } else {
        uart_puts("FAT32 root: absent\n");
    }

    if (fat32_mount_vfs_file(&g_fat32_fs, "/fat/user_demo.bin",
                             "USERDEMO.BIN") != 0 ||
        vfs_stat("/fat/user_demo.bin", &stat) != 0) {
        uart_puts("FAT32 file: absent\n");
        return -1;
    }

    uart_puts("FAT32 file bytes: ");
    print_hex64(stat.size);
    uart_puts("\n");

    if (fat32_mount_vfs_file(&g_fat32_fs, "/fat/edit.txt",
                             "EDIT.TXT") == 0) {
        uart_puts("FAT32 edit file: mounted\n");
    } else {
        uart_puts("FAT32 edit file: absent\n");
    }
    return 0;
}

static int probe_storage(void) {
    virtio_blk_info_t blk;
    uint8_t sector[512] __attribute__((aligned(8)));
    uint64_t blk_base;
    int read_status;

    if (virtio_blk_probe_range(board_virtio_mmio_base(),
                               board_virtio_mmio_size(),
                               board_virtio_mmio_stride(), &blk_base,
                               &blk) != 0) {
        uart_puts("VIRTIO blk: absent\n");
        return -1;
    }

    uart_puts("VIRTIO blk base: ");
    print_hex64(blk_base);
    uart_puts("\n");
    uart_puts("VIRTIO blk sectors: ");
    print_hex64(blk.capacity_sectors);
    uart_puts("\n");

    read_status = virtio_blk_init(&g_blk_dev, blk_base);
    if (read_status == 0) {
        read_status = virtio_blk_read_sector(&g_blk_dev, 0, sector);
    }

    if (read_status == 0) {
        uint32_t word = (uint32_t)sector[0] |
                        ((uint32_t)sector[1] << 8) |
                        ((uint32_t)sector[2] << 16) |
                        ((uint32_t)sector[3] << 24);

        uart_puts("VIRTIO blk sector0: ");
        print_hex64(word);
        uart_puts("\n");
        return probe_fat32();
    } else {
        uart_puts("VIRTIO read err: ");
        print_hex64((uint64_t)(uint32_t)read_status);
        uart_puts("\n");
    }

    return -1;
}

static void init_display(void) {
    uint64_t gpu_base;

    if (virtio_gpu_probe_range(board_virtio_mmio_base(),
                               board_virtio_mmio_size(),
                               board_virtio_mmio_stride(), &gpu_base) != 0) {
        console_set_framebuffer_ready(0);
        return;
    }

    if (virtio_gpu_draw(gpu_base, gui_draw_demo, 0) == 0) {
        uart_puts("VIRTIO gpu: windows\n");
        console_set_framebuffer_ready(1);
    } else {
        uart_puts("VIRTIO gpu: failed\n");
        console_set_framebuffer_ready(0);
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
        ipc_init();
        console_init(&memory);
        run_pmm_smoke();
        run_kheap_smoke();

        if (user_demo_prepare_images() != 0) {
            uart_puts("USER image load: failed\n");
        } else if (enable_identity_mmu(&memory, dtb_addr) == 0) {
            init_vfs();
            init_timer_irq_demo();
            if (probe_storage() == 0 &&
                user_demo_prepare_vfs_images("/fat/user_demo.bin") == 0) {
                console_set_storage_ready(1);
                uart_puts("USER image source: FAT32\n");
            } else {
                console_set_storage_ready(0);
                (void)user_demo_prepare_images();
                uart_puts("USER image source: bootfs\n");
            }
            init_display();
            run_user_demo_smoke(&memory);
            console_start_interactive();
            start_scheduler_demo();
        }
    } else {
        uart_puts("RAM map: unavailable\n");
    }

    for (;;) {
        __asm__ volatile("wfe");
    }
}
