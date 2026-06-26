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

static int _memcpy(void *restrict dest, void *const restrict src, size_t len) {
    if (dest == 0 || src == 0) {
#if defined(DEBUG_INFO_ENABLE)
        printf_("fat32.c:%d | unexpected input: null_pointer dereference\r\n", __LINE__);
#endif
        return -1;
    } 
    for (size_t i = 0; i < len; i++) ((uint8_t *)dest)[i] = ((uint8_t *)src)[i]; 
    return 0;
}

int main(void) {
    static fat_fs_t fs;

    system_rcc_setup();
    system_gpio_setup();
    system_systick_setup();
    pwm_setup();
    spi_setup(SPI_CR1_BAUDRATE_FPCLK_DIV_2);
    uart_setup();
    mmc_init();
    
    fat32_mount(64, &fs);


    fat_file_t *f = fat32_fopen(&fs, "/lowbytes/rocks.jpg", O_OPEN);
}
