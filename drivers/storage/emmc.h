#ifndef ARMONIOS_DRIVERS_STORAGE_EMMC_H
#define ARMONIOS_DRIVERS_STORAGE_EMMC_H

#include <stdint.h>

#define EMMC_BLKSZ                  512U
#define EMMC_IDENT_CLOCK_HZ         400000U
#define EMMC_TRANSFER_CLOCK_HZ      25000000U

#define EMMC_OK                     0
#define EMMC_ERR_INVAL             -1
#define EMMC_ERR_TIMEOUT           -2
#define EMMC_ERR_COMMAND           -3
#define EMMC_ERR_DATA              -4
#define EMMC_ERR_UNSUPPORTED       -5
#define EMMC_ERR_READ_ONLY         -6

typedef uint32_t (*emmc_read32_fn_t)(void *context, uint32_t offset);
typedef void (*emmc_write32_fn_t)(void *context, uint32_t offset,
                                  uint32_t value);
typedef void (*emmc_delay_us_fn_t)(void *context, uint32_t usec);

typedef struct {
    emmc_read32_fn_t read32;
    emmc_write32_fn_t write32;
    emmc_delay_us_fn_t delay_us;
    void *context;
} emmc_io_t;

typedef struct {
    emmc_io_t io;
    uint64_t base;
    uint32_t base_clock_hz;
    uint32_t actual_clock_hz;
    uint32_t rca;
    uint32_t ocr;
    uint32_t cid[4];
    uint32_t csd[4];
    uint8_t ready;
    uint8_t high_capacity;
    uint8_t read_only;
} emmc_device_t;

int emmc_init(emmc_device_t *dev, uint64_t base, uint32_t base_clock_hz);
int emmc_init_with_io(emmc_device_t *dev, const emmc_io_t *io,
                      uint32_t base_clock_hz);
int emmc_read_sector(emmc_device_t *dev, uint32_t lba, uint32_t count,
                     void *buffer);
int emmc_write_sector(emmc_device_t *dev, uint32_t lba, uint32_t count,
                      const void *buffer);

#endif
