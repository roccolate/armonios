#include "kernel/ipc.h"

#include <stdint.h>

/*
 * Fixed-size in-kernel IPC queue.
 *
 * Messages are copied into kernel-owned slots at send time and removed on the
 * first matching receive. Bytes beyond message.size are kept zero so callers
 * never observe stale payload tails after slot reuse.
 */

typedef struct {
    ipc_message_t message;
    uint64_t sequence;
    uint8_t used;
} ipc_slot_t;

static ipc_slot_t g_ipc_slots[IPC_MAX_MESSAGES];
static uint64_t g_ipc_next_sequence;

static void ipc_copy_payload(uint8_t *dest, const uint8_t *src,
                             uint32_t size) {
    for (uint32_t i = 0; i < size; i++) {
        dest[i] = src[i];
    }
    for (uint32_t i = size; i < IPC_MAX_MESSAGE_SIZE; i++) {
        dest[i] = 0;
    }
}

static void ipc_zero_payload(uint8_t *dest) {
    for (uint32_t i = 0; i < IPC_MAX_MESSAGE_SIZE; i++) {
        dest[i] = 0;
    }
}

static void ipc_clear_slot(ipc_slot_t *slot) {
    if (slot == 0) {
        return;
    }

    slot->message.sender_pid = 0;
    slot->message.target_pid = 0;
    slot->message.size = 0;
    ipc_zero_payload(slot->message.data);
    slot->sequence = 0;
    slot->used = 0;
}

static ipc_slot_t *ipc_find_free_slot(void) {
    for (uint32_t i = 0; i < IPC_MAX_MESSAGES; i++) {
        if (g_ipc_slots[i].used == 0) {
            return &g_ipc_slots[i];
        }
    }

    return 0;
}

static ipc_slot_t *ipc_find_oldest_target_slot(uint32_t target_pid) {
    ipc_slot_t *oldest = 0;

    for (uint32_t i = 0; i < IPC_MAX_MESSAGES; i++) {
        ipc_slot_t *slot = &g_ipc_slots[i];

        if (slot->used == 0 || slot->message.target_pid != target_pid) {
            continue;
        }
        if (oldest == 0 || slot->sequence < oldest->sequence) {
            oldest = slot;
        }
    }

    return oldest;
}

void ipc_init(void) {
    g_ipc_next_sequence = 1;
    for (uint32_t i = 0; i < IPC_MAX_MESSAGES; i++) {
        ipc_clear_slot(&g_ipc_slots[i]);
    }
}

int ipc_send(uint32_t sender_pid, uint32_t target_pid, const uint8_t *data,
             uint32_t size) {
    if (sender_pid == 0 || target_pid == 0 || data == 0 || size == 0 ||
        size > IPC_MAX_MESSAGE_SIZE) {
        return -1;
    }

    ipc_slot_t *slot = ipc_find_free_slot();
    if (slot != 0) {
        slot->message.sender_pid = sender_pid;
        slot->message.target_pid = target_pid;
        slot->message.size = size;
        ipc_copy_payload(slot->message.data, data, size);
        slot->sequence = g_ipc_next_sequence++;
        slot->used = 1;
        return 0;
    }

    return -1;
}

int ipc_recv(uint32_t target_pid, ipc_message_t *message) {
    if (target_pid == 0 || message == 0) {
        return -1;
    }

    ipc_slot_t *slot = ipc_find_oldest_target_slot(target_pid);
    if (slot != 0) {
        message->sender_pid = slot->message.sender_pid;
        message->target_pid = slot->message.target_pid;
        message->size = slot->message.size;
        ipc_copy_payload(message->data, slot->message.data,
                         slot->message.size);
        ipc_clear_slot(slot);
        return 0;
    }

    return -1;
}
