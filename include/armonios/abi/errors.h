#ifndef ARMONIOS_INCLUDE_ARMONIOS_ABI_ERRORS_H
#define ARMONIOS_INCLUDE_ARMONIOS_ABI_ERRORS_H

/*
 * Public negative status values returned across the ArmoniOS syscall ABI.
 *
 * Existing numeric values and meanings are frozen. New errors must use new
 * negative values; do not reuse retired values because standalone applications
 * may compare status codes directly. These are ArmoniOS-native values, not a
 * promise of POSIX/Linux errno numbering.
 */
#define ARMONIOS_ERR_NOMEM    (-2LL)
#define ARMONIOS_ERR_NOENT    (-3LL)
#define ARMONIOS_ERR_BADF     (-5LL)
#define ARMONIOS_ERR_INVAL    (-7LL)
#define ARMONIOS_ERR_AGAIN    (-11LL)
#define ARMONIOS_ERR_PERM     (-13LL)
#define ARMONIOS_ERR_EXIST    (-14LL)
#define ARMONIOS_ERR_NOTDIR   (-15LL)
#define ARMONIOS_ERR_ISDIR    (-16LL)
#define ARMONIOS_ERR_NOTEMPTY (-17LL)
#define ARMONIOS_ERR_NOSPC    (-18LL)
#define ARMONIOS_ERR_ROFS     (-19LL)
#define ARMONIOS_ERR_NOTSUP   (-20LL)
#define ARMONIOS_ERR_RANGE    (-21LL)

_Static_assert(ARMONIOS_ERR_NOMEM    == -2LL,  "ABI drift: NOMEM");
_Static_assert(ARMONIOS_ERR_NOENT    == -3LL,  "ABI drift: NOENT");
_Static_assert(ARMONIOS_ERR_BADF     == -5LL,  "ABI drift: BADF");
_Static_assert(ARMONIOS_ERR_INVAL    == -7LL,  "ABI drift: INVAL");
_Static_assert(ARMONIOS_ERR_AGAIN    == -11LL, "ABI drift: AGAIN");
_Static_assert(ARMONIOS_ERR_PERM     == -13LL, "ABI drift: PERM");
_Static_assert(ARMONIOS_ERR_EXIST    == -14LL, "ABI drift: EXIST");
_Static_assert(ARMONIOS_ERR_NOTDIR   == -15LL, "ABI drift: NOTDIR");
_Static_assert(ARMONIOS_ERR_ISDIR    == -16LL, "ABI drift: ISDIR");
_Static_assert(ARMONIOS_ERR_NOTEMPTY == -17LL, "ABI drift: NOTEMPTY");
_Static_assert(ARMONIOS_ERR_NOSPC    == -18LL, "ABI drift: NOSPC");
_Static_assert(ARMONIOS_ERR_ROFS     == -19LL, "ABI drift: ROFS");
_Static_assert(ARMONIOS_ERR_NOTSUP   == -20LL, "ABI drift: NOTSUP");
_Static_assert(ARMONIOS_ERR_RANGE    == -21LL, "ABI drift: RANGE");

#endif
