/* main.cpp

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

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "main.h"
#include "tinycl.h"

uint dac_pwm_slice_num;
uint32_t speed_us = 10000u;

project_configuration pc;

const project_configuration pc_default =
{
  PROJECT_MAGIC_NUMBER,  /* magic_number */
};

void initialize_project_configuration(void)
{
    if (pc.magic_number != PROJECT_MAGIC_NUMBER)
        memcpy((void *)&pc, (void *)&pc_default, sizeof(pc));
}

void idle_task(void)
{
}

void initialize_pwm(void)
{
    gpio_set_function(GPIO_OUTPUT_ENABLE, GPIO_FUNC_PWM);
    dac_pwm_slice_num = pwm_gpio_to_slice_num(GPIO_OUTPUT_ENABLE);
    pwm_set_clkdiv_int_frac(dac_pwm_slice_num, 1, 0);
    pwm_set_clkdiv_mode(dac_pwm_slice_num, PWM_DIV_FREE_RUNNING);
    pwm_set_phase_correct(dac_pwm_slice_num, false);
    pwm_set_wrap(dac_pwm_slice_num, DAC_PWM_WRAP_VALUE-1);
    pwm_set_output_polarity(dac_pwm_slice_num, false, false);
    pwm_set_enabled(dac_pwm_slice_num, true);
}

void initialize_adc(void)
{
    
    adc_init();
    adc_gpio_init(ADC_A1);
    adc_gpio_init(ADC_A2);
    adc_gpio_init(ADC_A3);
    adc_set_clkdiv(100);
    adc_set_round_robin(0);
    adc_run(false);
    adc_select_input(0);
    adc_run(true);
}

void initialize_spi(void)
{
    spi_init(SPI_DEF, 1000000u);
    gpio_set_function(GPIO_RDATA, GPIO_FUNC_SPI);
    gpio_set_function(GPIO_SCLK, GPIO_FUNC_SPI);
    gpio_set_function(GPIO_SDATA, GPIO_FUNC_SPI);
}

void initialize_gpio(void)
{
    // initialize debug LED
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    gpio_init(GPIO_OUTPUT_ENABLE);
    gpio_set_dir(GPIO_OUTPUT_ENABLE, GPIO_OUT);
    
    gpio_init(GPIO_RCLK);
    gpio_set_dir(GPIO_RCLK, GPIO_OUT);

    gpio_init(GPIO_SCLK);
    gpio_set_dir(GPIO_SCLK, GPIO_OUT);

    gpio_init(GPIO_SDATA);
    gpio_set_dir(GPIO_SDATA, GPIO_OUT);
}


#define FLASH_PAGES(x) ((((x)+(FLASH_PAGE_BYTES-1))/FLASH_PAGE_BYTES)*FLASH_PAGE_BYTES)

uint32_t last_gen_no;
uint8_t  desc[16];

typedef struct _flash_layout_data
{
    uint32_t magic_number;
    uint32_t gen_no;
    uint8_t  desc[16];
    project_configuration pc;
} flash_layout_data;

typedef union _flash_layout
{ 
    flash_layout_data fld;
    uint8_t      space[FLASH_PAGE_BYTES];
} flash_layout;

inline static uint32_t flash_offset_bank(uint bankno)
{
    return (FLASH_OFFSET_STORED - FLASH_PAGES(sizeof(flash_layout)) * (bankno+1));
}

inline static const uint8_t *flash_offset_address_bank(uint bankno)
{
    const uint8_t *const flashadr = (const uint8_t *const) FLASH_BASE_ADR;
    return &flashadr[flash_offset_bank(bankno)];
}

int write_data_to_flash(uint32_t flash_offset, const uint8_t *data, uint core, uint32_t length)
{
    length = FLASH_PAGES(length);                // pad up to next 4096 byte boundary
    flash_offset = FLASH_PAGES(flash_offset);
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(flash_offset, length);
    flash_range_program(flash_offset, data, length);
    restore_interrupts(ints);
    return 0;
}

int flash_load_bank(uint bankno)
{
    flash_layout *fl = (flash_layout *) flash_offset_address_bank(bankno);

    if (bankno >= FLASH_BANKS) return -1;
    if (fl->fld.magic_number == FLASH_MAGIC_NUMBER)
    {
        if (fl->fld.gen_no > last_gen_no)
            last_gen_no = fl->fld.gen_no;

        memcpy(desc, fl->fld.desc, sizeof(desc));
        memcpy((void *)&pc, (void *) &fl->fld.pc, sizeof(pc));
        initialize_project_configuration();
    } else return -1;
    return 0;
}

void flash_load_most_recent(void)
{
    uint32_t newest_gen_no = 0;
    uint load_bankno = FLASH_BANKS;

    last_gen_no = 0;
    memset(desc,'\000',sizeof(desc));

    for (uint bankno = 0;bankno < FLASH_BANKS; bankno++)
    {
        flash_layout *fl = (flash_layout *) flash_offset_address_bank(bankno);
        if (fl->fld.magic_number == FLASH_MAGIC_NUMBER)
        {
            if (fl->fld.gen_no >= newest_gen_no)
            {
                newest_gen_no = fl->fld.gen_no;
                load_bankno = bankno;
            }
        }
    }
    if (load_bankno < FLASH_BANKS) 
    {
        flash_load_bank(load_bankno);
    }
}

int flash_save_bank(uint bankno)
{
    flash_layout *fl;

    if (bankno >= FLASH_BANKS) return -1;
    if ((fl = (flash_layout *)malloc(sizeof(flash_layout))) == NULL) return -1;
    memset((void *)fl,'\000',sizeof(flash_layout));
    fl->fld.magic_number = FLASH_MAGIC_NUMBER;
    fl->fld.gen_no = (++last_gen_no);
    memcpy(fl->fld.desc, desc, sizeof(fl->fld.desc));
    memcpy((void *)&fl->fld.pc, (void *)&pc, sizeof(fl->fld.pc));
    int ret = write_data_to_flash(flash_offset_bank(bankno), (uint8_t *) fl, 1, sizeof(flash_layout));
    free(fl);
    return ret;
}

static inline void rclk_strobe() {
    for (volatile int i=0;i<50;i++) {};
    gpio_put(GPIO_RCLK, 1);
    for (volatile int i=0;i<50;i++) {};
    gpio_put(GPIO_RCLK, 0);
    for (volatile int i=0;i<50;i++) {};
}

const uint led_horiz_1[8] = {0,1,2,3,4,5,6,7};
const uint led_horiz_2[8] = {31,30,29,28,27,26,25,24};
const uint led_vert_1[8] = {15,14,13,12,11,10,9,8};
const uint led_vert_2[8] = {23,22,21,20,19,18,17,16};
const uint led_vert_3[8] = {33,34,35,36,37,38,39,32};
const uint led_vert_4[8] = {41,42,43,44,45,46,47,40};
const uint led_aux[4] = {8,16,32,40};

uint8_t leds[6];

static inline void update_leds(void)
{
    spi_write_blocking(SPI_DEF, leds, sizeof(leds)/sizeof(leds[0]));
    rclk_strobe();
}

void clear_led(void)
{
    for (uint i=0;i<(sizeof(leds)/sizeof(leds[0]));i++)
        leds[i] = 0xFF;
}

inline void set_led(uint8_t led)
{
    leds[5-(led/8)] &= ~(1<<(led & 0x07));
}

inline void clear_led(uint8_t led)
{
    leds[5-(led/8)] |= (1<<(led & 0x07));
}

void horiz_leds(int ln)
{
    set_led(led_horiz_1[ln]);
    set_led(led_horiz_2[ln]);
}

void vert_leds(int ln)
{
    set_led(led_vert_1[ln]);
    set_led(led_vert_2[ln]);
    set_led(led_vert_3[ln]);
    set_led(led_vert_4[ln]);
}

uint32_t sample_photodiode(void)
{
    uint32_t tot = 0;
    adc_select_input(0);
    for (uint i=0;i<16;i++)
        tot += adc_read();
    tot /= 16;
    return tot;
}

static inline void set_illumination_level(uint16_t level)
{
    pwm_set_both_levels(dac_pwm_slice_num, level, level);
}

const int32_t transition_level = 100;

bool wait_transition(uint32_t speed_us, bool trans)
{
    static uint32_t timer_last = 0;
    static int32_t last_val = 4095;
    for (;;)
    {
        uint32_t timer_cur = time_us_32();
        if ((timer_cur - timer_last) > speed_us)
        {
            timer_last = timer_cur;
            return false;
        }
        int32_t current_val = sample_photodiode();
        if ((trans) && ((current_val + transition_level) < last_val))
        {
            last_val = current_val;
            return true;
        }
        if ((!trans) && (current_val > (last_val + transition_level)))
        {
            last_val = current_val;
            return true;
        }
    }
}

void move_pattern(void)
{
    static bool on_state = false;
    static uint32_t state = 0;

    if (on_state)
    {
        if (!wait_transition(speed_us, false)) return;
        on_state = false;
        clear_led();
        update_leds();
        return;
    }
    if (!wait_transition(speed_us, true)) return;
    on_state = true;
    state++;
    if ((state >= 0) && (state < 128))
    {
        if ((state & 0x07) < 6)
        {
            clear_led();
            vert_leds(state & 0x07);
            vert_leds((state & 0x07)+1);
        }
        update_leds();
        return;
    }
    state = 0;
}

int speed_cmd(int args, tinycl_parameter* tp, void *v)
{
  uint sp=tp[0].ti.i;
  speed_us = sp;
  return 1;
}

int illum_cmd(int args, tinycl_parameter* tp, void *v)
{
  uint il=tp[0].ti.i;
  set_illumination_level(il & (DAC_PWM_WRAP_VALUE-1));
  return 1;
}

int save_cmd(int args, tinycl_parameter* tp, void *v)
{
  uint bankno=tp[0].ti.i;
  tinycl_put_string((bankno == 0) || flash_save_bank(bankno-1) ? "Not Saved\r\n" : "Saved\r\n");
  return 1;
}

int load_cmd(int args, tinycl_parameter* tp, void *v)
{
  uint bankno=tp[0].ti.i;
  tinycl_put_string((bankno == 0) || flash_load_bank(bankno-1) ? "Not Loaded\r\n" : "Loaded\r\n");
  return 1;
}

int adc_cmd(int args, tinycl_parameter* tp, void *v)
{
    uint16_t adcs[1024];
    adc_select_input(0);
    for (uint i=0;i<(sizeof(adcs)/sizeof(adcs[0]));i++)
    {
 //       sleep_us(100u);
        adcs[i] = sample_photodiode();
    }
    for (uint i=0;i<(sizeof(adcs)/sizeof(adcs[0]));i++)
    {
        printf("%04u ",adcs[i]);
        if ((i % 16) == 0)
            printf("\r\n");
    }
    printf("\r\n\r\n");
    return 0;
}

int help_cmd(int args, tinycl_parameter *tp, void *v);

const tinycl_command tcmds[] =
{
  { "HELP", "Display This Help", help_cmd, {TINYCL_PARM_END } },
  { "ADC", "Test ADC", adc_cmd, {TINYCL_PARM_END } },
  { "ILLUM", "Illumination level", illum_cmd, TINYCL_PARM_INT, TINYCL_PARM_END },
  { "SPEED", "Speed", speed_cmd, TINYCL_PARM_INT, TINYCL_PARM_END },
  { "LOAD", "Load configuration", load_cmd, TINYCL_PARM_INT, TINYCL_PARM_END },
  { "SAVE", "Save configuration", save_cmd, TINYCL_PARM_INT, TINYCL_PARM_END },
};

int help_cmd(int args, tinycl_parameter *tp, void *v)
{
  tinycl_print_commands(sizeof(tcmds) / sizeof(tinycl_command), tcmds);
  return 1;
}

int main()
{
//    sleep_us(250000u);
    stdio_init_all();
//    set_sys_clock_khz(250000u, true);
    initialize_project_configuration();
    initialize_gpio();
    initialize_pwm();
    initialize_spi();
    initialize_adc();
    flash_load_most_recent();

    set_illumination_level(DAC_PWM_WRAP_VALUE-DAC_PWM_WRAP_VALUE/8);
    //set_illumination_level(0);
    clear_led();
    update_leds();

    for (;;)
    {
       move_pattern();
       if (tinycl_task(sizeof(tcmds) / sizeof(tinycl_command), tcmds, NULL))
        {
           tinycl_do_echo = 1;
           tinycl_put_string("> ");
        }
        idle_task();
    }
}

