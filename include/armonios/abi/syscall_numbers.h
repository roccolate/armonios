#ifndef ARMONIOS_INCLUDE_ARMONIOS_ABI_SYSCALL_NUMBERS_H
#define ARMONIOS_INCLUDE_ARMONIOS_ABI_SYSCALL_NUMBERS_H

/*
 * Public syscall number table — ArmoniOS ABI.
 *
 * Existing numbers are frozen. New syscalls must be appended to an existing
 * reserved range or to a newly documented range. A number must never be reused,
 * even after an implementation is retired, because standalone user images may
 * remain on disk after the kernel is upgraded.
 *
 * Kernel and userland include this same header. The compatibility header at
 * kernel/syscall_numbers.h exists only for older in-tree includes.
 */

#define SYS_EXIT               1ULL
_Static_assert(SYS_EXIT == 1ULL, "ABI drift: SYS_EXIT — see docs/SYSCALLS.md");

#define SYS_YIELD              2ULL
_Static_assert(SYS_YIELD == 2ULL, "ABI drift: SYS_YIELD — see docs/SYSCALLS.md");

#define SYS_GETPID             3ULL
_Static_assert(SYS_GETPID == 3ULL, "ABI drift: SYS_GETPID — see docs/SYSCALLS.md");

#define SYS_SPAWN              4ULL
_Static_assert(SYS_SPAWN == 4ULL, "ABI drift: SYS_SPAWN — see docs/SYSCALLS.md");

#define SYS_WAIT               6ULL
_Static_assert(SYS_WAIT == 6ULL, "ABI drift: SYS_WAIT — see docs/SYSCALLS.md");

#define SYS_KILL               7ULL
_Static_assert(SYS_KILL == 7ULL, "ABI drift: SYS_KILL — see docs/SYSCALLS.md");

#define SYS_SPAWN_ARGV         8ULL
_Static_assert(SYS_SPAWN_ARGV == 8ULL,
               "ABI drift: SYS_SPAWN_ARGV — see docs/SYSCALLS.md");

#define SYS_MMAP               20ULL
_Static_assert(SYS_MMAP == 20ULL, "ABI drift: SYS_MMAP — see docs/SYSCALLS.md");

#define SYS_MUNMAP             21ULL
_Static_assert(SYS_MUNMAP == 21ULL, "ABI drift: SYS_MUNMAP — see docs/SYSCALLS.md");

#define SYS_OPEN               40ULL
_Static_assert(SYS_OPEN == 40ULL, "ABI drift: SYS_OPEN — see docs/SYSCALLS.md");

#define SYS_CLOSE              41ULL
_Static_assert(SYS_CLOSE == 41ULL, "ABI drift: SYS_CLOSE — see docs/SYSCALLS.md");

#define SYS_READ               42ULL
_Static_assert(SYS_READ == 42ULL, "ABI drift: SYS_READ — see docs/SYSCALLS.md");

#define SYS_WRITE              43ULL
_Static_assert(SYS_WRITE == 43ULL, "ABI drift: SYS_WRITE — see docs/SYSCALLS.md");

#define SYS_SEEK               44ULL
_Static_assert(SYS_SEEK == 44ULL, "ABI drift: SYS_SEEK — see docs/SYSCALLS.md");

#define SYS_STAT               45ULL
_Static_assert(SYS_STAT == 45ULL, "ABI drift: SYS_STAT — see docs/SYSCALLS.md");

#define SYS_READDIR            46ULL
_Static_assert(SYS_READDIR == 46ULL,
               "ABI drift: SYS_READDIR — see docs/SYSCALLS.md");

#define SYS_UNLINK             47ULL
_Static_assert(SYS_UNLINK == 47ULL,
               "ABI drift: SYS_UNLINK — see docs/SYSCALLS.md");

#define SYS_RENAME             48ULL
_Static_assert(SYS_RENAME == 48ULL,
               "ABI drift: SYS_RENAME — see docs/SYSCALLS.md");

#define SYS_STAT_V2            49ULL
_Static_assert(SYS_STAT_V2 == 49ULL,
               "ABI drift: SYS_STAT_V2 — see docs/SYSCALLS.md");

#define SYS_READDIR_V2         50ULL
_Static_assert(SYS_READDIR_V2 == 50ULL,
               "ABI drift: SYS_READDIR_V2 — see docs/SYSCALLS.md");

#define SYS_IPC_SEND           60ULL
_Static_assert(SYS_IPC_SEND == 60ULL,
               "ABI drift: SYS_IPC_SEND — see docs/SYSCALLS.md");

#define SYS_IPC_RECV           61ULL
_Static_assert(SYS_IPC_RECV == 61ULL,
               "ABI drift: SYS_IPC_RECV — see docs/SYSCALLS.md");

#define SYS_WINDOW_CREATE      70ULL
_Static_assert(SYS_WINDOW_CREATE == 70ULL,
               "ABI drift: SYS_WINDOW_CREATE — see docs/SYSCALLS.md");

#define SYS_WINDOW_DESTROY     71ULL
_Static_assert(SYS_WINDOW_DESTROY == 71ULL,
               "ABI drift: SYS_WINDOW_DESTROY — see docs/SYSCALLS.md");

#define SYS_WINDOW_DRAW_TEXT   72ULL
_Static_assert(SYS_WINDOW_DRAW_TEXT == 72ULL,
               "ABI drift: SYS_WINDOW_DRAW_TEXT — see docs/SYSCALLS.md");

#define SYS_WINDOW_DRAW_RECT   73ULL
_Static_assert(SYS_WINDOW_DRAW_RECT == 73ULL,
               "ABI drift: SYS_WINDOW_DRAW_RECT — see docs/SYSCALLS.md");

#define SYS_WINDOW_EVENT       74ULL
_Static_assert(SYS_WINDOW_EVENT == 74ULL,
               "ABI drift: SYS_WINDOW_EVENT — see docs/SYSCALLS.md");

#define SYS_WINDOW_SET_TITLE   75ULL
_Static_assert(SYS_WINDOW_SET_TITLE == 75ULL,
               "ABI drift: SYS_WINDOW_SET_TITLE — see docs/SYSCALLS.md");

#define SYS_WINDOW_REDRAW      76ULL
_Static_assert(SYS_WINDOW_REDRAW == 76ULL,
               "ABI drift: SYS_WINDOW_REDRAW — see docs/SYSCALLS.md");

#define SYS_WINDOW_FOCUS       77ULL
_Static_assert(SYS_WINDOW_FOCUS == 77ULL,
               "ABI drift: SYS_WINDOW_FOCUS — see docs/SYSCALLS.md");

#define SYS_WINDOW_FOR_PID     78ULL
_Static_assert(SYS_WINDOW_FOR_PID == 78ULL,
               "ABI drift: SYS_WINDOW_FOR_PID — see docs/SYSCALLS.md");

#define SYS_CURSOR_SET_SHAPE   79ULL
_Static_assert(SYS_CURSOR_SET_SHAPE == 79ULL,
               "ABI drift: SYS_CURSOR_SET_SHAPE — see docs/SYSCALLS.md");

#define SYS_WINDOW_FLUSH       80ULL
_Static_assert(SYS_WINDOW_FLUSH == 80ULL,
               "ABI drift: SYS_WINDOW_FLUSH — see docs/SYSCALLS.md");

#define SYS_WINDOW_GET_BOUNDS  81ULL
_Static_assert(SYS_WINDOW_GET_BOUNDS == 81ULL,
               "ABI drift: SYS_WINDOW_GET_BOUNDS — see docs/SYSCALLS.md");

#define SYS_WINDOW_SET_BOUNDS  82ULL
_Static_assert(SYS_WINDOW_SET_BOUNDS == 82ULL,
               "ABI drift: SYS_WINDOW_SET_BOUNDS — see docs/SYSCALLS.md");

#define SYS_WINDOW_MINIMIZE    83ULL
_Static_assert(SYS_WINDOW_MINIMIZE == 83ULL,
               "ABI drift: SYS_WINDOW_MINIMIZE — see docs/SYSCALLS.md");

#define SYS_WINDOW_RESTORE     84ULL
_Static_assert(SYS_WINDOW_RESTORE == 84ULL,
               "ABI drift: SYS_WINDOW_RESTORE — see docs/SYSCALLS.md");

#define SYS_WINDOW_STATE       85ULL
_Static_assert(SYS_WINDOW_STATE == 85ULL,
               "ABI drift: SYS_WINDOW_STATE — see docs/SYSCALLS.md");

#define SYS_CURSOR_REGISTER_REGION 86ULL
_Static_assert(SYS_CURSOR_REGISTER_REGION == 86ULL,
               "ABI drift: SYS_CURSOR_REGISTER_REGION — see docs/SYSCALLS.md");

#define SYS_TIMEINFO           100ULL
_Static_assert(SYS_TIMEINFO == 100ULL,
               "ABI drift: SYS_TIMEINFO — see docs/SYSCALLS.md");

#define SYS_MEMINFO            101ULL
_Static_assert(SYS_MEMINFO == 101ULL,
               "ABI drift: SYS_MEMINFO — see docs/SYSCALLS.md");

#define SYS_PROCLIST           102ULL
_Static_assert(SYS_PROCLIST == 102ULL,
               "ABI drift: SYS_PROCLIST — see docs/SYSCALLS.md");

/* Public range guards. */
_Static_assert(SYS_EXIT          >= 1ULL  && SYS_SPAWN_ARGV <= 8ULL,
               "ABI drift: process range");
_Static_assert(SYS_MMAP          >= 20ULL && SYS_MUNMAP <= 21ULL,
               "ABI drift: memory range");
_Static_assert(SYS_OPEN          >= 40ULL && SYS_READDIR_V2 <= 50ULL,
               "ABI drift: vfs range");
_Static_assert(SYS_IPC_SEND      >= 60ULL && SYS_IPC_RECV <= 61ULL,
               "ABI drift: ipc range");
_Static_assert(SYS_WINDOW_CREATE >= 70ULL && SYS_CURSOR_REGISTER_REGION <= 86ULL,
               "ABI drift: window range");
_Static_assert(SYS_TIMEINFO      >= 100ULL && SYS_PROCLIST <= 102ULL,
               "ABI drift: info range");

#endif
