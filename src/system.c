#include "system.h"

#define STD_STK_RELOAD_VAL (24e6 * 1e-3)

#define NOP __asm__("nop")

uint32_t counter;

void system_rcc_setup(void) {
    rcc_clock_setup_pll(&rcc_hse_configs[RCC_CLOCK_HSE8_24MHZ]);

    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_GPIOC);
    rcc_periph_clock_enable(RCC_SPI1);
}

void system_gpio_setup(void) {
    // Onboard LED
    gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_2_MHZ,
                  GPIO_CNF_OUTPUT_PUSHPULL, GPIO13);

    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_2_MHZ,
                  GPIO_CNF_OUTPUT_PUSHPULL, GPIO1);

    // MOSI1, SCK1, NSS1
    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_10_MHZ,
                  GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO7 | GPIO5 );
    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_10_MHZ, 
                  GPIO_CNF_OUTPUT_PUSHPULL, GPIO4);
    // MISO1
    gpio_set_mode(GPIOA, GPIO_MODE_INPUT,
                  GPIO_CNF_INPUT_FLOAT, GPIO6);
}

void sys_tick_handler(void) {
    counter++;
}

static uint32_t get_current_counter(void) {
    return counter;
}

void system_delay(uint32_t delay_ms) {
    uint32_t final_value = get_current_counter() + delay_ms;
    while (get_current_counter() < final_value) NOP;
}

void system_systick_setup(void) {
    systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);
    systick_set_reload(STD_STK_RELOAD_VAL);

    systick_counter_enable();
    systick_interrupt_enable();
}
