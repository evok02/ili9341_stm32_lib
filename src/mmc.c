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

static uint8_t crc7_compute(uint8_t* data, size_t length) {
    uint8_t crc = 0;
    for (int i = 0; i < length; i++) {
        uint8_t d = data[i];
        for (int bit = 0; bit < 8; bit++) {
            crc <<= 1;
            if ((d ^ crc) & 0x80) {
                crc ^= 0x09; // Polynomial x^7 + x^3 + 1
            }
            d <<= 1;
        }
    }
    return (crc << 1) | 1; // Align to bit 7-1 and set end bit
}

/* This is a general function. For single/multiple read/write com * mands use respected functions below... */
static uint8_t mmc_write_command(uint8_t cmd, uint32_t arg) {
    // Waiting if MMC is busy
    _mmc_wait();

    uint8_t buffer[6], r1 = 0xFF;

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
        buffer[5] = 0x01;
    }
    
    _mmc_write_buffer(buffer, sizeof(buffer));

    // Wait for the valid response
    while ((r1 = spi_read_write(0xFF)) & 0x80);

    return r1;
}


static int mcc_write_single_block(uint32_t block, const uint8_t* data, size_t length) {
    _mmc_wait();
    mmc_write_command(MMC_WRITE_SINGLE_BLOCK, block); 

    // Keep the clock running.
    (void)spi_read_write(0xFF);
    (void)spi_read_write(0xFF);

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

inline uint32_t mcc_get_sector_size(void) {
    return MMC_BLOCK_SIZE; 
}

typedef enum {
    MMC_STATE_STARTING,
    MMC_STATE_IS_IDLE,
    MMC_STATE_IF_COND_RES_MATCHED,
    MMC_STATE_IF_COND_RES_NOT_MATCHED,
    MMC_STATE_APP_SEND_OP_COND,
    MMC_STATE_READ_OCR,
    MMC_STATE_SET_BLOCKLEN,
    MMC_STATE_UNKNOWN_CARD,
    MMC_STATE_DONE,
} mmc_init_state;

static mmc_init_state mmc_state = MMC_STATE_IS_IDLE;

// TODO: write state machine which inlcudes full path
int mmc_init(void) {
    // Init sequence is taken from https://elm-chan.org/docs/mmc/mmc_e.html 

    // Setting SPI clock frequency to 100 - 400 kHz
    spi_setup(SPI_CR1_BAUDRATE_FPCLK_DIV_256);

    // POWER SEQUENCE
    system_delay(5);
    for (int i = 10; i > 0; i--) spi_read_write(0xFF);

    spi_setup(SPI_CR1_BAUDRATE_FPCLK_DIV_64);

    // SET CS LINE LOW.
    SET_MMC_NSS(LOW);
    
    if (mmc_write_command(MMC_GO_IDLE_STATE, 0x0) != 0x01) return -1;
    if (mmc_write_command(MMC_SEND_IF_COND, 0x01AA) == 0) {
        uint8_t buffer[4];
        _mmc_read_buffer(buffer, sizeof(buffer));
        if ((*(uint32_t*)buffer & 0xFFFF0000) != 0xAA010000) return -1;
    } else return -1;

    mmc_write_command(MMC_APP_CMD, 0x0);
    if (mmc_write_command(MMC_APP_SEND_OP_COND, 0x40000000) != 0x0) return -1; 
    

    if (mmc_write_command(MMC_READ_OCR, 0x0) == 0) {
        uint8_t buffer[4];
        _mmc_read_buffer(buffer, sizeof(buffer));
        if ((*(uint32_t*)buffer & 0x40000000) >> 30 == 1) return 0;
        if ((*(uint32_t*)buffer & 0x40000000) >> 30 == 0) mmc_write_command(MMC_SET_BLOCKLEN, 0x00000200);
    } else return -1;

    // SET CS LINE HIGH
    SET_MMC_NSS(HIGH);
    return 0;
}
