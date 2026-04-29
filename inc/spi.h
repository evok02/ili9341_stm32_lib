#ifndef INC_SPI_H
#define INC_SPI_H

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/spi.h>
#include <libopencm3/stm32/rcc.h>

#include "common.h"
#include "system.h"

#define HIGH    (true)
#define LOW     (false)

void spi_setup(void);
void spi_write_data(uint16_t* data, uint32_t length);
void spi_read_data(uint16_t* data, uint32_t length);
void spi_write_16_blocking(uint16_t data);

#endif
