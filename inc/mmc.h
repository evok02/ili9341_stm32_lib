#ifndef INC_MMC_H
#define INC_MMC_H

#include "spi.h"
#include "tft.h"

#define MMC_BLOCK_SIZE (512)

#define MMC_GO_IDLE_STATE           (0)
#define MMC_SEND_OP_COND            (1)
#define MMC_SEND_IF_COND            (8)
#define MMC_SEND_CSD                (9)
#define MMC_SEND_CID                (10)
#define MMC_STOP_TRANSMISSION       (12)
#define MMC_SET_BLOCKLEN            (16)
#define MMC_READ_SINGLE_BLOCK       (17)
#define MMC_READ_MULTIPLE_BLOCK     (18)
#define MMC_SET_BLOCK_COUNT         (23)
#define MMC_WRITE_SINGLE_BLOCK      (24)
#define MMC_WRITE_MULTIPLE_BLOCK    (25)
#define MMC_APP_SEND_OP_COND        (41)
#define MMC_APP_CMD                 (55)
#define MMC_READ_OCR                (58)

#define SET_MMC_NSS(high) { \
    spi_wait(); \
    (high) ? gpio_set(GPIOA, GPIO1) : gpio_clear(GPIOA, GPIO1); \
}


static inline void _mmc_write_buffer(const uint8_t* buffer, size_t len) {
   spi_write_data(buffer, len); 
}

static inline void _mmc_read_buffer(uint8_t* buffer, size_t len) {
    spi_read_data(buffer, len);
}

static inline void _mmc_wait(void) {
    while (spi_read_write(0xFF) != 0xFF) __asm__("nop");
}

// Exposed functions
int mmc_init(void);
inline uint32_t mcc_get_sector_size(void);

#endif
