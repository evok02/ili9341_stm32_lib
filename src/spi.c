#include "spi.h"

#define SET_NSS(high) (high ? gpio_set(GPIOA, GPIO4) : gpio_clear(GPIOA, GPIO4))

void spi_setup(void) {
    rcc_periph_reset_pulse(RST_SPI1);
    spi_init_master(SPI1, SPI_CR1_BAUDRATE_FPCLK_DIV_8,
                    SPI_CR1_CPOL_CLK_TO_0_WHEN_IDLE,
                    SPI_CR1_CPHA_CLK_TRANSITION_1,
                    SPI_CR1_DFF_8BIT, SPI_CR1_MSBFIRST);

    // SPI_CR2(SPI1) |= SPI_CR2_SSOE;      // set nss pin a output
    // SPI_CR1(SPI1) &= (~SPI_CR1_SSM);    // set ssm to 0

    SET_NSS(HIGH);

    spi_enable(SPI1);
}

void spi_write_data(uint8_t* buffer, uint32_t length) {
    const uint8_t* end = buffer + length;
    while (buffer < end) spi_send_byte_blocking(*buffer++);
}

void spi_read_data(uint8_t* data, uint32_t length) {
    for (uint32_t index = 0; index < length; index++) {
        data[index] = spi_read(SPI1);
    }
}

void spi_send_byte_blocking(uint8_t byte) {
    spi_send(SPI1, (uint16_t)byte);
}


