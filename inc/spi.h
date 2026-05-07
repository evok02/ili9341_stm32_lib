#ifndef INC_SPI_H
#define INC_SPI_H

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/spi.h>
#include <libopencm3/stm32/rcc.h>

#include "common.h"
#include "system.h"

#define HIGH    (true)
#define LOW     (false)

#define SET_NSS(high) { \
    spi_wait(); \
    (high) ? gpio_set(GPIOA, GPIO4) : gpio_clear(GPIOA, GPIO4); \
}

void spi_setup(void);
void spi_write_data(const uint8_t* data, uint32_t length);
void spi_read_data(uint8_t* data, uint32_t length);
uint8_t spi_read_write(uint8_t data);
void spi_send_byte_blocking(uint8_t data);
uint8_t spi_read_byte(void);
void spi_wait(void);

#endif
