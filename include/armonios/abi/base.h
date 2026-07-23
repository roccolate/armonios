#ifndef ARMONIOS_INCLUDE_ARMONIOS_ABI_BASE_H
#define ARMONIOS_INCLUDE_ARMONIOS_ABI_BASE_H

#include <stdint.h>

/* Fixed-width scalar types used by public ArmoniOS ABI structures. */
typedef int64_t  arm_status_t;
typedef uint32_t arm_pid_t;
typedef int32_t  arm_fd_t;

_Static_assert(sizeof(arm_status_t) == 8, "ABI drift: arm_status_t");
_Static_assert(sizeof(arm_pid_t) == 4, "ABI drift: arm_pid_t");
_Static_assert(sizeof(arm_fd_t) == 4, "ABI drift: arm_fd_t");

#endif
