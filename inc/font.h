#ifndef INC_FONT_H
#define INC_FONT_H

#include "common.h"
#include "tft.h"

#define MAX_ROW_COUNT (40)
#define MAX_COL_COUNT (48)

void tft_write_char(uint8_t ascii, uint8_t row, uint8_t col, uint16_t txt_color, uint16_t bg_color);

void tft_write_chars(const uint8_t* ascii, uint8_t cnt, uint8_t row, uint8_t col, uint16_t txt_color, uint16_t bg_color);

#endif
