#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "kernel/gui.h"
#include "kernel/mm/vmm.h"
#include "kernel/process.h"
#include "kernel/vfs.h"

uint32_t vfs_close_all_for_pid(uint32_t pid) {
    (void)pid;
    return 0;
}

gui_desktop_t *gui_desktop(void) {
    return 0;
}

void gui_destroy_windows_for_pid(gui_desktop_t *desktop, uint32_t pid) {
    (void)desktop;
    (void)pid;
}

void pmm_free_page(uint64_t paddr) {
    (void)paddr;
}

void vmm_free_table(uint64_t *pgd) {
    (void)pgd;
}

static void child_zombie_survives_until_parent_waits(void) {
    process_t *parent;
    process_t *child;
    uint64_t exit_code = 0;

    process_table_init();
    parent = process_alloc_child(1U, 0U, "parent");
    child = process_alloc_child(2U, 1U, "child");
    assert(parent != 0);
    assert(child != 0);
    assert(parent->parent_pid == 0U);
    assert(child->parent_pid == parent->pid);

    process_mark_exited(child, 0x44U);
    process_reclaim_orphan_zombies();
    assert(process_find(2U) == child);
    assert(process_count() == 2U);

    assert(process_wait_child_zombie(3U, 2U, &exit_code) != 0);
    assert(process_find(2U) == child);
    assert(process_wait_child_zombie(1U, 2U, &exit_code) == 0);
    assert(exit_code == 0x44U);
    assert(process_find(2U) == 0);
    assert(process_count() == 1U);

    process_release(parent);
}

static void orphan_zombies_are_reclaimed(void) {
    process_t *parent;
    process_t *child;

    process_table_init();
    parent = process_alloc_child(10U, 0U, "parent");
    child = process_alloc_child(11U, 10U, "child");
    assert(parent != 0);
    assert(child != 0);

    process_mark_exited(child, 2U);
    process_mark_exited(parent, 1U);
    process_reclaim_orphan_zombies();

    assert(process_find(10U) == 0);
    assert(process_find(11U) == 0);
    assert(process_count() == 0U);
}

static void child_allocation_rejects_invalid_parentage(void) {
    process_t *root;

    process_table_init();
    assert(process_alloc_child(4U, 4U, "self") == 0);
    assert(process_alloc_child(5U, 99U, "missing") == 0);

    root = process_alloc_child(1U, 0U, "root");
    assert(root != 0);
    process_mark_exited(root, 0U);
    assert(process_alloc_child(2U, 1U, "zombie-parent") == 0);
    process_reclaim_orphan_zombies();
}

int main(void) {
    child_zombie_survives_until_parent_waits();
    orphan_zombies_are_reclaimed();
    child_allocation_rejects_invalid_parentage();
    puts("process parent/wait lifecycle: ok");
    return 0;
}
