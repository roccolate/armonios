#ifndef ARMONIOS_INCLUDE_ARMONIOS_ABI_MEMORY_H
#define ARMONIOS_INCLUDE_ARMONIOS_ABI_MEMORY_H

/* Public SYS_MMAP protection and reserved mapping flags. */
#define ARM_VM_PROT_READ  0x01ULL
#define ARM_VM_PROT_WRITE 0x02ULL
#define ARM_VM_PROT_EXEC  0x04ULL

#define ARM_MAP_SHARED    0x10ULL
#define ARM_MAP_FIXED     0x20ULL

#define ARM_VM_PROT_ALLOWED \
    (ARM_VM_PROT_READ | ARM_VM_PROT_WRITE | ARM_VM_PROT_EXEC)
#define ARM_VM_FLAGS_KNOWN \
    (ARM_VM_PROT_ALLOWED | ARM_MAP_SHARED | ARM_MAP_FIXED)

_Static_assert(ARM_VM_PROT_READ == 0x01ULL, "ABI drift: VM read flag");
_Static_assert(ARM_VM_PROT_WRITE == 0x02ULL, "ABI drift: VM write flag");
_Static_assert(ARM_VM_PROT_EXEC == 0x04ULL, "ABI drift: VM exec flag");
_Static_assert(ARM_MAP_SHARED == 0x10ULL, "ABI drift: MAP_SHARED");
_Static_assert(ARM_MAP_FIXED == 0x20ULL, "ABI drift: MAP_FIXED");

#endif
