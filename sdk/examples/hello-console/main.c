#include <libkarm/syscall.h>

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    kli_write_cstr(ARM_FD_STDOUT,
                   "Hello from an external ArmoniOS SDK app\n");
    return 0;
}
