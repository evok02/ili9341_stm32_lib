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

int main(void) {
    static fat_fs_t fs;

    system_rcc_setup();
    system_gpio_setup();
    system_systick_setup();
    pwm_setup();
    spi_setup(SPI_CR1_BAUDRATE_FPCLK_DIV_8);
    uart_setup();
    mmc_init();
    
    fat32_mount(64, &fs);


    uint8_t buf[4096];
    fat_err_e err = FAT_ERR_NONE;
    fat_file_t *f = fat32_fopen( &fs, "/lowbytes/moon.jpg", O_OPEN | O_RDONLY );
    fat32_lseek( 3, f, SEEK_CUR, &fs, &err );
    fat32_lseek( 33, f, SEEK_CUR, &fs, &err );
    fat32_lseek( 10052, f, SEEK_CUR, &fs, &err );
    fat32_lseek( 32768, f, SEEK_CUR, &fs, &err );
    fat32_lseek( 16256, f, SEEK_CUR, &fs, &err );
    BPOINT();
    fat32_lseek( 16512, f, SEEK_SET, &fs, &err );
    size_t buf_off = 0;
    uint32_t start = get_current_counter();
    while ( 1 ) {
        buf_off = fat32_fread( buf, sizeof( buf ), f, &fs, &err );
        if ( err & FAT_ERR_EOF ) {
            printf_( "Image was read successfully! \r\n" );
            break;
        } else if ( err & FAT_ERR_BUF_OVERFLOW ) {
            printf_( "Buffer overflow occured, change the size of the buffer! \r\n " );
        } 
    } 
    uint32_t end = get_current_counter();
    uint32_t time = ( end - start );
#if defined( DEBUG_INFO_ENABLE )
    printf_( "Time to read the file: %d\r\n", time );
#endif

    fat32_fclose( f ); 
    BPOINT();
}
