#ifndef ARMONIOS_INCLUDE_ARMONIOS_ABI_SYSCALL_NUMBERS_H
#define ARMONIOS_INCLUDE_ARMONIOS_ABI_SYSCALL_NUMBERS_H

/*
 * Public syscall number table — ArmoniOS ABI.
 *
 * Existing numbers are frozen. New syscalls must be appended to an existing
 * reserved range or to a newly documented range. A number must never be reused.
 */
#define SYS_EXIT               1ULL
#define SYS_YIELD              2ULL
#define SYS_GETPID             3ULL
#define SYS_SPAWN              4ULL
#define SYS_WAIT               6ULL
#define SYS_KILL               7ULL
#define SYS_SPAWN_ARGV         8ULL
#define SYS_MMAP               20ULL
#define SYS_MUNMAP             21ULL
#define SYS_OPEN               40ULL
#define SYS_CLOSE              41ULL
#define SYS_READ               42ULL
#define SYS_WRITE              43ULL
#define SYS_SEEK               44ULL
#define SYS_STAT               45ULL
#define SYS_READDIR            46ULL
#define SYS_UNLINK             47ULL
#define SYS_RENAME             48ULL
#define SYS_STAT_V2            49ULL
#define SYS_READDIR_V2         50ULL
#define SYS_IPC_SEND           60ULL
#define SYS_IPC_RECV           61ULL
#define SYS_WINDOW_CREATE      70ULL
#define SYS_WINDOW_DESTROY     71ULL
#define SYS_WINDOW_DRAW_TEXT   72ULL
#define SYS_WINDOW_DRAW_RECT   73ULL
#define SYS_WINDOW_EVENT       74ULL
#define SYS_WINDOW_SET_TITLE   75ULL
#define SYS_WINDOW_REDRAW      76ULL
#define SYS_WINDOW_FOCUS       77ULL
#define SYS_WINDOW_FOR_PID     78ULL
#define SYS_CURSOR_SET_SHAPE   79ULL
#define SYS_WINDOW_FLUSH       80ULL
#define SYS_WINDOW_GET_BOUNDS  81ULL
#define SYS_WINDOW_SET_BOUNDS  82ULL
#define SYS_WINDOW_MINIMIZE    83ULL
#define SYS_WINDOW_RESTORE     84ULL
#define SYS_WINDOW_STATE       85ULL
#define SYS_CURSOR_REGISTER_REGION 86ULL
#define SYS_TIMEINFO           100ULL
#define SYS_MEMINFO            101ULL
#define SYS_PROCLIST           102ULL

_Static_assert(SYS_EXIT == 1ULL, "ABI drift: SYS_EXIT");
_Static_assert(SYS_YIELD == 2ULL, "ABI drift: SYS_YIELD");
_Static_assert(SYS_GETPID == 3ULL, "ABI drift: SYS_GETPID");
_Static_assert(SYS_SPAWN == 4ULL, "ABI drift: SYS_SPAWN");
_Static_assert(SYS_WAIT == 6ULL, "ABI drift: SYS_WAIT");
_Static_assert(SYS_KILL == 7ULL, "ABI drift: SYS_KILL");
_Static_assert(SYS_SPAWN_ARGV == 8ULL, "ABI drift: SYS_SPAWN_ARGV");
_Static_assert(SYS_MMAP == 20ULL, "ABI drift: SYS_MMAP");
_Static_assert(SYS_MUNMAP == 21ULL, "ABI drift: SYS_MUNMAP");
_Static_assert(SYS_OPEN == 40ULL, "ABI drift: SYS_OPEN");
_Static_assert(SYS_CLOSE == 41ULL, "ABI drift: SYS_CLOSE");
_Static_assert(SYS_READ == 42ULL, "ABI drift: SYS_READ");
_Static_assert(SYS_WRITE == 43ULL, "ABI drift: SYS_WRITE");
_Static_assert(SYS_SEEK == 44ULL, "ABI drift: SYS_SEEK");
_Static_assert(SYS_STAT == 45ULL, "ABI drift: SYS_STAT");
_Static_assert(SYS_READDIR == 46ULL, "ABI drift: SYS_READDIR");
_Static_assert(SYS_UNLINK == 47ULL, "ABI drift: SYS_UNLINK");
_Static_assert(SYS_RENAME == 48ULL, "ABI drift: SYS_RENAME");
_Static_assert(SYS_STAT_V2 == 49ULL, "ABI drift: SYS_STAT_V2");
_Static_assert(SYS_READDIR_V2 == 50ULL, "ABI drift: SYS_READDIR_V2");
_Static_assert(SYS_IPC_SEND == 60ULL, "ABI drift: SYS_IPC_SEND");
_Static_assert(SYS_IPC_RECV == 61ULL, "ABI drift: SYS_IPC_RECV");
_Static_assert(SYS_WINDOW_CREATE == 70ULL, "ABI drift: SYS_WINDOW_CREATE");
_Static_assert(SYS_WINDOW_DESTROY == 71ULL, "ABI drift: SYS_WINDOW_DESTROY");
_Static_assert(SYS_WINDOW_DRAW_TEXT == 72ULL, "ABI drift: SYS_WINDOW_DRAW_TEXT");
_Static_assert(SYS_WINDOW_DRAW_RECT == 73ULL, "ABI drift: SYS_WINDOW_DRAW_RECT");
_Static_assert(SYS_WINDOW_EVENT == 74ULL, "ABI drift: SYS_WINDOW_EVENT");
_Static_assert(SYS_WINDOW_SET_TITLE == 75ULL, "ABI drift: SYS_WINDOW_SET_TITLE");
_Static_assert(SYS_WINDOW_REDRAW == 76ULL, "ABI drift: SYS_WINDOW_REDRAW");
_Static_assert(SYS_WINDOW_FOCUS == 77ULL, "ABI drift: SYS_WINDOW_FOCUS");
_Static_assert(SYS_WINDOW_FOR_PID == 78ULL, "ABI drift: SYS_WINDOW_FOR_PID");
_Static_assert(SYS_CURSOR_SET_SHAPE == 79ULL, "ABI drift: SYS_CURSOR_SET_SHAPE");
_Static_assert(SYS_WINDOW_FLUSH == 80ULL, "ABI drift: SYS_WINDOW_FLUSH");
_Static_assert(SYS_WINDOW_GET_BOUNDS == 81ULL, "ABI drift: SYS_WINDOW_GET_BOUNDS");
_Static_assert(SYS_WINDOW_SET_BOUNDS == 82ULL, "ABI drift: SYS_WINDOW_SET_BOUNDS");
_Static_assert(SYS_WINDOW_MINIMIZE == 83ULL, "ABI drift: SYS_WINDOW_MINIMIZE");
_Static_assert(SYS_WINDOW_RESTORE == 84ULL, "ABI drift: SYS_WINDOW_RESTORE");
_Static_assert(SYS_WINDOW_STATE == 85ULL, "ABI drift: SYS_WINDOW_STATE");
_Static_assert(SYS_CURSOR_REGISTER_REGION == 86ULL,
               "ABI drift: SYS_CURSOR_REGISTER_REGION");
_Static_assert(SYS_TIMEINFO == 100ULL, "ABI drift: SYS_TIMEINFO");
_Static_assert(SYS_MEMINFO == 101ULL, "ABI drift: SYS_MEMINFO");
_Static_assert(SYS_PROCLIST == 102ULL, "ABI drift: SYS_PROCLIST");

_Static_assert(SYS_EXIT >= 1ULL && SYS_SPAWN_ARGV <= 8ULL,
               "ABI drift: process range");
_Static_assert(SYS_MMAP >= 20ULL && SYS_MUNMAP <= 21ULL,
               "ABI drift: memory range");
_Static_assert(SYS_OPEN >= 40ULL && SYS_READDIR_V2 <= 50ULL,
               "ABI drift: vfs range");
_Static_assert(SYS_IPC_SEND >= 60ULL && SYS_IPC_RECV <= 61ULL,
               "ABI drift: ipc range");
_Static_assert(SYS_WINDOW_CREATE >= 70ULL && SYS_CURSOR_REGISTER_REGION <= 86ULL,
               "ABI drift: window range");
_Static_assert(SYS_TIMEINFO >= 100ULL && SYS_PROCLIST <= 102ULL,
               "ABI drift: info range");

#endif
