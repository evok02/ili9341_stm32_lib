#include "spi.h"

static void nss_set(bool high) {
    if (!high) {
        gpio_clear(GPIOA, GPIO4);
        system_delay(1);
    } else {
        system_delay(1);
        gpio_set(GPIOA, GPIO4);
    }
}

void spi_setup(void) {
    rcc_periph_reset_pulse(RST_SPI1);
    nss_set(HIGH);
    spi_init_master(SPI1, SPI_CR1_BAUDRATE_FPCLK_DIV_8,
                    SPI_CR1_CPOL_CLK_TO_1_WHEN_IDLE,
                    SPI_CR1_CPHA_CLK_TRANSITION_1,
                    SPI_CR1_DFF_16BIT, SPI_CR1_MSBFIRST);

    spi_enable(SPI1);
}

void spi_write_data(uint16_t* data, uint32_t length) {
    nss_set(LOW);
    for (uint32_t index = 0; index < length; index++) {
        spi_send(SPI1, data[index]);
    }
    nss_set(HIGH);
}

void spi_read_data(uint16_t* data, uint32_t length) {
    nss_set(LOW);
    for (uint32_t index = 0; index < length; index++) {
        data[index] = spi_read(SPI1);
    }
    nss_set(HIGH);
}

void spi_write_16_blocking(uint16_t word) {
    nss_set(LOW);
    spi_send(SPI1, word);
    nss_set(HIGH);
}
