/*
 * Parent/child lifecycle policy for the fixed process table.
 *
 * process.c owns PCB storage, contexts, VM resources, and state transitions.
 * This module owns the higher-level rules around spawn parentage, wait(), and
 * safe automatic reclamation. A zombie with a live parent remains observable
 * until that parent waits; only kernel-owned or orphaned zombies are reaped
 * automatically to recover fixed process-table capacity.
 */

#include "kernel/process.h"

#include <stdint.h>

process_t *process_alloc_child(uint32_t pid, uint32_t parent_pid,
                               const char *name) {
    process_t *parent;
    process_t *child;

    if (pid == 0U || parent_pid == pid) {
        return 0;
    }

    if (parent_pid != 0U) {
        parent = process_find(parent_pid);
        if (parent == 0 || parent->state == PROCESS_UNUSED ||
            parent->state == PROCESS_ZOMBIE) {
            return 0;
        }
    }

    child = process_alloc(pid, name);
    if (child != 0) {
        child->parent_pid = parent_pid;
    }
    return child;
}

int process_wait_child_zombie(uint32_t parent_pid, uint32_t pid,
                              uint64_t *exit_code) {
    process_t *child;

    if (parent_pid == 0U || exit_code == 0) {
        return -1;
    }

    child = process_find(pid);
    if (child == 0 || child->parent_pid != parent_pid ||
        child->state != PROCESS_ZOMBIE) {
        return -1;
    }

    return process_wait_zombie(pid, exit_code);
}

void process_reclaim_orphan_zombies(void) {
    for (uint32_t i = 0; i < PROCESS_MAX_PROCESSES; i++) {
        const process_t *slot = process_at(i);
        process_t *parent;
        process_t *zombie;

        if (slot == 0 || slot->state != PROCESS_ZOMBIE) {
            continue;
        }

        parent = slot->parent_pid == 0U ? 0 : process_find(slot->parent_pid);
        if (slot->parent_pid != 0U && parent != 0 &&
            parent->state != PROCESS_UNUSED &&
            parent->state != PROCESS_ZOMBIE) {
            continue;
        }

        zombie = process_find(slot->pid);
        if (zombie != 0 && zombie->state == PROCESS_ZOMBIE) {
            process_release(zombie);
        }
    }
}
