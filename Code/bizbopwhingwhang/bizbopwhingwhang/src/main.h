/* main.h

*/

/*
   Copyright (c) 2024 Daniel Marks

  This software is provided 'as-is', without any express or implied
  warranty. In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#ifndef _MAIN_H
#define _MAIN_H

#include "hardware/pio.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "hardware/timer.h"
#include "hardware/sync.h"
#include "hardware/flash.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DAC_PWM_A3 9
#define DAC_PWM_A2 8
#define DAC_PWM_A1 7
#define DAC_PWM_A0 6

#define DAC_PWM_B3 15
#define DAC_PWM_B2 14
#define DAC_PWM_B1 13
#define DAC_PWM_B0 12

#define DAC_PWM_WRAP_VALUE 0x100

#define GPIO_ADC_SEL0 16
#define GPIO_ADC_SEL1 17
#define GPIO_ADC_SEL2 18

#define GPIO_OUTPUT_ENABLE 10
#define GPIO_RCLK 8
#define GPIO_SCLK 2
#define GPIO_SDATA 3
#define GPIO_RDATA 0

#define SPI_DEF spi0

#define ADC_A1 26
#define ADC_A2 27
#define ADC_A3 28

#define FLASH_BANKS 32
#define FLASH_PAGE_BYTES 4096u
#define FLASH_OFFSET_STORED (2*1024*1024)
#define FLASH_BASE_ADR 0x10000000
#define FLASH_MAGIC_NUMBER 0xFEE1FEDA

#define PROJECT_MAGIC_NUMBER 0xF00BB00F

void idle_task(void);

typedef struct _project_configuration
{ 
  uint32_t  magic_number;
} project_configuration;

extern project_configuration pc;

#ifndef LED_PIN
#define LED_PIN 25
#endif /* LED_PIN */

#ifdef __cplusplus
}
#endif

#endif // _MAIN_H
