#include "mmc.h"
#include "libopencm3/stm32/common/spi_common_v1.h"

/* MCC data tokens:
 * https://elm-chan.org/docs/mmc/mmc_e.html */

#define MMC_TOKEN_SINGLE_READ       ( 0xFEU )
#define MMC_TOKEN_MULTIPLE_READ     ( 0xFEU )
#define MMC_TOKEN_SINGLE_WRITE      ( 0xFEU )
#define MMC_TOKEN_MULTIPLE_WRITE    ( 0xFBU )
#define MMC_TOKEN_STOP_TRAN         ( 0xFCU )

/* MCC responses to the common commands:
 * https://elm-chan.org/docs/mmc/mmc_e.html */

#define MMC_R1_OK                   ( 0 )
#define MMC_R1_IDLE_STATE           ( 0x1 )
#define MMC_R1_ERASE_RESET          ( 0x1 << 1 )
#define MMC_R1_ILLIGAL_COMMAND      ( 0x1 << 2 )
#define MMC_R1_COMMAND_CRC_ERR      ( 0x1 << 3 )
#define MMC_R1_ERASE_SEQ_ERR        ( 0x1 << 4 )
#define MMC_R1_ADDRESS_ERROR        ( 0x1 << 5 )
#define MMC_R1_PARAMETER_ERROR      ( 0x1 << 6 )

static uint8_t dummy_tx[MMC_BLOCK_SIZE] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
};

static uint8_t dummy_rx[MMC_BLOCK_SIZE] = { 0 };

typedef enum {
    ERR_TOKEN_NONE,
    ERR_TOKEN_ERROR,
    ERR_TOKEN_CC_ERROR,
    ERR_TOKEN_CARD_ECC_FAILED,
    ERR_TOKEN_OUT_OF_RANGE,
    CARD_IS_LOCKED,
} mmc_err_token_t;

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


static uint8_t mmc_crc7( const uint8_t *data, size_t length ) {                                           
    uint8_t crc = 0;
    for ( size_t i = 0; i < length; i++ ) {
        for ( uint8_t bit = 0; bit < 8; bit++ ) {
            uint8_t data_bit = ( data[i] >> ( 7 - bit ) ) & 0x01;
            if ( ( ( crc >> 6 ) ^ data_bit ) & 0x01 ) {
                crc = ( ( crc << 1 ) ^ 0x09 ) & 0x7F;
            } else {
                crc = ( crc << 1 ) & 0x7F;
            }
        }
    }
    return ( crc << 1 ) | 0x01;
}

/* This is a general function. For single/multiple read/write com * mands use respected functions below... */
uint8_t mmc_write_command( uint8_t cmd, uint32_t arg ) {
    // Waiting if MMC is busy
    _mmc_wait(  );

    uint8_t buffer[5], r1 = 0xFF;

    buffer[0] = cmd | 0x40;
    // Convert to little endian
    buffer[1] = ( arg >> 24 ) & 0xFF;
    buffer[2] = ( arg >> 16 ) & 0xFF;
    buffer[3] = ( arg >> 8 ) & 0xFF;
    buffer[4] = arg & 0xFF;

    _mmc_write_buffer( buffer, sizeof( buffer ) );
    spi_read_write( mmc_crc7( buffer, sizeof( buffer ) ) );

    // Wait for the valid response
    while ( ( r1 = spi_read_write( 0xFF ) ) & 0x80 );

    return r1;
}


int mmc_write_single_block( uint32_t block, const uint8_t *data, size_t length ) {
     _SET_MMC_NSS( LOW );
    if ( length > 2048 || length == 0 ) return -1;
    _mmc_wait( );
    uint8_t r = mmc_write_command( MMC_WRITE_SINGLE_BLOCK, block );
    _mmc_wait( );
    spi_read_write( MMC_TOKEN_SINGLE_WRITE );  
    _mmc_write_buffer_dma( data, length, dummy_rx );
    spi_read_write( 0xFF );
    spi_read_write( 0xFF );
    uint8_t res = spi_read_write( 0xFF );
    _mmc_wait( );
    _SET_MMC_NSS( HIGH );
    return res;
}

size_t mmc_write_multiple_blocks( uint32_t block, size_t count, const uint8_t *data ) {
    size_t counter = 0;
    uint8_t r = 0;
    _mmc_wait( );

    _SET_MMC_NSS( LOW );
    mmc_write_command( MMC_SET_BLOCK_COUNT, count );
    mmc_write_command( MMC_WRITE_MULTIPLE_BLOCK, block );

    while ( counter < count ) {
        _mmc_wait( );
        spi_read_write( MMC_TOKEN_MULTIPLE_WRITE );
        _mmc_write_buffer_dma( data, MMC_BLOCK_SIZE, dummy_rx );
        ( void )spi_read_write( 0xFF );
        ( void )spi_read_write( 0xFF );
        r = spi_read_write( 0xFF );
        _mmc_wait( );
        counter++;
        data += MMC_BLOCK_SIZE;
    }

    mmc_write_command( MMC_STOP_TRANSMISSION, 0 );
    _mmc_wait(  );
    _SET_MMC_NSS( HIGH );

    return counter;
}

int mmc_read_single_block( uint32_t block, size_t length, uint8_t *data ) {

    _mmc_wait(  );
    spi_read_write( 0xFF );
    spi_read_write( 0xFF );
    spi_read_write( 0xFF );
    _SET_MMC_NSS( LOW );
    uint8_t r = mmc_write_command( MMC_READ_SINGLE_BLOCK, block );

    // Keep the clock running.
    r = 0xFF;
    _mmc_wait(  );
    uint32_t c = get_current_counter( );
    while ( ( r = spi_read_write( 0xFF ) ) != MMC_TOKEN_SINGLE_READ ) {
        if ( get_current_counter( ) > c + 10 ) return -1;
    } 
    _mmc_read_buffer_dma( data , length, dummy_tx );

    // Skipping CRC.
    ( void )spi_read_write( 0xFF ); 
    ( void )spi_read_write( 0xFF ); 
    _SET_MMC_NSS( HIGH );
    return 0;
}

size_t mmc_read_multiple_blocks( const uint32_t block, size_t count, uint8_t *data ) {
    size_t counter = 0;
    uint8_t r = 0;
    _mmc_wait( );
    spi_read_write( 0xFF );
    spi_read_write( 0xFF );
    spi_read_write( 0xFF );

    _SET_MMC_NSS( LOW );
    mmc_write_command( MMC_SET_BLOCK_COUNT, count );
    mmc_write_command( MMC_READ_MULTIPLE_BLOCK, block );

    while ( counter != count ) {
        _mmc_wait(  );
        uint32_t c = get_current_counter( );
        while ( ( r = spi_read_write( 0xFF ) ) != MMC_TOKEN_SINGLE_READ ) {
            if ( get_current_counter( ) == c + 10 ) return -1;
        } 
        _mmc_read_buffer_dma( data, MMC_BLOCK_SIZE, dummy_tx );
        ( void )spi_read_write( 0xFF );
        ( void )spi_read_write( 0xFF );
        counter++;
        data += MMC_BLOCK_SIZE;
    }

    mmc_write_command( MMC_STOP_TRANSMISSION, 0 );
    _mmc_wait(  );
    _SET_MMC_NSS( HIGH );

    return counter;
}

inline uint32_t mcc_get_sector_size( void ) {
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

static mmc_init_state mmc_state;

// TODO: WRITE STATE MACHINE WHICH INLCUDES FULL PATH
int mmc_init( void ) {
    // Init sequence is taken from https://elm-chan.org/docs/mmc/mmc_e.html 

    // SETTING SPI CLOCK FREQUENCY TO 100 - 400 KHZ
    //
    uint8_t cmd_resp = 0;
    spi_setup( SPI_CR1_BAUDRATE_FPCLK_DIV_256 );

    // POWER SEQUENCE
    system_delay( 5 );
    for ( int i = 10; i > 0; i-- ) spi_read_write( 0xFF );

    spi_setup( SPI_CR1_BAUDRATE_FPCLK_DIV_8 );

    // SET CS LINE LOW.
    _SET_MMC_NSS( LOW );
    
    if ( ( cmd_resp = mmc_write_command( MMC_GO_IDLE_STATE, 0x0 ) ) != MMC_R1_IDLE_STATE ) {
        if ( cmd_resp == 0x03 ) {
#ifdef DEBUG_INFO_ENABLE
            printf_( "MMC already initialized\r\n" );
#endif 
            return 0; 
        }
        else {
            _SET_MMC_NSS( HIGH );
#ifdef DEBUG_INFO_ENABLE
            printf_( "Init procces was not successfull: final command MMC_GO_IDLE_STATE\r\n" );
#endif 
            return -1;
        }
    }

    // TODO: REPLACE GOTO WITH STATE MACHINE
    while (  1  ) {
        uint8_t cmd_res;
        if ( mmc_write_command( MMC_SEND_IF_COND, 0x01AA ) == MMC_R1_OK ) {
            uint8_t buffer[4];
            _mmc_read_buffer( buffer, sizeof( buffer ) );
            if ( ( *( uint32_t * )buffer & 0xFFFF0000 ) != 0xAA010000 ) {
                _SET_MMC_NSS( HIGH );
#ifdef DEBUG_INFO_ENABLE
                printf_( "Init procces was not successfull: final command MMC_SEND_IF_COND: != 0xAA010000\r\n" );
#endif 
                return -1;
            } 
        } else {
            _SET_MMC_NSS( HIGH );
#ifdef DEBUG_INFO_ENABLE
            printf_( "Init procces was not successfull: final command MCC_SEND_IF_COND\r\n" );
#endif 
            return -1;
        } 

ACMD_SEND:
        mmc_write_command( MMC_APP_CMD, 0x0 );

        if ( ( cmd_res = mmc_write_command( MMC_APP_SEND_OP_COND, 0x40000000 ) ) == MMC_R1_OK ) { 
            goto ACMD_LOOP_BREAK;
        } else if ( cmd_res == 0x01 ) {
            goto ACMD_SEND;
        } else continue;
    }
ACMD_LOOP_BREAK:
    

    if ( mmc_write_command( MMC_READ_OCR, 0x0 ) == MMC_R1_OK ) {
        uint8_t buffer[4];
        _mmc_read_buffer( buffer, sizeof( buffer ) );
        // Checking CCS bit in OCR
        if ( !( buffer[0] & 0x40 ) ) {
            // Force block size to 512 bytes to work with FAT file system
            mmc_write_command( MMC_SET_BLOCKLEN, 0x00000200 );
        }
    } else {
#ifdef DEBUG_INFO_ENABLE
        printf_( "Init procces was not successfull: final command MMC_READ_OCR\r\n" );
#endif 
        _SET_MMC_NSS( HIGH );
        return -1;
    }
    mmc_write_command( MMC_CRC_ON_OFF, 0 );

    // SET CS LINE HIGH
    _SET_MMC_NSS( HIGH );
    system_delay( 10 );
#ifdef DEBUG_INFO_ENABLE
        printf_( "MMC was initialized successfuly with MMC_R1_OK\r\n" );
#endif 
    spi_setup(  SPI_CR1_BAUDRATE_FPCLK_DIV_4 );
    return 0;
}
