#ifndef ARMONIOS_KERNEL_PANEL_BOOT_H
#define ARMONIOS_KERNEL_PANEL_BOOT_H

#include <stdint.h>

/*
 * EL0 boot helpers. The kernel boots its first userland process (the
 * panel taskbar) through panel_boot_run; later apps come up via
 * app_spawn_vfs from sys_spawn / sys_spawn_argv; el0_return_address
 * points at the trampoline the lower-EL exception vector returns to
 * after sys_exit.
 *
 * `app_spawn_vfs` accepts only kernel-owned argv pointers and strings. The
 * syscall boundary must copy all EL0 argv data before calling it.
 *
 * The file used to be called user_demo.* and hosted an embedded
 * programs/user_demo.S blob. That blob is gone; the loader now owns
 * boot images through kernel/user_image.c and the bootfs registry.
 * The kernel-side helpers here are the EL0 launch path, not a demo.
 */

typedef int (*panel_map_mmio_fn_t)(uint64_t *pgd);

uint64_t panel_boot_run(uint64_t memory_base, uint64_t memory_size,
                        panel_map_mmio_fn_t map_mmio);
int app_spawn_vfs(const char *path, uint32_t entry_index,
                  const uint64_t *argv_ptr, uint32_t argc);
uint64_t el0_return_address(void);

#endif
