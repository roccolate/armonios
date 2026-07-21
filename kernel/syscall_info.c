#include "kernel/syscall_internal.h"

#include <stdint.h>

#include "kernel/mm/pmm.h"
#include "kernel/sched/sched.h"
#include "kernel/syscall_helpers.h"
#include "kernel/timer/timer.h"

typedef struct {
    uint32_t pid;
    uint32_t state;
    char name[16];
} syscall_proc_entry_t;

int64_t sys_meminfo(process_t *process, uint64_t info_ptr) {
    uint64_t info[2];
    int64_t status;

    status = sys_user_buf_out(process, info_ptr, sizeof(info));
    if (status != 0) {
        return status;
    }

    info[0] = pmm_total_count();
    info[1] = pmm_free_count();
    return sys_copy_to_user(process, info_ptr, info, sizeof(info));
}

int64_t sys_timeinfo(process_t *process, uint64_t info_ptr) {
    uint64_t info[3];
    int64_t status;

    status = sys_user_buf_out(process, info_ptr, sizeof(info));
    if (status != 0) {
        return status;
    }

    info[0] = timer_ticks();
    info[1] = sched_ticks();
    info[2] = sched_quantums();
    return sys_copy_to_user(process, info_ptr, info, sizeof(info));
}

int64_t sys_proclist(process_t *process, uint64_t entries_ptr,
                     uint64_t max_entries) {
    syscall_proc_entry_t entries[PROCESS_MAX_PROCESSES];
    uint64_t written = 0;
    int64_t status;

    if (max_entries == 0) {
        return 0;
    }
    if (max_entries > PROCESS_MAX_PROCESSES) {
        return ERR_INVAL;
    }

    status = sys_user_buf_out(process, entries_ptr,
                              max_entries * sizeof(syscall_proc_entry_t));
    if (status != 0) {
        return status;
    }

    for (uint32_t i = 0; i < PROCESS_MAX_PROCESSES && written < max_entries;
         i++) {
        const process_t *slot = process_at(i);
        syscall_proc_entry_t *entry;
        const char *name;
        uint32_t j;

        if (slot == 0 || slot->state == PROCESS_ZOMBIE) {
            continue;
        }

        entry = &entries[written];
        entry->pid = slot->pid;
        entry->state = (uint32_t)slot->state;
        name = slot->name != 0 ? slot->name : "";

        for (j = 0; j + 1U < sizeof(entry->name) && name[j] != '\0'; j++) {
            entry->name[j] = name[j];
        }
        entry->name[j] = '\0';
        for (j++; j < sizeof(entry->name); j++) {
            entry->name[j] = '\0';
        }

        written++;
    }

    status = sys_copy_to_user(process, entries_ptr, entries,
                              written * sizeof(syscall_proc_entry_t));
    return status == 0 ? (int64_t)written : status;
}
