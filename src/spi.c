#include "spi.h"
#include "mmc.h"

static volatile xmit_status xmit_state;

void spi_dma_wait( void ) {
    while ( xmit_state != DONE ) __asm__( "nop" );
}

static void spi_dma_enable( void ) {
    nvic_set_priority( NVIC_DMA1_CHANNEL2_IRQ, 0 );
    nvic_set_priority( NVIC_DMA1_CHANNEL3_IRQ, 0 );

    nvic_enable_irq( NVIC_DMA1_CHANNEL2_IRQ );
    nvic_enable_irq( NVIC_DMA1_CHANNEL3_IRQ );
}

void spi_dma_setup( void ) {
    spi_dma_enable();
}

void spi_dma_disable( void ) {
    nvic_disable_irq( NVIC_DMA1_CHANNEL2_IRQ );
    nvic_disable_irq( NVIC_DMA1_CHANNEL3_IRQ );
}

int spi_dma_xmit( uint8_t const *tx_buf, size_t tx_len, uint8_t *rx_buf, size_t rx_len ) {
    if ( rx_len < 1 && tx_len < 1 ) return -1;

    xmit_state = NONE;

    dma_channel_reset( DMA1, DMA_CHANNEL2 );
    dma_channel_reset( DMA1, DMA_CHANNEL3 );

    if ( rx_len > 0 ) {
        dma_set_peripheral_address( DMA1, DMA_CHANNEL2, ( uint32_t )&SPI1_DR );
        dma_set_memory_address( DMA1, DMA_CHANNEL2, ( uint32_t )rx_buf );
        dma_set_number_of_data( DMA1, DMA_CHANNEL2, rx_len );
        dma_set_read_from_peripheral( DMA1, DMA_CHANNEL2 );
        dma_enable_memory_increment_mode( DMA1, DMA_CHANNEL2 );
        dma_set_peripheral_size( DMA1, DMA_CHANNEL2, DMA_CCR_PSIZE_8BIT );
        dma_set_memory_size( DMA1, DMA_CHANNEL2, DMA_CCR_PSIZE_8BIT );
        dma_set_priority( DMA1, DMA_CHANNEL2, DMA_CCR_PL_VERY_HIGH );

        dma_enable_transfer_complete_interrupt( DMA1, DMA_CHANNEL2 );
        dma_enable_channel( DMA1, DMA_CHANNEL2 ); 
        spi_enable_rx_dma( SPI1 );

    }

    if ( tx_len > 0 ) {
        dma_set_peripheral_address( DMA1, DMA_CHANNEL3, ( uint32_t )&SPI1_DR );
        dma_set_memory_address( DMA1, DMA_CHANNEL3, ( uint32_t )tx_buf );
        dma_set_number_of_data( DMA1, DMA_CHANNEL3, tx_len );
        dma_set_read_from_memory( DMA1, DMA_CHANNEL3 );
        dma_enable_memory_increment_mode( DMA1, DMA_CHANNEL3 );
        dma_set_peripheral_size( DMA1, DMA_CHANNEL3, DMA_CCR_PSIZE_8BIT );
        dma_set_memory_size( DMA1, DMA_CHANNEL3, DMA_CCR_PSIZE_8BIT );
        dma_set_priority( DMA1, DMA_CHANNEL3, DMA_CCR_PL_VERY_HIGH );

        dma_enable_transfer_complete_interrupt( DMA1, DMA_CHANNEL3 );
        dma_enable_channel( DMA1, DMA_CHANNEL3 ); 
        spi_enable_tx_dma( SPI1 );
    }

    return 0;
}

void dma1_channel2_isr( void ) {
    gpio_set( GPIOC, GPIO14 );
    if ( ( DMA1_ISR & DMA_ISR_TCIF2 ) != 0 ) DMA1_IFCR |= DMA_IFCR_CTCIF2;
    dma_disable_transfer_complete_interrupt( DMA1, DMA_CHANNEL2 );
    spi_disable_rx_dma( SPI1 );
    dma_disable_channel( DMA1, DMA_CHANNEL2 );

    xmit_state++;
    gpio_clear( GPIOC, GPIO14 );
}

void dma1_channel3_isr( void ) {
    gpio_set( GPIOC, GPIO15 );
    if ( ( DMA1_ISR & DMA_ISR_TCIF3 ) != 0 ) DMA1_IFCR |= DMA_IFCR_CTCIF3;
    spi_disable_tx_dma( SPI1 );
    dma_disable_channel( DMA1, DMA_CHANNEL3 );

    xmit_state++;
    gpio_clear( GPIOC, GPIO15 );
}

void spi_setup(uint32_t baudrate) {
    rcc_periph_reset_pulse(RST_SPI1);

    spi_init_master(SPI1, baudrate, SPI_CR1_CPOL_CLK_TO_0_WHEN_IDLE,
                    SPI_CR1_CPHA_CLK_TRANSITION_1, SPI_CR1_DFF_8BIT, SPI_CR1_MSBFIRST);

    SET_NSS(HIGH);
    _SET_MMC_NSS(HIGH);

    spi_enable(SPI1);
    spi_dma_enable();
}

void spi_reset_disable(void) {
    spi_wait();
    spi_disable(SPI1);
    SET_NSS(HIGH);
    _SET_MMC_NSS(HIGH);
    spi_disable(SPI1);
    rcc_periph_reset_pulse(RST_SPI1);
    spi_dma_disable();
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

