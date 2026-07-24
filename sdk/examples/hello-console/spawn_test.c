#include <stdint.h>

#include <libkarm/errno.h>
#include <libkarm/syscall.h>

static int text_equal(const char *left, const char *right) {
    uint32_t i = 0;

    if (left == 0 || right == 0) {
        return left == right;
    }
    while (left[i] != '\0' && right[i] != '\0') {
        if (left[i] != right[i]) {
            return 0;
        }
        i++;
    }
    return left[i] == right[i];
}

int main(int argc, char **argv) {
    static const char child_argument[] = "child";
    static const char child_path[] = "/fat/HELLO.KLI";
    long child_argv[1];
    long pid;
    long status;

    if (argc == 1 && argv != 0 && text_equal(argv[0], child_argument)) {
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
