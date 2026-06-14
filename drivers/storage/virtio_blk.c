#include "storage/virtio_blk.h"

#include <stddef.h>
#include <stdint.h>

#define VIRTIO_MMIO_MAGIC_VALUE 0x000
#define VIRTIO_MMIO_VERSION     0x004
#define VIRTIO_MMIO_DEVICE_ID   0x008
#define VIRTIO_MMIO_VENDOR_ID   0x00c
#define VIRTIO_MMIO_DRIVER_FEATURES 0x020
#define VIRTIO_MMIO_DRIVER_FEATURES_SEL 0x024
#define VIRTIO_MMIO_QUEUE_SEL   0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX 0x034
#define VIRTIO_MMIO_QUEUE_NUM   0x038
#define VIRTIO_MMIO_QUEUE_READY 0x044
#define VIRTIO_MMIO_QUEUE_NOTIFY 0x050
#define VIRTIO_MMIO_STATUS      0x070
#define VIRTIO_MMIO_QUEUE_DESC_LOW 0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH 0x084
#define VIRTIO_MMIO_QUEUE_DRIVER_LOW 0x090
#define VIRTIO_MMIO_QUEUE_DRIVER_HIGH 0x094
#define VIRTIO_MMIO_QUEUE_DEVICE_LOW 0x0a0
#define VIRTIO_MMIO_QUEUE_DEVICE_HIGH 0x0a4
#define VIRTIO_MMIO_CONFIG      0x100

#define VIRTIO_MMIO_MAGIC       0x74726976U
#define VIRTIO_DEVICE_ID_BLOCK  2U
#define VIRTIO_STATUS_ACKNOWLEDGE 1U
#define VIRTIO_STATUS_DRIVER    2U
#define VIRTIO_STATUS_DRIVER_OK 4U
#define VIRTIO_STATUS_FEATURES_OK 8U
#define VIRTIO_STATUS_FAILED    128U
#define VIRTQ_DESC_F_NEXT       1U
#define VIRTQ_DESC_F_WRITE      2U
#define VIRTIO_BLK_T_IN         0U
#define VIRTIO_BLK_S_OK         0U
#define VIRTIO_BLK_QUEUE_SIZE   8U
#define VIRTIO_BLK_SECTOR_SIZE  512U
#define VIRTIO_BLK_POLL_LIMIT   10000000U
#define VIRTIO_F_VERSION_1_HIGH 1U

typedef struct {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} virtq_desc_t;

typedef struct {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[VIRTIO_BLK_QUEUE_SIZE];
} virtq_avail_t;

typedef struct {
    uint32_t id;
    uint32_t len;
} virtq_used_elem_t;

typedef struct {
    uint16_t flags;
    uint16_t idx;
    virtq_used_elem_t ring[VIRTIO_BLK_QUEUE_SIZE];
} virtq_used_t;

typedef struct {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
} virtio_blk_req_t;

static virtq_desc_t g_desc[VIRTIO_BLK_QUEUE_SIZE] __attribute__((aligned(16)));
static virtq_avail_t g_avail __attribute__((aligned(2)));
static virtq_used_t g_used __attribute__((aligned(4)));
static virtio_blk_req_t g_request __attribute__((aligned(8)));
static uint8_t g_status __attribute__((aligned(1)));

static volatile uint32_t *virtio_reg(uint64_t base, uint32_t offset) {
    return (volatile uint32_t *)(uintptr_t)(base + offset);
}

static void virtio_write64(uint64_t base, uint32_t low_offset, uint64_t value) {
    *virtio_reg(base, low_offset) = (uint32_t)value;
    *virtio_reg(base, low_offset + 4U) = (uint32_t)(value >> 32);
}

static void virtio_barrier(void) {
#if defined(__aarch64__)
    __asm__ volatile("dmb sy" ::: "memory");
#else
    __sync_synchronize();
#endif
}

int virtio_blk_probe(uint64_t base, virtio_blk_info_t *info) {
    uint32_t magic;
    uint32_t version;
    uint32_t device_id;

    if (base == 0 || info == NULL) {
        return -1;
    }

    magic = *virtio_reg(base, VIRTIO_MMIO_MAGIC_VALUE);
    version = *virtio_reg(base, VIRTIO_MMIO_VERSION);
    device_id = *virtio_reg(base, VIRTIO_MMIO_DEVICE_ID);

    if (magic != VIRTIO_MMIO_MAGIC || device_id != VIRTIO_DEVICE_ID_BLOCK) {
        return -1;
    }

    if (version != 1U && version != 2U) {
        return -1;
    }

    info->version = version;
    info->vendor_id = *virtio_reg(base, VIRTIO_MMIO_VENDOR_ID);
    info->capacity_sectors =
        (uint64_t)(*virtio_reg(base, VIRTIO_MMIO_CONFIG + 4U)) << 32 |
        *virtio_reg(base, VIRTIO_MMIO_CONFIG);

    return 0;
}

int virtio_blk_probe_range(uint64_t base, uint64_t size, uint64_t stride,
                           uint64_t *found_base, virtio_blk_info_t *info) {
    if (base == 0 || size == 0 || stride == 0 || found_base == NULL ||
        info == NULL) {
        return -1;
    }

    for (uint64_t offset = 0; offset < size; offset += stride) {
        uint64_t candidate = base + offset;

        if (virtio_blk_probe(candidate, info) == 0) {
            *found_base = candidate;
            return 0;
        }
    }

    return -1;
}

int virtio_blk_init(virtio_blk_device_t *device, uint64_t base) {
    virtio_blk_info_t info;
    uint32_t queue_max;
    uint32_t status;

    if (device == NULL || virtio_blk_probe(base, &info) != 0 ||
        info.version != 2U) {
        return -1;
    }

    *virtio_reg(base, VIRTIO_MMIO_STATUS) = 0;
    *virtio_reg(base, VIRTIO_MMIO_STATUS) = VIRTIO_STATUS_ACKNOWLEDGE;
    *virtio_reg(base, VIRTIO_MMIO_STATUS) =
        VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;

    *virtio_reg(base, VIRTIO_MMIO_DRIVER_FEATURES_SEL) = 0;
    *virtio_reg(base, VIRTIO_MMIO_DRIVER_FEATURES) = 0;
    *virtio_reg(base, VIRTIO_MMIO_DRIVER_FEATURES_SEL) = 1;
    *virtio_reg(base, VIRTIO_MMIO_DRIVER_FEATURES) = VIRTIO_F_VERSION_1_HIGH;
    status = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER |
             VIRTIO_STATUS_FEATURES_OK;
    *virtio_reg(base, VIRTIO_MMIO_STATUS) = status;

    if ((*virtio_reg(base, VIRTIO_MMIO_STATUS) & VIRTIO_STATUS_FEATURES_OK) == 0) {
        *virtio_reg(base, VIRTIO_MMIO_STATUS) = VIRTIO_STATUS_FAILED;
        return -1;
    }

    *virtio_reg(base, VIRTIO_MMIO_QUEUE_SEL) = 0;
    queue_max = *virtio_reg(base, VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (queue_max < 3U) {
        *virtio_reg(base, VIRTIO_MMIO_STATUS) = VIRTIO_STATUS_FAILED;
        return -1;
    }

    for (uint32_t i = 0; i < VIRTIO_BLK_QUEUE_SIZE; i++) {
        g_desc[i].addr = 0;
        g_desc[i].len = 0;
        g_desc[i].flags = 0;
        g_desc[i].next = 0;
        g_avail.ring[i] = 0;
        g_used.ring[i].id = 0;
        g_used.ring[i].len = 0;
    }
    g_avail.flags = 0;
    g_avail.idx = 0;
    g_used.flags = 0;
    g_used.idx = 0;
    g_status = 0xffU;

    *virtio_reg(base, VIRTIO_MMIO_QUEUE_NUM) = VIRTIO_BLK_QUEUE_SIZE;
    virtio_write64(base, VIRTIO_MMIO_QUEUE_DESC_LOW,
                   (uint64_t)(uintptr_t)g_desc);
    virtio_write64(base, VIRTIO_MMIO_QUEUE_DRIVER_LOW,
                   (uint64_t)(uintptr_t)&g_avail);
    virtio_write64(base, VIRTIO_MMIO_QUEUE_DEVICE_LOW,
                   (uint64_t)(uintptr_t)&g_used);
    *virtio_reg(base, VIRTIO_MMIO_QUEUE_READY) = 1;
    virtio_barrier();

    status = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER |
             VIRTIO_STATUS_FEATURES_OK | VIRTIO_STATUS_DRIVER_OK;
    *virtio_reg(base, VIRTIO_MMIO_STATUS) = status;

    device->base = base;
    device->queue_size = VIRTIO_BLK_QUEUE_SIZE;
    device->last_used_idx = 0;
    device->ready = 1;
    return 0;
}

int virtio_blk_read_sector(virtio_blk_device_t *device, uint64_t sector,
                           void *buffer) {
    uint16_t avail_idx;

    if (device == NULL || device->ready == 0 || buffer == NULL) {
        return -1;
    }

    g_request.type = VIRTIO_BLK_T_IN;
    g_request.reserved = 0;
    g_request.sector = sector;
    g_status = 0xffU;

    g_desc[0].addr = (uint64_t)(uintptr_t)&g_request;
    g_desc[0].len = sizeof(g_request);
    g_desc[0].flags = VIRTQ_DESC_F_NEXT;
    g_desc[0].next = 1;

    g_desc[1].addr = (uint64_t)(uintptr_t)buffer;
    g_desc[1].len = VIRTIO_BLK_SECTOR_SIZE;
    g_desc[1].flags = VIRTQ_DESC_F_NEXT | VIRTQ_DESC_F_WRITE;
    g_desc[1].next = 2;

    g_desc[2].addr = (uint64_t)(uintptr_t)&g_status;
    g_desc[2].len = sizeof(g_status);
    g_desc[2].flags = VIRTQ_DESC_F_WRITE;
    g_desc[2].next = 0;

    avail_idx = g_avail.idx;
    g_avail.ring[avail_idx % device->queue_size] = 0;
    virtio_barrier();
    g_avail.idx = avail_idx + 1U;
    virtio_barrier();

    *virtio_reg(device->base, VIRTIO_MMIO_QUEUE_NOTIFY) = 0;

    for (uint32_t spins = 0; spins < VIRTIO_BLK_POLL_LIMIT; spins++) {
        virtio_barrier();
        if (g_used.idx != device->last_used_idx) {
            device->last_used_idx = g_used.idx;
            return g_status == VIRTIO_BLK_S_OK ? 0 : -3;
        }
    }

    return -2;
}
