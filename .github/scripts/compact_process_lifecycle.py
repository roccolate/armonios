from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text, encoding="utf-8")


def replace_once(path: str, old: str, new: str) -> None:
    text = read(path)
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected one match, got {count}: {old!r}")
    write(path, text.replace(old, new, 1))


def replace_exact(path: str, old: str, new: str, expected: int) -> None:
    text = read(path)
    count = text.count(old)
    if count != expected:
        raise RuntimeError(f"{path}: expected {expected} matches, got {count}: {old!r}")
    write(path, text.replace(old, new))


replace_once(
    "Makefile",
    "    $(BUILD_DIR)/kernel/process.o \\\n    $(BUILD_DIR)/kernel/process_lifecycle.o \\\n    $(BUILD_DIR)/kernel/sched/sched.o \\\n",
    "    $(BUILD_DIR)/kernel/process.o \\\n    $(BUILD_DIR)/kernel/sched/sched.o \\\n",
)

lifecycle = ROOT / "kernel/process_lifecycle.c"
if not lifecycle.exists():
    raise RuntimeError("kernel/process_lifecycle.c missing")
lifecycle.unlink()

replace_once(
    "kernel/process.h",
    "process_t *process_alloc(uint32_t pid, const char *name);\n"
    "process_t *process_alloc_child(uint32_t pid, uint32_t parent_pid,\n"
    "                               const char *name);\n",
    "process_t *process_alloc(uint32_t pid, const char *name);\n",
)
replace_once(
    "kernel/process.h",
    "void process_reclaim_zombies(void);\n"
    "void process_reclaim_orphan_zombies(void);\n"
    "int process_wait_zombie(uint32_t pid, uint64_t *exit_code);\n"
    "int process_wait_child_zombie(uint32_t parent_pid, uint32_t pid,\n"
    "                              uint64_t *exit_code);\n",
    "void process_reclaim_zombies(void);\n"
    "int process_wait_zombie(uint32_t pid, uint64_t *exit_code);\n",
)

replace_once(
    "kernel/process.c",
    '''void process_reclaim_zombies(void) {
    for (uint32_t i = 0; i < PROCESS_MAX_PROCESSES; i++) {
        if (g_processes[i].state == PROCESS_ZOMBIE) {
            /*
             * Note: GUI windows were destroyed by
             * process_mark_exited when the process first became a
             * zombie. process_release here only needs to free
             * the page table and mmap pages.
             */
            process_release(&g_processes[i]);
        }
    }
}
''',
    '''void process_reclaim_zombies(void) {
    for (uint32_t i = 0; i < PROCESS_MAX_PROCESSES; i++) {
        process_t *zombie = &g_processes[i];
        process_t *parent;

        if (zombie->state != PROCESS_ZOMBIE) {
            continue;
        }

        parent = zombie->parent_pid == 0U
                     ? 0
                     : process_find(zombie->parent_pid);
        if (zombie->parent_pid != 0U && parent != 0 &&
            parent->state != PROCESS_UNUSED &&
            parent->state != PROCESS_ZOMBIE) {
            continue;
        }

        process_release(zombie);
    }
}
''',
)

replace_exact(
    "kernel/panel_boot.c",
    "process_reclaim_orphan_zombies()",
    "process_reclaim_zombies()",
    3,
)
replace_once(
    "kernel/panel_boot.c",
    "    process = process_alloc_child(g_next_spawn_pid++,\n"
    "                                  parent != 0 ? parent->pid : 0U,\n"
    "                                  app_name);\n"
    "    if (process == 0) {\n"
    "        return -1;\n"
    "    }\n",
    "    process = process_alloc(g_next_spawn_pid++, app_name);\n"
    "    if (process == 0) {\n"
    "        return -1;\n"
    "    }\n"
    "    process->parent_pid = parent != 0 ? parent->pid : 0U;\n",
)
replace_once(
    "kernel/panel_boot.c",
    "    panel = process_alloc_child(PANEL_BOOT_PID_BASE, 0U, PANEL_BOOT_APP);\n",
    "    panel = process_alloc(PANEL_BOOT_PID_BASE, PANEL_BOOT_APP);\n",
)

replace_once(
    "kernel/syscall_process.c",
    "    if (process_wait_child_zombie(process->pid, (uint32_t)pid,\n"
    "                                  &exit_code) != 0) {\n",
    "    if (process_wait_zombie((uint32_t)pid, &exit_code) != 0) {\n",
)

path = "tests/process_parent_wait_test.c"
text = read(path)
text = text.replace('#include "kernel/process.h"\n', '#include "kernel/process.h"\n#include "kernel/syscall_helpers.h"\n#include "kernel/syscall_internal.h"\n')
text = text.replace('process_alloc_child(', 'test_alloc_child(')
text = text.replace('process_reclaim_orphan_zombies()', 'process_reclaim_zombies()')
text = text.replace('process_wait_child_zombie(3U, 2U, &exit_code)', 'sys_wait(process_find(3U), 2U)')
text = text.replace('process_wait_child_zombie(1U, 2U, &exit_code) == 0', 'sys_wait(parent, 2U) == 0x44')
text = text.replace('    assert(exit_code == 0x44U);\n', '')
insert = '''\nstatic process_t *test_alloc_child(uint32_t pid, uint32_t parent_pid,\n                                   const char *name) {\n    process_t *child = process_alloc(pid, name);\n    if (child != 0) {\n        child->parent_pid = parent_pid;\n    }\n    return child;\n}\n'''
marker = 'void vmm_free_table(uint64_t *pgd) {\n    (void)pgd;\n}\n'
if text.count(marker) != 1:
    raise RuntimeError("test insertion marker missing")
text = text.replace(marker, marker + insert, 1)
# Foreign-parent regression needs an actual caller process.
old = '''    assert(process_wait_child_zombie(3U, 2U, &exit_code) != 0);
    assert(process_find(2U) == child);
'''
# It may already have been replaced above; replace the resulting line instead.
new_old = '''    assert(sys_wait(process_find(3U), 2U) != 0);
    assert(process_find(2U) == child);
'''
new = '''    process_t *foreign = test_alloc_child(3U, 0U, "foreign");
    assert(foreign != 0);
    assert(sys_wait(foreign, 2U) == ERR_PERM);
    assert(process_find(2U) == child);
'''
if text.count(new_old) != 1:
    raise RuntimeError("foreign wait assertion missing")
text = text.replace(new_old, new, 1)
# The invalid-parent allocation assertions belonged to the removed wrapper API.
old_block = '''static void child_allocation_rejects_invalid_parentage(void) {
    process_t *root;

    process_table_init();
    assert(test_alloc_child(4U, 4U, "self") == 0);
    assert(test_alloc_child(5U, 99U, "missing") == 0);

    root = test_alloc_child(1U, 0U, "root");
    assert(root != 0);
    process_mark_exited(root, 0U);
    assert(test_alloc_child(2U, 1U, "zombie-parent") == 0);
    process_reclaim_zombies();
}

'''
if text.count(old_block) != 1:
    raise RuntimeError("obsolete allocation test missing")
text = text.replace(old_block, '', 1)
text = text.replace('    child_allocation_rejects_invalid_parentage();\n', '')
# Remove now-unused local exit_code.
text = text.replace('    uint64_t exit_code = 0;\n', '')
write(path, text)

replace_once(
    "tests/run_process_parent_wait_test.sh",
    '    "${repo_root}/kernel/process.c" \\\n    "${repo_root}/kernel/process_lifecycle.c" \\\n    -o "${binary}"\n',
    '    -ffunction-sections -fdata-sections -Wl,--gc-sections \\\n    "${repo_root}/kernel/process.c" \\\n    "${repo_root}/kernel/syscall_process.c" \\\n    -o "${binary}"\n',
)

print("compacted process lifecycle")
