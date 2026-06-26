#include "spi.h"
#include "mmc.h"

void spi_setup(uint32_t baudrate) {
    rcc_periph_reset_pulse(RST_SPI1);

    spi_init_master(SPI1, baudrate, SPI_CR1_CPOL_CLK_TO_0_WHEN_IDLE,
                    SPI_CR1_CPHA_CLK_TRANSITION_1, SPI_CR1_DFF_8BIT, SPI_CR1_MSBFIRST);

    SET_NSS(HIGH);
    _SET_MMC_NSS(HIGH);

    spi_enable(SPI1);
}

void spi_reset_disable(void) {
    spi_wait();
    spi_disable(SPI1);
    SET_NSS(HIGH);
    _SET_MMC_NSS(HIGH);
    spi_disable(SPI1);
    rcc_periph_reset_pulse(RST_SPI1);
}

void spi_write_data(const uint8_t* buffer, uint32_t length) {
    const uint8_t* end = buffer + length;
    while (buffer < end) spi_send_byte_blocking(*buffer++);
}

void spi_read_data(uint8_t* data, uint32_t length) {
    for (uint32_t index = 0; index < length; index++) {
        data[index] = spi_read_write(0xFF);
    }
}

void spi_send_byte_blocking(uint8_t byte) {
    spi_send(SPI1, (uint16_t)byte);
}

uint8_t spi_read_byte(void) {
    return spi_read(SPI1);
}

inline void spi_wait(void) {
   while (SPI_SR(SPI1) & SPI_SR_BSY) __asm__("nop");
}

uint8_t spi_read_write(uint8_t data) {
    // spi_wait();
    spi_write(SPI1, data);
    return spi_read(SPI1);
}
