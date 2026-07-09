#include <libopencm3/stm32/spi.h>

#include "common.h"
#include "system.h"
#include "spi.h"
#include "tft.h"
#include "font.h"
#include "mmc.h"
#include "uart.h"
#include "fat32.h"

#define SQUARE_SIDE_LENGTH      (square_side_length)
#define SQUARE_AREA(x)          (x * x)
#define RECTANGLE_AREA(x, y)    (x * y)

static uint32_t square_side_length;

static int _fat_memcpy( void *restrict dest, void const *restrict src, size_t len ) {
    if ( dest == 0 || src == 0 ) {
#if defined( DEBUG_INFO_ENABLE )
        printf_( "fat32.c:%d | unexpected input: null_pointer dereference\r\n", __LINE__ );
#endif
        return -1;
    } 
    for ( size_t i = 0; i < len; i++ ) ( ( uint8_t * )dest )[i] = ( ( uint8_t * )src )[i]; 
    return 0;
}

int main(void) {
    system_rcc_setup();
    system_gpio_setup();
    system_systick_setup();
    pwm_setup();
    spi_setup(SPI_CR1_BAUDRATE_FPCLK_DIV_8);
    uart_setup();
    mmc_init();
    
    fat32_mount( 64 );


    fat_err_e err = FAT_ERR_NONE;
    fat_file_t *f_notes = fat32_fopen( "/lowbytes/books/text.txt", O_OPEN | O_WRONLY );
    if ( f_notes == NULL ) {
#if defined ( DEBUG_INFO_ENABLE )
        printf_( "wrong path or couldn't find place to allocate the file\r\n" );
        return -1;
#endif
    }

    
    size_t buf_off = 0;
    uint32_t start = get_current_counter();
    char buffer[512 * 8]; 
    for ( size_t i = 0; i < sizeof( buffer ); i++ ) {
        buffer[i] = 'a';
    }

    int i = 0;
    while ( i++ < 4 ) {
        fat32_lseek( fat32_get_file_size( f_notes ), f_notes, SEEK_SET, &err );
        buf_off = fat32_fwrite( buffer, sizeof( buffer ), f_notes, &err );
        if ( err & FAT_ERR_EOF ) {
            printf_( "Document was read successfully! \r\n" );
            break;
        } else if ( err & FAT_ERR_BUF_OVERFLOW ) {
            printf_( "Buffer overflow occured, change the size of the buffer! \r\n " );
            break;
        } 
    }
    // _fat_memcpy( buffer, " yo, its egg.txt btw \n\n", sizeof( " yo, its egg.txt btw \n\n" ) );
    uint32_t end = get_current_counter();
    uint32_t time = ( end - start );
    BPOINT();
    fat32_fclose( f_notes ); 
}
