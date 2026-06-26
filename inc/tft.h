#ifndef INC_TFT_H
#define INC_TFT_H

#include <libopencm3/stm32/gpio.h>

#include "spi.h"
#include "system.h"
#include "font.h"

#define TFT_HEIGHT  (320) // 320 / 8 = 40 (rows)
#define TFT_WIDTH   (240) // 240 / 5 = 48 (chars/row)

// --- TFT LCD COMMANDS DEFINITIOS
#define TFT_CMD_SOFTWARE_RESET          (0x01U)
#define TFT_CMD_DISPLAY_OFF             (0x28U)
#define TFT_CMD_DISPLAY_ON              (0x29U)
#define TFT_CMD_SLEEP_OUT               (0x11U)
#define TFT_CMD_PIXEL_FORMAT_SET        (0x3AU)
#define TFT_CMD_MEM_ACS_CTRL            (0xF6U)
#define TFT_CMD_FRAME_RATE_CONTROL      (0xB1U) 
#define TFT_CMD_WR_DISPLAY_BRIGHTNESS   (0x51U) 
#define TFT_CMD_COL_ADDR_SET            (0x2AU) 
#define TFT_CMD_PAGE_ADDR_SET           (0x2BU) 
#define TFT_CMD_MEMORY_WRITE            (0x2CU) 
#define TFT_CMD_MADCTL                  (0x36U) 


// --- TFT DATA PARAMTERS DEFINITIONS
#define TFT_DATA_STD_MEM_ACS_CTRL       (0x1FU) // mx=0, my=0, mv=1
#define TFT_DATA_FRAME_RATE_DIVA        (0x3U)
#define TFT_DATA_FORMAT_16              (0x55U) 
#define TFT_DATA_FRAME_RATE_RTNA        (0x1FU)

typedef struct {
    uint8_t command;
    uint8_t* arguments;
    size_t argument_count;
} command_t;

static command_t temp_command = { .command = 0, .arguments = 0, .argument_count = 0 };

void tft_setup(void);
void tft_set_brightness(uint8_t percentage_0_100);
void tft_fill_pixel(uint16_t x, uint16_t y, uint16_t color);
void tft_fill_rectangle(uint16_t x0, uint16_t y0, uint16_t w, uint16_t l, uint16_t color);
void tft_fill_screen(uint16_t color);
void tft_draw_picture(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h, uint16_t* data);
void tft_write_char(uint8_t ascii, uint8_t row, uint8_t col, uint16_t txt_color, uint16_t bg_color);

void tft_write_chars(const uint8_t* ascii, uint8_t cnt, uint8_t row, uint8_t col, uint16_t txt_color, uint16_t bg_color);
#endif
