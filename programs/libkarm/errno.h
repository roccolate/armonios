// libkarm/errno.h
//
// Stable libkarm names for the public ArmoniOS syscall error ABI. Numeric
// values live in <include/armonios/abi/errors.h> so kernel and userland compile
// against one source of truth.
//
// `kli_isok` and `kli_again` exist because several syscalls (non-blocking read,
// IPC recv/send, window event poll) return EAGAIN as a normal control-flow
// value, not a hard error.

#ifndef ARMONIOS_PROGRAMS_LIBKARM_ERRNO_H
#define ARMONIOS_PROGRAMS_LIBKARM_ERRNO_H

#include "include/armonios/abi/errors.h"

/* Historical libkarm spellings retained for source compatibility. */
#define KLI_NOMEM ((long)ARMONIOS_ERR_NOMEM)
#define KLI_NOENT ((long)ARMONIOS_ERR_NOENT)
#define KLI_BADF  ((long)ARMONIOS_ERR_BADF)
#define KLI_INVAL ((long)ARMONIOS_ERR_INVAL)
#define KLI_AGAIN ((long)ARMONIOS_ERR_AGAIN)
#define KLI_PERM  ((long)ARMONIOS_ERR_PERM)

static inline int kli_isok(long ret) {
    return ret >= 0;
}

static inline int kli_again(long ret) {
    return ret == KLI_AGAIN;
}

#endif
