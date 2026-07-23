// libkarm/syscall.h
//
// Typed C wrappers for the stable ArmoniOS syscall surface. Return values are
// raw kernel longs: non-negative on success, negative <errno.h> values on error.

#ifndef ARMONIOS_PROGRAMS_LIBKARM_SYSCALL_H
#define ARMONIOS_PROGRAMS_LIBKARM_SYSCALL_H

#include <stddef.h>
#include <stdint.h>

#include "include/armonios/abi/memory.h"
#include "include/armonios/abi/process.h"
#include "include/armonios/abi/syscall_numbers.h"
#include "include/armonios/abi/system.h"
#include "include/armonios/abi/vfs.h"

long __syscall0(long n);
long __syscall1(long n, long a0);
long __syscall2(long n, long a0, long a1);
long __syscall3(long n, long a0, long a1, long a2);
long __syscall4(long n, long a0, long a1, long a2, long a3);
long __syscall5(long n, long a0, long a1, long a2, long a3, long a4);
long __syscall6(long n, long a0, long a1, long a2, long a3, long a4, long a5);
long __syscall7(long n, long a0, long a1, long a2, long a3, long a4,
                long a5, long a6);

static inline long kli_exit(long code) {
    return __syscall1(SYS_EXIT, code);
}
static inline long kli_yield(void) {
    return __syscall0(SYS_YIELD);
}
static inline long kli_getpid(void) {
    return __syscall0(SYS_GETPID);
}
static inline long kli_spawn(const char *path, long entry_index) {
    return __syscall2(SYS_SPAWN, (long)(uintptr_t)path, entry_index);
}
static inline long kli_wait(long pid) {
    return __syscall1(SYS_WAIT, pid);
}
static inline long kli_kill(long pid) {
    return __syscall1(SYS_KILL, pid);
}
static inline long kli_spawn_argv(const char *path, long entry_index,
                                  const long *argv_ptr, long argc) {
    return __syscall4(SYS_SPAWN_ARGV, (long)(uintptr_t)path, entry_index,
                      (long)(uintptr_t)argv_ptr, argc);
}

static inline long kli_mmap(uintptr_t hint, size_t size, long flags) {
    return __syscall3(SYS_MMAP, (long)hint, (long)size, flags);
}
static inline long kli_munmap(uintptr_t vaddr, size_t size) {
    return __syscall2(SYS_MUNMAP, (long)vaddr, (long)size);
}

static inline long kli_open(const char *path, long flags) {
    return __syscall2(SYS_OPEN, (long)(uintptr_t)path, flags);
}
static inline long kli_close(int fd) {
    return __syscall1(SYS_CLOSE, (long)fd);
}
static inline long kli_read(int fd, void *buf, size_t len) {
    return __syscall3(SYS_READ, (long)fd, (long)(uintptr_t)buf, (long)len);
}
static inline long kli_write(int fd, const void *buf, size_t len) {
    return __syscall3(SYS_WRITE, (long)fd, (long)(uintptr_t)buf, (long)len);
}
void kli_write_cstr(int fd, const char *s);
static inline long kli_seek(int fd, long offset, long whence) {
    return __syscall3(SYS_SEEK, (long)fd, offset, whence);
}

static inline long kli_stat_v1(const char *path, arm_stat_t *stat_ptr) {
    return __syscall2(SYS_STAT, (long)(uintptr_t)path,
                      (long)(uintptr_t)stat_ptr);
}
static inline long kli_stat(const char *path, void *stat_ptr) {
    return __syscall2(SYS_STAT, (long)(uintptr_t)path,
                      (long)(uintptr_t)stat_ptr);
}
static inline long kli_readdir(const char *path, void *buf, size_t len) {
    return __syscall3(SYS_READDIR, (long)(uintptr_t)path,
                      (long)(uintptr_t)buf, (long)len);
}

/*
 * Structured metadata calls. Before kli_stat_v2, initialize version and
 * struct_size with ARM_VFS_METADATA_VERSION and sizeof(*stat_ptr).
 * kli_readdir_v2 returns the number of complete entries copied.
 */
static inline long kli_stat_v2(const char *path, arm_stat_v2_t *stat_ptr) {
    return __syscall2(SYS_STAT_V2, (long)(uintptr_t)path,
                      (long)(uintptr_t)stat_ptr);
}
static inline long kli_readdir_v2(const char *path, uint64_t start_index,
                                  arm_dirent_v2_t *entries,
                                  size_t max_entries) {
    return __syscall4(SYS_READDIR_V2, (long)(uintptr_t)path,
                      (long)start_index, (long)(uintptr_t)entries,
                      (long)max_entries);
}

static inline long kli_unlink(const char *path_ptr) {
    return __syscall1(SYS_UNLINK, (long)(uintptr_t)path_ptr);
}
static inline long kli_rename(const char *old_ptr, const char *new_ptr) {
    return __syscall2(SYS_RENAME, (long)(uintptr_t)old_ptr,
                      (long)(uintptr_t)new_ptr);
}

static inline long kli_ipc_send(long target_pid, const void *buf, size_t len) {
    return __syscall3(SYS_IPC_SEND, target_pid, (long)(uintptr_t)buf,
                      (long)len);
}
static inline long kli_ipc_recv(void *buf, size_t capacity) {
    return __syscall2(SYS_IPC_RECV, (long)(uintptr_t)buf, (long)capacity);
}

static inline long kli_timeinfo_v1(arm_timeinfo_t *info_ptr) {
    return __syscall1(SYS_TIMEINFO, (long)(uintptr_t)info_ptr);
}
static inline long kli_timeinfo(uint64_t *info_ptr) {
    return __syscall1(SYS_TIMEINFO, (long)(uintptr_t)info_ptr);
}
static inline long kli_meminfo_v1(arm_meminfo_t *info_ptr) {
    return __syscall1(SYS_MEMINFO, (long)(uintptr_t)info_ptr);
}
static inline long kli_meminfo(uint64_t *info_ptr) {
    return __syscall1(SYS_MEMINFO, (long)(uintptr_t)info_ptr);
}
static inline long kli_proclist_v1(arm_process_entry_t *entries,
                                   size_t max_entries) {
    return __syscall2(SYS_PROCLIST, (long)(uintptr_t)entries,
                      (long)max_entries);
}
static inline long kli_proclist(void *entries, size_t max_entries) {
    return __syscall2(SYS_PROCLIST, (long)(uintptr_t)entries,
                      (long)max_entries);
}

#endif
