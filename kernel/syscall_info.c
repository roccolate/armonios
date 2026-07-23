#include "kernel/syscall_internal.h"

#include <stdint.h>

#include "include/armonios/abi/process.h"
#include "include/armonios/abi/system.h"
#include "kernel/kstring.h"
#include "kernel/mm/pmm.h"
#include "kernel/sched/sched.h"
#include "kernel/syscall_helpers.h"
#include "kernel/timer/timer.h"

int64_t sys_meminfo(process_t *process, uint64_t info_ptr) {
    arm_meminfo_t info;

    info.total_pages = pmm_total_count();
    info.free_pages = pmm_free_count();
    return sys_copy_to_user(process, info_ptr, &info, sizeof(info));
}

int64_t sys_timeinfo(process_t *process, uint64_t info_ptr) {
    arm_timeinfo_t info;

    info.timer_ticks = timer_ticks();
    info.scheduler_ticks = sched_ticks();
    info.scheduler_quantums = sched_quantums();
    return sys_copy_to_user(process, info_ptr, &info, sizeof(info));
}

int64_t sys_proclist(process_t *process, uint64_t entries_ptr,
                     uint64_t max_entries) {
    uint64_t written = 0;
    int64_t status;

    if (max_entries == 0) {
        return 0;
    }
    if (max_entries > PROCESS_MAX_PROCESSES) {
        return ERR_INVAL;
    }

    status = sys_user_buf_out(process, entries_ptr,
                              max_entries * sizeof(arm_process_entry_t));
    if (status != 0) {
        return status;
    }

    for (uint32_t i = 0; i < PROCESS_MAX_PROCESSES && written < max_entries;
         i++) {
        const process_t *slot = process_at(i);
        arm_process_entry_t entry;
        const char *name;
        uint32_t j;

        if (slot == 0 || slot->state == PROCESS_ZOMBIE) {
            continue;
        }

        entry.pid = slot->pid;
        entry.state = (uint32_t)slot->state;
        name = slot->name != 0 ? slot->name : "";

        for (j = 0; j + 1U < sizeof(entry.name) && name[j] != '\0'; j++) {
            entry.name[j] = name[j];
        }
        entry.name[j] = '\0';
        for (j++; j < sizeof(entry.name); j++) {
            entry.name[j] = '\0';
        }

        kmemcpy((void *)(uintptr_t)(
                    entries_ptr + written * sizeof(arm_process_entry_t)),
                &entry, sizeof(entry));
        written++;
    }

    return (int64_t)written;
}
