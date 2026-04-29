#include <libopencm3/stm32/spi.h>

#include "common.h"
#include "system.h"
#include "spi.h"
#include "tft.h"

#define SQUARE_SIDE_LENGTH (200)

int main(void) {
    system_rcc_setup();
    system_gpio_setup();
    system_systick_setup();
    spi_setup();

    tft_setup();

    uint16_t counter = 0;
    uint16_t red_fill[SQUARE_SIDE_LENGTH];

    while (counter < SQUARE_SIDE_LENGTH) {
        red_fill[counter] = 0XF800;
        counter++;
    }

    tft_set_addr_window(0, SQUARE_SIDE_LENGTH,
                        0, SQUARE_SIDE_LENGTH);
    tft_push_color(red_fill, SQUARE_SIDE_LENGTH);

    for (;;) {
        system_delay(1000);
        gpio_toggle(GPIOC, GPIO13);
    }
}
