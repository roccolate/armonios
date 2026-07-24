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
#define KLI_NOMEM    ARMONIOS_ERR_NOMEM
#define KLI_NOENT    ARMONIOS_ERR_NOENT
#define KLI_BADF     ARMONIOS_ERR_BADF
#define KLI_INVAL    ARMONIOS_ERR_INVAL
#define KLI_AGAIN    ARMONIOS_ERR_AGAIN
#define KLI_PERM     ARMONIOS_ERR_PERM
#define KLI_EXIST    ARMONIOS_ERR_EXIST
#define KLI_NOTDIR   ARMONIOS_ERR_NOTDIR
#define KLI_ISDIR    ARMONIOS_ERR_ISDIR
#define KLI_NOTEMPTY ARMONIOS_ERR_NOTEMPTY
#define KLI_NOSPC    ARMONIOS_ERR_NOSPC
#define KLI_ROFS     ARMONIOS_ERR_ROFS
#define KLI_NOTSUP   ARMONIOS_ERR_NOTSUP
#define KLI_RANGE    ARMONIOS_ERR_RANGE

static inline int kli_isok(long ret) {
    return ret >= 0;
}

static inline int kli_again(long ret) {
    return ret == KLI_AGAIN;
}

#endif
