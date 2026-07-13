# Descriptor isolation design

The syscall ABI continues to expose standard streams as descriptors `0`, `1`,
and `2`; file descriptors begin at `3`.

Internally, `vfs_open_flags()` now allocates:

1. a descriptor number local to the current process in the range `0..7`;
2. a kernel-private slot in the global open-file handle pool.

The syscall layer adds `3` when returning a file descriptor and subtracts `3`
before read, write, seek, and close operations. No syscall number or userland
calling convention changes.

Every VFS descriptor operation resolves the pair `(current PID, local fd)`.
This gives each process its own descriptor namespace and independent offsets
while retaining the existing filesystem implementations.

`vfs_close_all_for_pid()` provides the explicit process-lifecycle cleanup hook.
The VFS also reaps handles whose owner is already missing or zombie before
allocation and lookup, preventing stale handles from exhausting the pool while
the process cleanup integration is reviewed.
