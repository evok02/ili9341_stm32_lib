#include "tft.h"

static void hardware_reset(void) {
    gpio_clear(GPIOA, GPIO3); // Set RSX pin low
    system_delay(10);
    gpio_set(GPIOA, GPIO3); // Set RSX pin high 
}

static void set_dcx_pin(bool high) {
    if (high) {
        gpio_set(GPIOA, GPIO2); // Set DATA mode
    } else {
        gpio_clear(GPIOA, GPIO2); // Set CMD mode
    }
}

static void tft_send_command(uint8_t cmd) {
    set_dcx_pin(LOW); 
    spi_send_byte_blocking(cmd);
}

static void tft_send_data(uint8_t* buffer, uint32_t length) {
    set_dcx_pin(HIGH);
    spi_write_data(buffer, length);
}

void tft_setup(void) {
    hardware_reset();
    system_delay(10);

    SET_NSS(LOW);

    tft_send_command(TFT_CMD_SOFTWARE_RESET);
    system_delay(150);

    // POWER CONTROL 1
    tft_send_command(0xCB);
    {
        uint8_t buffer[] = { 0x39, 0x2C, 0x00, 0x34, 0x02 };
        tft_send_data(buffer, sizeof(buffer));
    }

    // POWER CONTOL 2
    tft_send_command(0xCF);
    {
        uint8_t buffer[] = { 0x00, 0xC1, 0x30 };
        tft_send_data(buffer, sizeof(buffer));
    }

    // DRIVER TIMING CONTROL A
    tft_send_command(0xE8);
    {
        uint8_t buffer[] = { 0x85, 0x00, 0x78 };
        tft_send_data(buffer, sizeof(buffer));
    }

    // DRIVER TIMING CONTROL B
    tft_send_command(0xEA);
    {
        uint8_t buffer[] = { 0x00, 0x00 };
        tft_send_data(buffer, sizeof(buffer));
    }

    // POWER ON SEQUENCE CONTROL
    tft_send_command(0xED);
    {
        uint8_t buffer[] = { 0x64, 0x03, 0x12, 0x81 };
        tft_send_data(buffer, sizeof(buffer));
    }

    // PUMP RATIO CONTROL
    tft_send_command(0xF7);
    {
        uint8_t buffer[] = { 0x20 };
        tft_send_data(buffer, sizeof(buffer));
    }

    // POWER CONTROL,VRH[5:0]
    tft_send_command(0xC0);
    {
        uint8_t buffer[] = { 0x23 };
        tft_send_data(buffer, sizeof(buffer));
    }

    // POWER CONTROL,SAP[2:0];BT[3:0]
    tft_send_command(0xC1);
    {
        uint8_t buffer[] = { 0x10 };
        tft_send_data(buffer, sizeof(buffer));
    }

    // VCM CONTROL
    tft_send_command(0xC5);
    {
        uint8_t buffer[] = { 0x3E, 0x28 };
        tft_send_data(buffer, sizeof(buffer));
    }

    // VCM CONTROL 2
    tft_send_command(0xC7);
    {
        uint8_t buffer[] = { 0x86 };
        tft_send_data(buffer, sizeof(buffer));
    }

    // MEMORY ACCESS CONTROL
    tft_send_command(0x36);
    {
        uint8_t data[] = { 0x48 };
        tft_send_data(data, sizeof(data));
    }

    // PIXEL FORMAT
    tft_send_command(0x3A);
    {
        uint8_t buffer[] = { 0x55 };
        tft_send_data(buffer, sizeof(buffer));
    }

    // FRAME RATIO CONTROL, STANDARD RGB COLOR
    tft_send_command(0xB1);
    {
        uint8_t buffer[] = { 0x00, 0x18 };
        tft_send_data(buffer, sizeof(buffer));
    }

    // DISPLAY FUNCTION CONTROL
    tft_send_command(0xB6);
    {
        uint8_t buffer[] = { 0x08, 0x82, 0x27 };
        tft_send_data(buffer, sizeof(buffer));
    }

    // 3GAMMA FUNCTION DISABLE
    tft_send_command(0xF2);
    {
        uint8_t buffer[] = { 0x00 };
        tft_send_data(buffer, sizeof(buffer));
    }

    // GAMMA CURVE SELECTED
    tft_send_command(0x26);
    {
        uint8_t buffer[] = { 0x01 };
        tft_send_data(buffer, sizeof(buffer));
    }

    // POSITIVE GAMMA CORRECTION
    tft_send_command(0xE0);
    {
        uint8_t buffer[] = {
            0x0F, 0x31, 0x2B, 0x0C,
            0x0E, 0x08, 0x4E, 0xF1,
            0x37, 0x07, 0x10, 0x03,
            0x0E, 0x09, 0x00 
        };
        tft_send_data(buffer, sizeof(buffer));
    }

    // NEGATIVE GAMMA CORRECTION
    tft_send_command(0xE1);
    {
        uint8_t buffer[] = {
            0x00, 0x0E, 0x14, 0x03,
            0x11, 0x07, 0x31, 0xC1,
            0x48, 0x08, 0x0F, 0x0C,
            0x31, 0x36, 0x0F 
        };
        tft_send_data(buffer, sizeof(buffer));
    }

    // EXIT SLEEP
    tft_send_command(0x11);
    system_delay(120);

    // TURN ON DISPLAY
    tft_send_command(0x29);

    // MADCTL
    tft_send_command(0x36);
    {
        uint8_t buffer[] = { 0x40 |  0x08};
        tft_send_data(buffer, sizeof(buffer));
    }

    SET_NSS(HIGH);
}

void tft_set_addr_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {

    SET_NSS(LOW);

    // Adjust accotding to the resolution
    x1 = (x1 > TFT_WIDTH) ? TFT_WIDTH : x1;
    y1 = (y1 > TFT_HEIGHT) ? TFT_HEIGHT : y1;

    tft_send_command(TFT_CMD_COL_ADDR_SET);

    // Sending start column counter
    {
        uint8_t buffer[] = { WORD_HIGH(x0), WORD_BOTTOM(x0) };
        tft_send_data(buffer, sizeof(buffer));
    }

    // Sending end column counter
    {
        uint8_t buffer[] = { WORD_HIGH(x1), WORD_BOTTOM(x1) };
        tft_send_data(buffer, sizeof(buffer));
    }

    tft_send_command(TFT_CMD_PAGE_ADDR_SET);
    // Sending start page counter
    {
        uint8_t buffer[] = { WORD_HIGH(y0), WORD_BOTTOM(y0) };
        tft_send_data(buffer, sizeof(buffer));
    }

    // Sending end page counter
    {
        uint8_t buffer[] = { WORD_HIGH(y1), WORD_BOTTOM(y1) };
        tft_send_data(buffer, sizeof(buffer));
    }

    tft_send_command(TFT_CMD_MEMORY_WRITE);

    SET_NSS(HIGH);
}

void tft_push_color(uint16_t *color, int cnt) {
    //
    // TODO: find better way to pulling low NSS pin
    //
    SET_NSS(LOW);
    for (int index = 0; index < cnt; index++) {
        uint8_t buffer[] = { WORD_HIGH(color[index]), WORD_BOTTOM(color[index]) };
        tft_send_data(buffer, sizeof(buffer));
    }
    SET_NSS(HIGH);
}

