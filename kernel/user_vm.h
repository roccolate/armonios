#ifndef KOLIBRIARM_KERNEL_USER_VM_H
#define KOLIBRIARM_KERNEL_USER_VM_H

#include <stdint.h>

#include "kernel/process.h"

#define USER_VM_PROT_READ  0x01ULL
#define USER_VM_PROT_WRITE 0x02ULL
#define USER_VM_PROT_EXEC  0x04ULL

#define USER_VM_ERR_NOMEM (-2LL)
#define USER_VM_ERR_INVAL (-7LL)

int64_t user_vm_map_anonymous(process_t *process, uint64_t hint,
                              uint64_t size, uint64_t flags);
int64_t user_vm_unmap_anonymous(process_t *process, uint64_t addr,
                                uint64_t size);
int user_vm_map_physical(process_t *process, uint64_t vaddr, uint64_t paddr,
                         uint64_t size, uint64_t flags);

#endif
