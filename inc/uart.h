#ifndef INC_UART_H
#define INC_UART_H

#include <libopencm3/stm32/usart.h>

#include "common.h"

void uart_setup(void);
void uart_write8(uint32_t uart_periph, uint8_t data);

#endif
