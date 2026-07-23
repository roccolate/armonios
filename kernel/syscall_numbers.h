/*
 * Compatibility include for the historical kernel-private path.
 *
 * Syscall numbers are a public kernel/userland ABI contract and now live under
 * include/armonios/abi/. New kernel and userland code should include the public
 * header directly. Keep this wrapper until all existing in-tree includes have
 * migrated.
 */
#ifndef ARMONIOS_KERNEL_SYSCALL_NUMBERS_H
#define ARMONIOS_KERNEL_SYSCALL_NUMBERS_H

#include "include/armonios/abi/syscall_numbers.h"

#endif
