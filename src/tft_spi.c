#include <libopencm3/stm32/spi.h>

#include "common.h"
#include "system.h"
#include "spi.h"
#include "tft.h"
#include "font.h"

#include <string.h>

#define SQUARE_SIDE_LENGTH      (square_side_length)
#define SQUARE_AREA(x)          (x * x)
#define RECTANGLE_AREA(x, y)    (x * y)

#define BLACK     (0x0000U)
#define RED       (0xF800)
#define WHITE     (0xFFFFU)

#define STRING_MAX_LEN (48)

static uint32_t square_side_length;

int main(void) {
    system_rcc_setup();
    system_gpio_setup();
    system_systick_setup();
    spi_setup();
    tft_setup();
    size_t counter = 0;

    // tft_fill_screen(BLACK);
    // char* str = "18:47:22: [!] Buffer overflow";
    // tft_write_chars(str, strlen(str), 1, 1, WHITE, BLACK);

    for (;;) {
        switch (counter % 3) {
            case 0: {
                tft_fill_screen(RED);
            } break;
            case 1: {
                tft_fill_screen(WHITE);
            } break;
            case 2: {
                tft_fill_screen(BLACK);
            } break;
            default: break;
        }
        counter++;
    }
}
