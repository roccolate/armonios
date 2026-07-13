// libkarm/crt0.S
//
// Shared `_start` for every ArmoniOS userland app.
//
// The kernel delivers `x0 = argc` and `x1 = &argv[0]` for
// sys_spawn_argv (8) per the AArch64 procedure-call ABI. Apps spawned
// via plain sys_spawn (without argv) land here with `x0 = 0`,
// `x1 = 0`; crt0 forwards that as `argc = 0`, `argv = NULL` without
// crashing.
//
// _start calls `int main(int argc, char **argv)` and forwards the
// return value to sys_exit (1). The integer return value is
// sign-extended to long so negative exit codes survive.
//
// If `main` ever returns for any reason (it normally does not — the
// app's main either loops or calls kli_exit), the kernel still sees
// a clean SYS_EXIT trap rather than a random instruction fetch.

.section .user.image.text, "ax"

.global _start
.type _start, %function

.extern main

_start:
#ifdef USERCOPY_RX_PROBE
    // Preserve process-entry arguments while the probe issues syscalls.
    mov x19, x0
    mov x20, x1

    // SYS_MEMINFO writes two uint64_t values. `_start` is inside the app's
    // read/execute image mapping, so a safe kernel must reject it with -13.
    adr x0, _start
    mov x8, #101                // SYS_MEMINFO
    svc #0
    mov x9, #-13                // ERR_PERM
    cmp x0, x9
    b.ne .Lusercopy_probe_failed

    mov x0, #1                  // stdout
    adr x1, .Lusercopy_probe_ok
    mov x2, #.Lusercopy_probe_ok_len
    mov x8, #43                 // SYS_WRITE
    svc #0

    mov x0, x19
    mov x1, x20
    b .Lusercopy_probe_done

.Lusercopy_probe_failed:
    mov x0, #1                  // stdout
    adr x1, .Lusercopy_probe_bad
    mov x2, #.Lusercopy_probe_bad_len
    mov x8, #43                 // SYS_WRITE
    svc #0

    mov x0, #2
    mov x8, #1                  // SYS_EXIT
    svc #0

.Lusercopy_probe_halt:
    wfe
    b .Lusercopy_probe_halt

.Lusercopy_probe_done:
#endif
    // x0 = argc, x1 = argv already per AArch64 PCS at process entry.
    bl main
    // main returns int in w0. Sign-extend to long for sys_exit.
    sxtw x0, w0
    mov x8, #1                 // SYS_EXIT
    svc #0

1:
    wfe
    b 1b

#ifdef USERCOPY_RX_PROBE
.Lusercopy_probe_ok:
    .ascii "USERCOPY: RX output rejected\n"
.set .Lusercopy_probe_ok_len, . - .Lusercopy_probe_ok

.Lusercopy_probe_bad:
    .ascii "USERCOPY: unexpected result\n"
.set .Lusercopy_probe_bad_len, . - .Lusercopy_probe_bad
#endif
