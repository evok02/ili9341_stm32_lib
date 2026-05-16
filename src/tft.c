#include "tft.h"

static void hardware_reset(void) {
    gpio_set(GPIOA, GPIO3); // Set RSX pin high 
    system_delay(5);
    gpio_clear(GPIOA, GPIO3); // Set RSX pin low
    system_delay(20);
    gpio_set(GPIOA, GPIO3); // Set RSX pin high 
    system_delay(150);
}

static void set_dcx_pin(bool high) {
    spi_wait();

    if (high) {
        gpio_set(GPIOA, GPIO2); // Set DATA mode
    } else {
        gpio_clear(GPIOA, GPIO2); // Set CMD mode
    }
}

static void command_init(void) {
    temp_command.command = 0;
    temp_command.arguments = 0;
    temp_command.argument_count = 0;
}

static command_t* command_create(uint8_t command, uint8_t* arguments, size_t argument_count) {
    command_init();

    temp_command.command = command;
    temp_command.arguments = arguments;
    temp_command.argument_count = argument_count;
    return &temp_command;
}

static void tft_send_data(uint8_t* buffer, uint32_t length) {
    SET_NSS(LOW);

    set_dcx_pin(HIGH);
    spi_write_data(buffer, length);

    SET_NSS(HIGH);
}

static void tft_send_command(command_t* cmd) {
    SET_NSS(LOW);

    set_dcx_pin(LOW); 
    spi_send_byte_blocking(cmd->command);
    if (cmd->argument_count) {
        set_dcx_pin(HIGH);
        spi_write_data(cmd->arguments, cmd->argument_count);
    }

    SET_NSS(HIGH);
}


void tft_setup(void) {
    SET_NSS(LOW);

    hardware_reset();
    system_delay(10);

    /* Command sequence is taken from 
     * https://vivonomicon.com/2018/06/17/drawing-to-a-small-tft-display-the-ili9341-and-stm32/ */

    //
    // TODO: replace opcodes with command definitions
    
    tft_send_command(command_create(TFT_CMD_SOFTWARE_RESET, 0, 0));
    system_delay(150);

    {
        uint8_t buffer[] = { 0x03, 0x80, 0x02 };
        tft_send_command(command_create(0xEF, buffer, sizeof(buffer)));
    }

    {
        uint8_t buffer[] = { 0x00, 0xC1, 0x30 };
        tft_send_command(command_create(0xCF, buffer, sizeof(buffer)));
    }

    {
        uint8_t buffer[] = { 0x64, 0x03, 0x12, 0x81 };
        tft_send_command(command_create(0xED, buffer, sizeof(buffer)));
    }

    {
        uint8_t buffer[] = { 0x85, 0x00, 0x78 };
        tft_send_command(command_create(0xE8, buffer, sizeof(buffer)));
    }

    {
        uint8_t buffer[] = { 0x39, 0x2C, 0x00, 0x34, 0x02 };
        tft_send_command(command_create(0xCB, buffer, sizeof(buffer)));
    }

    {
        uint8_t buffer[] = { 0x20 };
        tft_send_command(command_create(0xF7, buffer, sizeof(buffer)));
    }

    {
        uint8_t buffer[] = { 0x00, 0x00 };
        tft_send_command(command_create(0xEA, buffer, sizeof(buffer)));
    }

    {
        uint8_t buffer[] = { 0x23 };
        tft_send_command(command_create(0xC0, buffer, sizeof(buffer)));
    }

    {
        uint8_t buffer[] = { 0x10 };
        tft_send_command(command_create(0xC1, buffer, sizeof(buffer)));
    }

    {
        uint8_t buffer[] = { 0x3E, 0x28 };
        tft_send_command(command_create(0xC5, buffer, sizeof(buffer)));
    }

    {
        uint8_t buffer[] = { 0x86 };
        tft_send_command(command_create(0xC7, buffer, sizeof(buffer)));
    }

    {
        uint8_t buffer[] = { 0x48 };
        tft_send_command(command_create(0x36, buffer, sizeof(buffer)));
    }

    {
        uint8_t buffer[] = { 0x00 };
        tft_send_command(command_create(0x37, buffer, sizeof(buffer)));
    }

    {
        uint8_t buffer[] = { 0x55 };
        tft_send_command(command_create(0x3A, buffer, sizeof(buffer)));
    }

    {
        uint8_t buffer[] = { 0x00, 0x18 };
        tft_send_command(command_create(0xB1, buffer, sizeof(buffer)));
    }

    {
        uint8_t buffer[] = { 0x08, 0x82, 0x27 };
        tft_send_command(command_create(0xB6, buffer, sizeof(buffer)));
    }

    {
        uint8_t buffer[] = { 0x00 };
        tft_send_command(command_create(0xF2, buffer, sizeof(buffer)));
    }

    {
        uint8_t buffer[] = { 0x01 };
        tft_send_command(command_create(0x26, buffer, sizeof(buffer)));
    }

    {
        uint8_t buffer[] = { 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E,
                             0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00 };
        tft_send_command(command_create(0xE0, buffer, sizeof(buffer)));
    }

    {
        uint8_t buffer[] = { 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31,
                             0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F};
        tft_send_command(command_create(0xE1, buffer, sizeof(buffer)));
    }


    tft_send_command(command_create(TFT_CMD_SLEEP_OUT, 0, 0));
    system_delay(500);

    tft_send_command(command_create(TFT_CMD_DISPLAY_ON, 0, 0));
    system_delay(500);

    tft_send_command(command_create(0x13, 0, 0));
    system_delay(10);

    tft_send_command(command_create(0, 0, 0));

    SET_NSS(HIGH);
}

// NSS line should be low in the moment of the function call
//
static void tft_set_addr_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {

    if (x0 >= TFT_WIDTH || y0 >= TFT_HEIGHT) return; 

    // Adjust according to the resolution
    x1 = (x1 > (TFT_WIDTH - 1)) ? TFT_WIDTH - 1 : x1;
    y1 = (y1 > (TFT_HEIGHT - 1)) ? TFT_HEIGHT - 1 : y1;

    // COLUMN COUNTER
    {
        uint8_t buffer[] = { WORD_HIGH(x0), WORD_BOTTOM(x0), WORD_HIGH(x1), WORD_BOTTOM(x1) };
        tft_send_command(command_create(TFT_CMD_COL_ADDR_SET, buffer, sizeof(buffer)));
    }

    // PAGE COUNTER
    {
        uint8_t buffer[] = { WORD_HIGH(y0), WORD_BOTTOM(y0),  WORD_HIGH(y1), WORD_BOTTOM(y1) };
        tft_send_command(command_create(TFT_CMD_PAGE_ADDR_SET, buffer, sizeof(buffer)));
    }

}

static void tft_push_color(uint16_t color, uint32_t cnt) {
    tft_send_command(command_create(TFT_CMD_MEMORY_WRITE, 0, 0));
    while(cnt--) {
        uint8_t buffer[] = { WORD_HIGH(color), WORD_BOTTOM(color) }; 
        tft_send_data(buffer, 2);
    }
}

void tft_fill_rectangle(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h, uint16_t color) {
    SET_NSS(LOW);

    tft_set_addr_window(x0, y0, x0 + w, y0 + h);
    tft_push_color(color, w * h);

    SET_NSS(HIGH);
}

void tft_fill_screen(uint16_t color) {
    tft_fill_rectangle(0, 0, TFT_WIDTH, TFT_HEIGHT, color);
}

void tft_fill_pixel(uint16_t x, uint16_t y, uint16_t color) {
    SET_NSS(LOW);

    tft_set_addr_window(x, y, x + 1, y + 1);
    uint8_t buffer[] = { WORD_HIGH(color), WORD_BOTTOM(color) };
    tft_send_command(command_create(TFT_CMD_MEMORY_WRITE, buffer, sizeof(buffer)));

    SET_NSS(HIGH);
}

void tft_write_char(uint8_t ascii, uint8_t row, uint8_t col, uint16_t txt_color, uint16_t bg_color) {
    // TODO: handle this better
    if (ascii < 32) return;

    row = (row < 1) ? 1 : row;
    col = (col < 1) ? 1 : col;

    // Each ACII character represents a rectangle 8 x 5 px
    const uint8_t *ascii_char = ascii_5x8[ascii - 32];

    // TODO: Optimize this loop
    for (uint8_t index_col = 0; index_col < 5; index_col++) {
        for (uint8_t index_row = 0; index_row < 8; index_row++) {
            if (ascii_char[index_col] & (1 << index_row)) {
                tft_fill_pixel(col * 5 + index_col, row * 8 + index_row, txt_color);
            } else {
                tft_fill_pixel(col * 5 + index_col, row * 8 + index_row, bg_color);
            }
        }
    }
}

void tft_write_chars(const uint8_t* ascii, uint8_t cnt, uint8_t row, uint8_t col, uint16_t txt_color, uint16_t bg_color) {
    // TODO: handle this better
    if (cnt > MAX_COL_COUNT) return;

    for (uint8_t index = 0; index < MAX_COL_COUNT; index++) {
        tft_write_char(' ', row, col + index, txt_color, bg_color);
    }

    for (uint8_t index = 0; index < cnt; index++) {
        tft_write_char(ascii[index], row, col + index, txt_color, bg_color);
    }
}
