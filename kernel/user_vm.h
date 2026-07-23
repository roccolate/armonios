#ifndef ARMONIOS_KERNEL_USER_VM_H
#define ARMONIOS_KERNEL_USER_VM_H

#include <stdint.h>

#include "include/armonios/abi/errors.h"
#include "kernel/process.h"

/*
 * User VM API shared by sys_mmap/sys_munmap and the panel loader.
 *
 * flags=0 maps anonymous memory as readable/writable. Non-zero flags must be
 * a combination of USER_VM_PROT_* bits; MAP_SHARED and MAP_FIXED are reserved
 * at the syscall ABI level but intentionally rejected here until implemented.
 * Anonymous mappings allocate PMM pages and mark the process region as owned;
 * physical mappings install an existing registered range without taking PMM
 * ownership.
 */

#define USER_VM_PROT_READ  0x01ULL
#define USER_VM_PROT_WRITE 0x02ULL
#define USER_VM_PROT_EXEC  0x04ULL

/* Historical VM-local spellings retained as aliases of the public ABI. */
#define USER_VM_ERR_NOMEM ARMONIOS_ERR_NOMEM
#define USER_VM_ERR_INVAL ARMONIOS_ERR_INVAL

int64_t user_vm_map_anonymous(process_t *process, uint64_t hint,
                              uint64_t size, uint64_t flags);
int64_t user_vm_unmap_anonymous(process_t *process, uint64_t addr,
                                uint64_t size);
int user_vm_map_physical(process_t *process, uint64_t vaddr, uint64_t paddr,
                         uint64_t size, uint64_t flags);

#endif
