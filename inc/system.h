#ifndef INC_SYSTEM_H
#define INC_SYSTEM_H

#include <stdlib.h>
#include <string.h>

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/vector.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/f1/timer.h>

#include "printf.h"

#define WORD_HIGH(word) ((0xff00 & word) >> 8)
#define WORD_BOTTOM(word) (0x00ff & word)

void system_gpio_setup(void);
void system_rcc_setup(void);

void system_systick_setup(void);

void system_delay(uint32_t delay_ms);
void pwm_setup(void);

uint32_t get_current_counter(void);

#endif
