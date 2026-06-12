#ifndef KOLIBRIARM_KERNEL_USER_DEMO_H
#define KOLIBRIARM_KERNEL_USER_DEMO_H

#include <stdint.h>

uint64_t user_demo_run(void);
int user_demo_range_contains(uint64_t start, uint64_t end);
uint64_t user_demo_return_address(void);

#endif
