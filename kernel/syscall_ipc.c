#include "kernel/syscall_internal.h"

#include <stdint.h>

#include "kernel/ipc.h"
#include "kernel/syscall_helpers.h"

int64_t sys_ipc_send(process_t *process, uint64_t target_pid, uint64_t buf,
                     uint64_t len) {
    int64_t status;

    if (process == 0 || target_pid == 0 || len == 0 ||
        target_pid > UINT32_MAX || len > IPC_MAX_MESSAGE_SIZE) {
        return ERR_INVAL;
    }
    status = sys_user_buf_in(process, buf, len);
    if (status != 0) {
        return status;
    }

    if (ipc_send(process->pid, (uint32_t)target_pid,
                 (const uint8_t *)(uintptr_t)buf, (uint32_t)len) != 0) {
        return ERR_AGAIN;
    }

    return (int64_t)len;
}

int64_t sys_ipc_recv(process_t *process, uint64_t buf, uint64_t capacity) {
    ipc_message_t message;
    uint8_t *out = (uint8_t *)(uintptr_t)buf;
    int64_t status;

    if (process == 0 || capacity != IPC_MAX_MESSAGE_SIZE) {
        return ERR_INVAL;
    }
    status = sys_user_buf_out(process, buf, capacity);
    if (status != 0) {
        return status;
    }

    if (ipc_recv(process->pid, &message) != 0) {
        return ERR_AGAIN;
    }

    if (message.size > capacity) {
        return ERR_INVAL;
    }

    for (uint32_t i = 0; i < message.size; i++) {
        out[i] = message.data[i];
    }

    return (int64_t)message.size;
}
