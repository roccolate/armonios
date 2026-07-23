#ifndef ARMONIOS_INCLUDE_ARMONIOS_ABI_SYSTEM_H
#define ARMONIOS_INCLUDE_ARMONIOS_ABI_SYSTEM_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint64_t total_pages;
    uint64_t free_pages;
} arm_meminfo_t;

typedef struct {
    uint64_t timer_ticks;
    uint64_t scheduler_ticks;
    uint64_t scheduler_quantums;
} arm_timeinfo_t;

_Static_assert(sizeof(arm_meminfo_t) == 16, "ABI drift: arm_meminfo_t");
_Static_assert(offsetof(arm_meminfo_t, total_pages) == 0,
               "ABI drift: meminfo total_pages offset");
_Static_assert(offsetof(arm_meminfo_t, free_pages) == 8,
               "ABI drift: meminfo free_pages offset");
_Static_assert(sizeof(arm_timeinfo_t) == 24, "ABI drift: arm_timeinfo_t");
_Static_assert(offsetof(arm_timeinfo_t, timer_ticks) == 0,
               "ABI drift: timeinfo timer_ticks offset");
_Static_assert(offsetof(arm_timeinfo_t, scheduler_ticks) == 8,
               "ABI drift: timeinfo scheduler_ticks offset");
_Static_assert(offsetof(arm_timeinfo_t, scheduler_quantums) == 16,
               "ABI drift: timeinfo scheduler_quantums offset");

#endif
