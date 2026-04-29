#ifndef INC_TFT_H
#define INC_TFT_H

#include <libopencm3/stm32/gpio.h>

#include "spi.h"
#include "system.h"

#define WORD_TOP(word) ((0xff00 & word) >> 8)
#define WORD_BOTTOM(word) (0x00ff & word)

// --- TFT LCD COMMANDS DEFINITIOS
#define TFT_CMD_SOFTWARE_RESET ((uint16_t)0x01U)
#define TFT_CMD_DISPLAY_OFF ((uint16_t)0x28U)
#define TFT_CMD_DISPLAY_ON ((uint16_t)0x29U)
#define TFT_CMD_SLEEP_OUT ((uint16_t)0x11U)
#define TFT_CMD_PIXEL_FORMAT_SET ((uint16_t)0x3AU)
#define TFT_CMD_MEM_ACS_CTRL ((uint16_t)0xF6U)
#define TFT_CMD_FRAME_RATE_CONTROL ((uint16_t)0xB1U) 
#define TFT_CMD_WR_DISPLAY_BRIGHTNESS ((uint16_t)0x51U) 
#define TFT_CMD_COL_ADDR_SET ((uint16_t)0x2AU) 
#define TFT_CMD_PAGE_ADDR_SET ((uint16_t)0x2BU) 
#define TFT_CMD_MEMORY_WRITE ((uint16_t)0x2CU) 


// --- TFT DATA PARAMTERS DEFINITIONS
#define TFT_DATA_STD_MEM_ACS_CTRL ((uint16_t)0x1FU) // mx=0, my=0, mv=1
#define TFT_DATA_FRAME_RATE_DIVA ((uint16_t)0x3U)
#define TFT_DATA_FORMAT_16 ((uint16_t)0x55U) 
#define TFT_DATA_FRAME_RATE_RTNA ((uint16_t)0x1FU)

void tft_setup(void);
void tft_set_addr_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void tft_push_color(uint16_t *color, int cnt);

#endif
