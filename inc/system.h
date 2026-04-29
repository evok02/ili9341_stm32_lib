#ifndef INC_SYSTEM_H
#define INC_SYSTEM_H

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/vector.h>
#include <libopencm3/cm3/systick.h>

void system_gpio_setup(void);
void system_rcc_setup(void);

void system_systick_setup(void);

void system_delay(uint32_t delay_ms);

#endif
