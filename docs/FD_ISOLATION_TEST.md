# Process-local VFS descriptor regression

Run the focused descriptor-isolation regression with:

```sh
bash tests/run_vfs_process_fd_test.sh
```

The test compiles the production `kernel/vfs.c` implementation with a minimal
process/FAT harness and verifies:

- two processes can both receive local descriptor `0` (EL0 descriptor `3`);
- their descriptor offsets remain independent;
- closing one process's descriptor does not affect the other process;
- a process cannot close another process's descriptor when it has no matching
  local descriptor;
- handles owned by a zombie process are reclaimed before the next allocation;
- explicit `vfs_close_all_for_pid()` cleanup invalidates all descriptors for a
  process.

This focused test is included in `bash tools/verify.sh`. It does not replace the
full host suite or the visible FAT/editor QEMU workflow.
