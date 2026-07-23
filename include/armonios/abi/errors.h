#ifndef ARMONIOS_INCLUDE_ARMONIOS_ABI_ERRORS_H
#define ARMONIOS_INCLUDE_ARMONIOS_ABI_ERRORS_H

/*
 * Public negative status values returned across the ArmoniOS syscall ABI.
 *
 * Existing numeric values and meanings are frozen. New errors must use new
 * negative values; do not reuse retired values because standalone applications
 * may compare status codes directly.
 */
#define ARMONIOS_ERR_NOMEM (-2LL)
#define ARMONIOS_ERR_NOENT (-3LL)
#define ARMONIOS_ERR_BADF  (-5LL)
#define ARMONIOS_ERR_INVAL (-7LL)
#define ARMONIOS_ERR_AGAIN (-11LL)
#define ARMONIOS_ERR_PERM  (-13LL)

_Static_assert(ARMONIOS_ERR_NOMEM == -2LL, "ABI drift: ENOMEM");
_Static_assert(ARMONIOS_ERR_NOENT == -3LL, "ABI drift: ENOENT");
_Static_assert(ARMONIOS_ERR_BADF  == -5LL, "ABI drift: EBADF");
_Static_assert(ARMONIOS_ERR_INVAL == -7LL, "ABI drift: EINVAL");
_Static_assert(ARMONIOS_ERR_AGAIN == -11LL, "ABI drift: EAGAIN");
_Static_assert(ARMONIOS_ERR_PERM  == -13LL, "ABI drift: EPERM");

#endif
