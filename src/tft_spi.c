#include <libopencm3/stm32/spi.h>

#include "common.h"
#include "system.h"
#include "spi.h"
#include "tft.h"
#include "font.h"
#include "mmc.h"

#include <string.h>

#define SQUARE_SIDE_LENGTH      (square_side_length)
#define SQUARE_AREA(x)          (x * x)
#define RECTANGLE_AREA(x, y)    (x * y)

#define STRING_MAX_LEN (48)

static uint32_t square_side_length;

int main(void) {
    system_rcc_setup();
    system_gpio_setup();
    system_systick_setup();
    spi_setup();
    tft_setup();

    tft_fill_screen(WHITE);

    mmc_init();

    for (;;) {
        gpio_toggle(GPIOC, GPIO4);
        system_delay(1000);
    }
}
