#ifndef INC_MMC_H
#define INC_MMC_H

#include "spi.h"
#include "tft.h"

#define MMC_BLOCK_SIZE (512)

// #define MMC_FS_TYPE_FAT12
// #define MMC_FS_TYPE_FAT16 
#define MMC_FS_TYPE_FAT32 

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

#define _SET_MMC_NSS(high) { \
    spi_wait(); \
    (high) ? gpio_set(GPIOA, GPIO1) : gpio_clear(GPIOA, GPIO1); \
}

static uint16_t _mmc_capacity;

static inline void _mmc_write_buffer(const uint8_t* buffer, size_t len) {
   spi_write_data(buffer, len); 
}

static inline void _mmc_write_buffer_dma(const uint8_t* buffer, size_t len, uint8_t *dummy_rx ) {
    spi_dma_xmit( buffer, len, dummy_rx, len );
    spi_dma_wait();
}

static inline void _mmc_read_buffer(uint8_t* buffer, size_t len) {
    // TODO: CONVERT IT TO BIG ENDIAN
    spi_read_data(buffer, len);
}

static inline void _mmc_read_buffer_dma( uint8_t* buffer, size_t len, uint8_t const *dummy_tx ) {
    spi_dma_xmit( dummy_tx, len, buffer, len );
    spi_dma_wait();
}

static inline void _mmc_wait(void) {
    while (spi_read_write(0xFF) != 0xFF) __asm__("nop");
}

// High level interface
int mmc_init(void);

uint8_t mmc_write_command(uint8_t cmd, uint32_t arg);

int mmc_read_single_block(uint32_t block, size_t length, uint8_t* data);
size_t mmc_read_multiple_blocks(const uint32_t block, size_t count, uint8_t* data);

int mmc_write_single_block(uint32_t block, const uint8_t* data, size_t length);
int mmc_write_multiple_blocks(const uint32_t block, size_t count, uint8_t* data);

#endif
