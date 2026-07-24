#include <stdint.h>

#include <libkarm/errno.h>
#include <libkarm/syscall.h>

#if defined(ARMONIOS_EXTERNAL_KLI_SPAWN_TEST)
static int run_spawn_test(int argc) {
    static const char child_argument[] = "child";
    static const char child_path[] = "/fat/HELLO.KLI";
    long child_argv[1];
    long pid;
    long status;

    if (argc != 0) {
        kli_write_cstr(ARM_FD_STDOUT,
                       "Hello from an external ArmoniOS SDK app\n");
        return 0;
    }

    kli_write_cstr(ARM_FD_STDOUT, "External spawn parent started\n");
    child_argv[0] = (long)(uintptr_t)child_argument;
    pid = kli_spawn_argv(child_path, 0, child_argv, 1);
    if (pid < 0) {
        kli_write_cstr(ARM_FD_STDOUT, "External spawn failed\n");
        return 1;
    }

    for (;;) {
        status = kli_wait(pid);
        if (status == KLI_AGAIN) {
            if (kli_yield() < 0) {
                return 1;
            }
            continue;
        }
        if (status != 0) {
            kli_write_cstr(ARM_FD_STDOUT,
                           "External spawn child failed\n");
            return 1;
        }
        break;
    }

    kli_write_cstr(ARM_FD_STDOUT,
                   "External spawn child exited cleanly\n");
    return 0;
}
#endif

int main(int argc, char **argv) {
    (void)argv;

#if defined(ARMONIOS_EXTERNAL_KLI_SPAWN_TEST)
    return run_spawn_test(argc);
#else
    (void)argc;
    kli_write_cstr(ARM_FD_STDOUT,
                   "Hello from an external ArmoniOS SDK app\n");
    return 0;
#endif
}
