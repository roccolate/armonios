#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "kernel/gui.h"
#include "kernel/mm/vmm.h"
#include "kernel/process.h"
#include "kernel/syscall_helpers.h"
#include "kernel/syscall_internal.h"
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

static process_t *test_alloc_child(uint32_t pid, uint32_t parent_pid,
                                   const char *name) {
    process_t *child = process_alloc(pid, name);
    if (child != 0) {
        child->parent_pid = parent_pid;
    }
    return child;
}

static void child_zombie_survives_until_parent_waits(void) {
    process_t *parent;
    process_t *child;
    process_t *foreign;

    process_table_init();
    parent = test_alloc_child(1U, 0U, "parent");
    child = test_alloc_child(2U, 1U, "child");
    assert(parent != 0);
    assert(child != 0);
    assert(parent->parent_pid == 0U);
    assert(child->parent_pid == parent->pid);

    process_mark_exited(child, 0x44U);
    process_reclaim_zombies();
    assert(process_find(2U) == child);
    assert(process_count() == 2U);

    foreign = test_alloc_child(3U, 0U, "foreign");
    assert(foreign != 0);
    assert(sys_wait(foreign, 2U) == ERR_PERM);
    assert(process_find(2U) == child);
    assert(sys_wait(parent, 2U) == 0x44);
    assert(process_find(2U) == 0);

    process_release(foreign);
    assert(process_count() == 1U);
    process_release(parent);
}

static void orphan_zombies_are_reclaimed(void) {
    process_t *parent;
    process_t *child;

    process_table_init();
    parent = test_alloc_child(10U, 0U, "parent");
    child = test_alloc_child(11U, 10U, "child");
    assert(parent != 0);
    assert(child != 0);

    process_mark_exited(child, 2U);
    process_mark_exited(parent, 1U);
    process_reclaim_zombies();

    assert(process_find(10U) == 0);
    assert(process_find(11U) == 0);
    assert(process_count() == 0U);
}

int main(void) {
    child_zombie_survives_until_parent_waits();
    orphan_zombies_are_reclaimed();
    puts("process parent/wait lifecycle: ok");
    return 0;
}
