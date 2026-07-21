from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, content: str) -> None:
    target = ROOT / path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(content, encoding="utf-8")


def replace_once(path: str, old: str, new: str) -> None:
    text = read(path)
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected one match, found {count}: {old!r}")
    write(path, text.replace(old, new, 1))


def replace_all_exact(path: str, old: str, new: str, expected: int) -> None:
    text = read(path)
    count = text.count(old)
    if count != expected:
        raise RuntimeError(f"{path}: expected {expected} matches, found {count}: {old!r}")
    write(path, text.replace(old, new))


replace_once(
    "kernel/process.h",
    " * PIDs are non-zero and unique inside the fixed process table. User regions\n",
    " * PIDs are non-zero and unique inside the fixed process table. Spawned\n"
    " * processes record their parent PID so only the parent can reap a child exit.\n"
    " * User regions\n",
)
replace_once(
    "kernel/process.h",
    "typedef struct process {\n    uint32_t pid;\n    const char *name;\n",
    "typedef struct process {\n    uint32_t pid;\n    uint32_t parent_pid;\n    const char *name;\n",
)
replace_once(
    "kernel/process.h",
    "process_t *process_alloc(uint32_t pid, const char *name);\n",
    "process_t *process_alloc(uint32_t pid, const char *name);\n"
    "process_t *process_alloc_child(uint32_t pid, uint32_t parent_pid,\n"
    "                               const char *name);\n",
)
replace_once(
    "kernel/process.h",
    "void process_reclaim_zombies(void);\nint process_wait_zombie(uint32_t pid, uint64_t *exit_code);\n",
    "void process_reclaim_zombies(void);\n"
    "void process_reclaim_orphan_zombies(void);\n"
    "int process_wait_zombie(uint32_t pid, uint64_t *exit_code);\n"
    "int process_wait_child_zombie(uint32_t parent_pid, uint32_t pid,\n"
    "                              uint64_t *exit_code);\n",
)
replace_once(
    "kernel/process.c",
    "    process->pid = pid;\n    process_copy_name(process, name);\n",
    "    process->pid = pid;\n    process->parent_pid = 0;\n    process_copy_name(process, name);\n",
)

write(
    "kernel/process_lifecycle.c",
    '''/*
 * Parent/child lifecycle policy for the fixed process table.
 *
 * process.c owns PCB storage, contexts, VM resources, and state transitions.
 * This module owns the higher-level rules around spawn parentage, wait(), and
 * safe automatic reclamation. A zombie with a live parent remains observable
 * until that parent waits; only kernel-owned or orphaned zombies are reaped
 * automatically to recover fixed process-table capacity.
 */

#include "kernel/process.h"

#include <stdint.h>

process_t *process_alloc_child(uint32_t pid, uint32_t parent_pid,
                               const char *name) {
    process_t *parent;
    process_t *child;

    if (pid == 0U || parent_pid == pid) {
        return 0;
    }

    if (parent_pid != 0U) {
        parent = process_find(parent_pid);
        if (parent == 0 || parent->state == PROCESS_UNUSED ||
            parent->state == PROCESS_ZOMBIE) {
            return 0;
        }
    }

    child = process_alloc(pid, name);
    if (child != 0) {
        child->parent_pid = parent_pid;
    }
    return child;
}

int process_wait_child_zombie(uint32_t parent_pid, uint32_t pid,
                              uint64_t *exit_code) {
    process_t *child;

    if (parent_pid == 0U || exit_code == 0) {
        return -1;
    }

    child = process_find(pid);
    if (child == 0 || child->parent_pid != parent_pid ||
        child->state != PROCESS_ZOMBIE) {
        return -1;
    }

    return process_wait_zombie(pid, exit_code);
}

void process_reclaim_orphan_zombies(void) {
    for (uint32_t i = 0; i < PROCESS_MAX_PROCESSES; i++) {
        const process_t *slot = process_at(i);
        process_t *parent;
        process_t *zombie;

        if (slot == 0 || slot->state != PROCESS_ZOMBIE) {
            continue;
        }

        parent = slot->parent_pid == 0U ? 0 : process_find(slot->parent_pid);
        if (slot->parent_pid != 0U && parent != 0 &&
            parent->state != PROCESS_UNUSED &&
            parent->state != PROCESS_ZOMBIE) {
            continue;
        }

        zombie = process_find(slot->pid);
        if (zombie != 0 && zombie->state == PROCESS_ZOMBIE) {
            process_release(zombie);
        }
    }
}
''',
)

replace_once(
    "Makefile",
    "    $(BUILD_DIR)/kernel/process.o \\\n    $(BUILD_DIR)/kernel/sched/sched.o \\\n",
    "    $(BUILD_DIR)/kernel/process.o \\\n    $(BUILD_DIR)/kernel/process_lifecycle.o \\\n    $(BUILD_DIR)/kernel/sched/sched.o \\\n",
)

replace_all_exact(
    "kernel/panel_boot.c",
    "(void)process_reclaim_zombies();",
    "(void)process_reclaim_orphan_zombies();",
    2,
)
replace_once(
    "kernel/panel_boot.c",
    "    process_t *process;\n    user_image_t image;\n",
    "    process_t *process;\n    process_t *parent;\n    user_image_t image;\n",
)
replace_once(
    "kernel/panel_boot.c",
    "    (void)process_reclaim_orphan_zombies();\n    process = process_alloc(g_next_spawn_pid++, app_name);\n",
    "    (void)process_reclaim_orphan_zombies();\n"
    "    parent = process_current();\n"
    "    process = process_alloc_child(g_next_spawn_pid++,\n"
    "                                  parent != 0 ? parent->pid : 0U,\n"
    "                                  app_name);\n",
)
replace_once(
    "kernel/panel_boot.c",
    "    panel = process_alloc(PANEL_BOOT_PID_BASE, PANEL_BOOT_APP);\n",
    "    panel = process_alloc_child(PANEL_BOOT_PID_BASE, 0U, PANEL_BOOT_APP);\n",
)

replace_once(
    "kernel/syscall_internal.h",
    "int64_t sys_wait(uint64_t pid);\n",
    "int64_t sys_wait(process_t *process, uint64_t pid);\n",
)
replace_once(
    "kernel/syscall.c",
    "    case SYS_WAIT:\n        return (uint64_t)sys_wait(x[0]);\n",
    "    case SYS_WAIT:\n        return (uint64_t)sys_wait(current, x[0]);\n",
)
replace_once(
    "kernel/syscall_process.c",
    '''int64_t sys_wait(uint64_t pid) {
    uint64_t exit_code = 0;
    process_t *process;

    if (pid == 0 || pid > UINT32_MAX) {
        return ERR_INVAL;
    }

    process = process_find((uint32_t)pid);
    if (process == 0) {
        return ERR_NOENT;
    }

    if (process->state != PROCESS_ZOMBIE) {
        return ERR_AGAIN;
    }

    if (process_wait_zombie((uint32_t)pid, &exit_code) != 0) {
        return ERR_INVAL;
    }

    return (int64_t)exit_code;
}
''',
    '''int64_t sys_wait(process_t *process, uint64_t pid) {
    uint64_t exit_code = 0;
    process_t *child;

    if (process == 0 || pid == 0 || pid > UINT32_MAX) {
        return ERR_INVAL;
    }

    child = process_find((uint32_t)pid);
    if (child == 0) {
        return ERR_NOENT;
    }
    if (child->parent_pid != process->pid) {
        return ERR_PERM;
    }
    if (child->state != PROCESS_ZOMBIE) {
        return ERR_AGAIN;
    }

    if (process_wait_child_zombie(process->pid, (uint32_t)pid,
                                  &exit_code) != 0) {
        return ERR_INVAL;
    }

    return (int64_t)exit_code;
}
''',
)

write(
    "tests/process_parent_wait_test.c",
    '''#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "kernel/gui.h"
#include "kernel/mm/vmm.h"
#include "kernel/process.h"
#include "kernel/vfs.h"

uint32_t vfs_close_all_for_pid(uint32_t pid) {
    (void)pid;
    return 0;
}

gui_desktop_t *gui_desktop(void) {
    return 0;
}

void gui_destroy_windows_for_pid(gui_desktop_t *desktop, uint32_t pid) {
    (void)desktop;
    (void)pid;
}

void pmm_free_page(uint64_t paddr) {
    (void)paddr;
}

void vmm_free_table(uint64_t *pgd) {
    (void)pgd;
}

static void child_zombie_survives_until_parent_waits(void) {
    process_t *parent;
    process_t *child;
    uint64_t exit_code = 0;

    process_table_init();
    parent = process_alloc_child(1U, 0U, "parent");
    child = process_alloc_child(2U, 1U, "child");
    assert(parent != 0);
    assert(child != 0);
    assert(parent->parent_pid == 0U);
    assert(child->parent_pid == parent->pid);

    process_mark_exited(child, 0x44U);
    process_reclaim_orphan_zombies();
    assert(process_find(2U) == child);
    assert(process_count() == 2U);

    assert(process_wait_child_zombie(3U, 2U, &exit_code) != 0);
    assert(process_find(2U) == child);
    assert(process_wait_child_zombie(1U, 2U, &exit_code) == 0);
    assert(exit_code == 0x44U);
    assert(process_find(2U) == 0);
    assert(process_count() == 1U);

    process_release(parent);
}

static void orphan_zombies_are_reclaimed(void) {
    process_t *parent;
    process_t *child;

    process_table_init();
    parent = process_alloc_child(10U, 0U, "parent");
    child = process_alloc_child(11U, 10U, "child");
    assert(parent != 0);
    assert(child != 0);

    process_mark_exited(child, 2U);
    process_mark_exited(parent, 1U);
    process_reclaim_orphan_zombies();

    assert(process_find(10U) == 0);
    assert(process_find(11U) == 0);
    assert(process_count() == 0U);
}

static void child_allocation_rejects_invalid_parentage(void) {
    process_t *root;

    process_table_init();
    assert(process_alloc_child(4U, 4U, "self") == 0);
    assert(process_alloc_child(5U, 99U, "missing") == 0);

    root = process_alloc_child(1U, 0U, "root");
    assert(root != 0);
    process_mark_exited(root, 0U);
    assert(process_alloc_child(2U, 1U, "zombie-parent") == 0);
    process_reclaim_orphan_zombies();
}

int main(void) {
    child_zombie_survives_until_parent_waits();
    orphan_zombies_are_reclaimed();
    child_allocation_rejects_invalid_parentage();
    puts("process parent/wait lifecycle: ok");
    return 0;
}
''',
)

write(
    "tests/run_process_parent_wait_test.sh",
    '''#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${repo_root}/build-process-parent-wait-test"
binary="${build_dir}/process_parent_wait_test"

rm -rf "${build_dir}"
mkdir -p "${build_dir}"

${HOST_CC:-cc} \
    -std=c11 -Wall -Wextra -Werror -DARMONIOS_TEST \
    -I"${repo_root}" -I"${repo_root}/drivers" \
    "${repo_root}/tests/process_parent_wait_test.c" \
    "${repo_root}/kernel/process.c" \
    "${repo_root}/kernel/process_lifecycle.c" \
    -o "${binary}"

"${binary}"
''',
)
replace_once(
    "tools/verify.sh",
    "run_gate host-tests make -C tests test\n",
    "run_gate host-tests make -C tests test\n"
    "run_gate process-parent-wait bash tests/run_process_parent_wait_test.sh\n",
)

replace_once(
    "docs/ARCHITECTURE.md",
    "- PID and state;\n",
    "- PID, parent PID, and state;\n",
)
replace_once(
    "docs/ARCHITECTURE.md",
    "EL0 dispatch uses `process_dispatch_next()` and a round-robin scan of ready slots.\n",
    "EL0 dispatch uses `process_dispatch_next()` and a round-robin scan of ready slots. "
    "Spawn records the current process as parent. A child zombie remains in the fixed "
    "table until that parent calls `sys_wait`; automatic reclamation is limited to "
    "kernel-owned or orphaned zombies, so later spawns cannot erase an observable exit "
    "status.\n",
)
replace_once(
    "docs/SYSCALLS.md",
    "| 6 | `sys_wait` | `x0=pid` | exit code/error | Non-blocking: succeeds only for an existing zombie process, then reclaims it. |\n",
    "| 6 | `sys_wait` | `x0=pid` | exit code/error | Non-blocking: succeeds only when `pid` is a zombie child of the caller, then returns its exit code and reclaims it. Foreign children return `ERR_PERM`. |\n",
)
replace_once(
    "docs/CURRENT_STATE.md",
    "| EL0 processes | IMPLEMENTED; HOST-VERIFIED | Process table, saved trap frames, per-process page tables, spawn/wait/kill/exit tests, packed argv import | Permission-aware validation and kernel-owned syscall payloads are implemented; fault-recoverable copyin/copyout is still pending. |\n",
    "| EL0 processes | IMPLEMENTED; HOST-VERIFIED | Process table, saved trap frames, per-process page tables, parent-owned zombie/wait regression, spawn/wait/kill/exit tests, packed argv import | Process capacity remains fixed; permission-aware validation is implemented, while fault-recoverable copyin/copyout is still pending. |\n",
)
replace_once(
    "docs/TECHNICAL_RISKS.md",
    "| RISK-015 | P2 hardening | Fault-contained copy | OPEN | User-copy transfers remain ordinary EL1 loads/stores without exception recovery. |\n",
    "| RISK-015 | P2 hardening | Fault-contained copy | OPEN | User-copy transfers remain ordinary EL1 loads/stores without exception recovery. |\n"
    "| RISK-016 | P1 | Process lifecycle | CLOSED | Child zombies remain waitable by their parent; only kernel-owned or orphaned zombies are auto-reclaimed. |\n",
)
replace_once(
    "docs/TECHNICAL_RISKS.md",
    "### RISK-012 — Kernel-owned syscall buffers\n",
    "### RISK-016 — Parent-owned zombie lifecycle\n\n"
    "Spawn records a parent PID. `sys_wait` accepts only a zombie child of the caller, "
    "and later spawns cannot reclaim that child before its exit code is collected. "
    "The automatic reaper is restricted to kernel-owned or orphaned zombies so the "
    "fixed table can recover abandoned slots without violating wait semantics.\n\n"
    "**Evidence:** standalone parent/wait lifecycle regression in "
    "`tests/run_process_parent_wait_test.sh`, plus the normal kernel build and complete "
    "verification matrix.\n\n"
    "### RISK-012 — Kernel-owned syscall buffers\n",
)

print("process lifecycle patch applied")
