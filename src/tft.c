#include "tft.h"

static void hardware_reset(void) {
    system_delay(10);
}

static void tft_send_command(uint16_t cmd) {
    cmd |= (0x0 << 15);   // D/CX bit set to 0 
    cmd |= (0x1 << 14);   // RDX bit set to 1
    cmd |= (0x0 << 13);   // WRX bit set to 0 

    spi_write_16_blocking(cmd);
}

static void tft_send_data(uint16_t word) {
    word |= (0x1 << 15);   // D/CX bit set to 1 
    word |= (0x1 << 14);   // RDX bit set to 1
    word |= (0x0 << 13);   // WRX bit set to 0 

    spi_write_16_blocking(word);
}

void tft_setup(void) {
    hardware_reset();
    system_delay(120);

    tft_send_command(TFT_CMD_DISPLAY_OFF);
    system_delay(120);

    tft_send_command(TFT_CMD_SOFTWARE_RESET);
    system_delay(120);

    tft_send_command(TFT_CMD_SLEEP_OUT);
    system_delay(60);

    tft_send_command(TFT_CMD_MEM_ACS_CTRL);
    tft_send_data(TFT_DATA_STD_MEM_ACS_CTRL);
    tft_send_data(0x0);
    tft_send_data(0x0);

    // Set pixel format to 16 bit/pixel
    tft_send_command(TFT_CMD_PIXEL_FORMAT_SET);
    tft_send_data(TFT_DATA_FORMAT_16);

    // Set frame rate to 60 fps
    tft_send_command(TFT_CMD_FRAME_RATE_CONTROL);
    tft_send_data(TFT_DATA_FRAME_RATE_DIVA);
    tft_send_data(TFT_DATA_FRAME_RATE_RTNA);

    tft_send_command(TFT_CMD_DISPLAY_ON);
}

void tft_set_addr_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    tft_send_command(TFT_CMD_COL_ADDR_SET);

    // Sending start column counter
    tft_send_data(WORD_TOP(x0));
    tft_send_data(WORD_BOTTOM(x0));

    // Sending end column counter
    tft_send_data(WORD_TOP(x1));
    tft_send_data(WORD_BOTTOM(x1));

    tft_send_command(TFT_CMD_PAGE_ADDR_SET);
    // Sending start page counter
    tft_send_data(WORD_TOP(y0));
    tft_send_data(WORD_BOTTOM(y0));

    // Sending end page counter
    tft_send_data(WORD_TOP(y1));
    tft_send_data(WORD_BOTTOM(y1));
}

void tft_push_color(uint16_t *color, int cnt) {
    tft_send_command(TFT_CMD_MEMORY_WRITE); 

    for (int index = 0; index < cnt; index++) 
         tft_send_data(color[index]);
}

