#ifndef ARMONIOS_INCLUDE_ARMONIOS_ABI_PROCESS_H
#define ARMONIOS_INCLUDE_ARMONIOS_ABI_PROCESS_H

#include <stddef.h>
#include <stdint.h>

#include "base.h"

#define ARM_PROCESS_NAME_LEN 16U

/* Observable state values returned by SYS_PROCLIST. */
#define ARM_PROCESS_UNUSED  0U
#define ARM_PROCESS_READY   1U
#define ARM_PROCESS_RUNNING 2U
#define ARM_PROCESS_BLOCKED 3U
#define ARM_PROCESS_ZOMBIE  4U

/* Kernel-generated exit codes observable through SYS_WAIT. */
#define ARM_PROCESS_EXIT_KILLED 0x80ULL
#define ARM_PROCESS_EXIT_FAULT  0xfffffffffffffff0ULL

typedef struct {
    arm_pid_t pid;
    uint32_t state;
    char name[ARM_PROCESS_NAME_LEN];
} arm_process_entry_t;

_Static_assert(sizeof(arm_process_entry_t) == 24,
               "ABI drift: arm_process_entry_t");
_Static_assert(offsetof(arm_process_entry_t, pid) == 0,
               "ABI drift: process pid offset");
_Static_assert(offsetof(arm_process_entry_t, state) == 4,
               "ABI drift: process state offset");
_Static_assert(offsetof(arm_process_entry_t, name) == 8,
               "ABI drift: process name offset");

#endif
