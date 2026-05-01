#include <libopencm3/stm32/spi.h>

#include "common.h"
#include "system.h"
#include "spi.h"
#include "tft.h"

#define SQUARE_SIDE_LENGTH (10)
#define SQUARE_AREA (SQUARE_SIDE_LENGTH * SQUARE_SIDE_LENGTH)

int main(void) {
    system_rcc_setup();
    system_gpio_setup();
    system_systick_setup();
    spi_setup();

    tft_setup();

    uint16_t counter = 0;
    uint16_t red_fill[SQUARE_AREA];

    while (counter < SQUARE_AREA) {
        red_fill[counter] = 0xF800;
        counter++;
    }

    tft_fill_rectangle(0, 0, SQUARE_SIDE_LENGTH, SQUARE_SIDE_LENGTH, red_fill);

    for (;;) {
        system_delay(1e3);
        gpio_toggle(GPIOC, GPIO13);
    }
}
