#include "mmc.h"

/* MCC data tokens:
 * https://elm-chan.org/docs/mmc/mmc_e.html */

#define MMC_TOKEN_SINGLE_READ       (0xFEU)
#define MMC_TOKEN_MULTIPLE_READ     (0xFEU)
#define MMC_TOKEN_SINGLE_WRITE      (0xFEU)
#define MMC_TOKEN_MULTIPLE_WRITE    (0xFBU)
#define MMC_TOKEN_STOP_TRAN         (0xFCU)

/* MCC responses to the common commands:
 * https://elm-chan.org/docs/mmc/mmc_e.html */

#define MMC_R1_OK                   (0)
#define MMC_R1_IDLE_STATE           (0x1)
#define MMC_R1_ERASE_RESET          (0x1 << 1)
#define MMC_R1_ILLIGAL_COMMAND      (0x1 << 2)
#define MMC_R1_COMMAND_CRC_ERR      (0x1 << 3)
#define MMC_R1_ERASE_SEQ_ERR        (0x1 << 4)
#define MMC_R1_ADDRESS_ERROR        (0x1 << 5)
#define MMC_R1_PARAMETER_ERROR      (0x1 << 6)

typedef enum {
    ERR_TOKEN_NONE,
    ERR_TOKEN_ERROR,
    ERR_TOKEN_CC_ERROR,
    ERR_TOKEN_CARD_ECC_FAILED,
    ERR_TOKEN_OUT_OF_RANGE,
    CARD_IS_LOCKED,
} err_token_t;


/* This is a general function. For single/multiple read/write com * mands use respected functions below... */

static err_token_t mmc_write_command(uint8_t cmd, uint32_t arg) {
    uint8_t buffer[6], r1;

    buffer[0] = cmd | 0x40;
    // Convert to little endian
    buffer[1] = (arg >> 24) & 0xFF;
    buffer[2] = (arg >> 16) & 0xFF;
    buffer[3] = (arg >> 8) & 0xFF;
    buffer[4] = arg & 0xFF;

    // Hard-coded CRC
    if (cmd == MMC_GO_IDLE_STATE) {
        buffer[5] = 0x95;
    } else if (cmd == MMC_SEND_IF_COND) {
        buffer[5] = 0x87;
    } else {
        buffer[5] = 0;
    }
    
    _mmc_write_buffer(buffer, sizeof(buffer));

    // Wait for the valid response
    while ((r1 = spi_read_write(0xFF) & 0x1F)) __asm__("nop");

    return r1;
}


static int mcc_write_single_block(uint32_t block, const uint8_t* data, size_t length) {
    _mmc_wait();
    mmc_write_command(MMC_WRITE_SINGLE_BLOCK, block); 

    // Keep the clock running.
    spi_read_write(0xFF);
    spi_read_write(0xFF);

    spi_send_byte_blocking(MMC_TOKEN_SINGLE_WRITE);
    _mmc_write_buffer(data, length);

    return spi_read_write(0xFF);
}

static int mcc_write_multiple_blocks(uint32_t block, size_t count, uint8_t* data) {
    size_t counter = 0;

    _mmc_wait();
    mmc_write_command(MMC_SET_BLOCK_COUNT, count);
    mmc_write_command(MMC_WRITE_MULTIPLE_BLOCK, block);

    // Keep the clock running.
    (void)spi_read_write(0xFF);
    (void)spi_read_write(0xFF);

    while (counter < count) {
        _mmc_write_buffer(data, MMC_BLOCK_SIZE);
        if (spi_read_write(0xFF) != MMC_TOKEN_MULTIPLE_WRITE) return counter;
        _mmc_wait();
        data += MMC_BLOCK_SIZE;
        counter++;
    }

    return counter;
}

static int mcc_read_single_block(uint32_t block, size_t length, uint8_t* data) {
    _mmc_wait();
    mmc_write_command(MMC_READ_SINGLE_BLOCK, block);

    // Keep the clock running.
    _mmc_wait();
    (void)spi_read_write(0xFF);

    if(spi_read_write(0xFF) != MMC_TOKEN_SINGLE_READ) return -1; 
    _mmc_read_buffer(data, length);

    // Skipping CRC.
    (void)spi_read_byte(); 
    (void)spi_read_byte(); 
    
    return 0;
}

static int mcc_read_multiple_blocks(const uint32_t block, size_t count, uint8_t* data) {
    size_t counter = 0;

    _mmc_wait();
    mmc_write_command(MMC_SET_BLOCK_COUNT, count);
    mmc_write_command(MMC_READ_MULTIPLE_BLOCK, block);

    while (counter != count) {
        if (spi_read_byte() != MMC_TOKEN_MULTIPLE_READ) return counter;
        _mmc_read_buffer(data, MMC_BLOCK_SIZE);
        // Skipping CRC.
        (void)spi_read_byte();
        (void)spi_read_byte();
        counter++;
    }

    return counter;
}

inline uint32_t mcc_get_block_size(void) {
    return MMC_BLOCK_SIZE; 
}

int mmc_init(void) {
    // Init sequence is taken from https://elm-chan.org/docs/mmc/mmc_e.html 

    // POWER SEQUENCE
    system_delay(5);
    SET_MMC_NSS(HIGH);
    for (int i = 80; i > 0; i--) spi_read_write(0xFF);

    if(mmc_write_command(MMC_GO_IDLE_STATE, 0) != ERR_TOKEN_NONE) {
        const char* ouput = "Unknown card";
        tft_write_chars(ouput, strlen(ouput), 1, 1, BLACK, WHITE);
        return -1;
    }

    if(mmc_write_command(MMC_SEND_IF_COND, 0) != ERR_TOKEN_NONE) {
        const char* ouput = "Unknown card";
        tft_write_chars(ouput, strlen(ouput), 1, 1, BLACK, WHITE);
        return -1;
    }

    if((mmc_write_command(MMC_READ_OCR, 0) & 0x1) == 0) {
        const char* ouput = "SD Ver.2+ (Byte address)";
        tft_write_chars(ouput, strlen(ouput), 1, 1, BLACK, WHITE);
        mmc_write_command(MMC_SET_BLOCKLEN, 512);
        return 0;
    }

    const char* ouput = "SD Ver.2* (Block address)";
    SET_MMC_NSS(LOW);
    return 0;
}
