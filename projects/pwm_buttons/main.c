/**
 * Copyright (c) 2015 - 2017, Nordic Semiconductor ASA
 * 
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 * 
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 * 
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 * 
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 * 
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */

#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include "nrf_drv_pwm.h"
#include "app_util_platform.h"
#include "boards.h"
#include "bsp.h"
#include "app_timer.h"
#include "nrf_drv_clock.h"
#include "nrf_delay.h"
#include "nrf_drv_timer.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#ifndef PI
  #define PI 3.141592653589793
#endif

#define PROGRAM_NAME "pwm_buttons"

// Buttons to control program
#define PWM_BUTTON_DEC    BSP_BUTTON_0
#define PWM_BUTTON_INC    BSP_BUTTON_1
#define PWM_BUTTON_STOP   BSP_BUTTON_2
#define PWM_BUTTON_START  BSP_BUTTON_3
#define NUM_BUTTONS   4

#define NUM_PWM_MAX_CYCLES 1024

#define PWM_LED

#ifdef PWM_LED
  #define PWM_NRF_CLK        NRF_PWM_CLK_125kHz
  #define PWM_FCLK                        125e3
  #define PWM_FCYCLE                     0.10e3
  #define PWM_FIN                          0.25
  #define PWM_TIME                         30.0
  #define PWM_OUTPUT0                 BSP_LED_0
  #define PWM_OUTPUT1  NRF_DRV_PWM_PIN_NOT_USED
#else
  #define PWM_NRF_CLK         NRF_PWM_CLK_16MHz
  #define PWM_FCLK                         16e6
  #define PWM_FCYCLE                      160e3
  #define PWM_FIN                           1e3
  #define PWM_TIME                         10.0
  #define PWM_OUTPUT0  NRF_DRV_PWM_PIN_NOT_USED
  #define PWM_OUTPUT1                 BSP_LED_0
#endif

static nrf_drv_pwm_t m_pwm0 = NRF_DRV_PWM_INSTANCE(0);

static uint8_t event_start = 0;
static uint8_t event_dec   = 0;
static uint8_t event_inc   = 0;
static uint8_t pwm_running = 0;
static uint8_t event_stop  = 0;

/**
 * Button handler
 */
static void button_handler(uint8_t pin_no, uint8_t button_action) {
  switch (pin_no) {
    case PWM_BUTTON_START:
      event_start = 1;
      break;
    case PWM_BUTTON_STOP:
      event_stop = 1;
      break;
    case PWM_BUTTON_DEC:
      event_dec = 1;
      break;
    case PWM_BUTTON_INC:
      event_inc = 1;
      break;
  }
}

/**
 * Timer handler
 */
static void timer_handler(nrf_timer_event_t event_type, void* p_context) {
  switch (event_type) {
    case NRF_TIMER_EVENT_COMPARE0:
      event_stop = 1;
      break;
    default:
      break;
  }
}

/**
 * Initialize LEDs
 */
static void init_leds() {
  NRF_LOG_INFO("init_leds()");

  bsp_board_leds_init();
}

/**
 * Initialize Buttons
 */
static void init_buttons() {
  uint32_t error_code;
  static app_button_cfg_t buttons[NUM_BUTTONS];

  NRF_LOG_INFO("init_buttons()");

  buttons[0].pin_no         = PWM_BUTTON_START;
  buttons[0].active_state   = APP_BUTTON_ACTIVE_LOW;
  buttons[0].pull_cfg       = NRF_GPIO_PIN_PULLUP;
  buttons[0].button_handler = button_handler;

  buttons[1].pin_no         = PWM_BUTTON_STOP;
  buttons[1].active_state   = APP_BUTTON_ACTIVE_LOW;
  buttons[1].pull_cfg       = NRF_GPIO_PIN_PULLUP;
  buttons[1].button_handler = button_handler;

  buttons[2].pin_no         = PWM_BUTTON_INC;
  buttons[2].active_state   = APP_BUTTON_ACTIVE_LOW;
  buttons[2].pull_cfg       = NRF_GPIO_PIN_PULLUP;
  buttons[2].button_handler = button_handler;

  buttons[3].pin_no         = PWM_BUTTON_DEC;
  buttons[3].active_state   = APP_BUTTON_ACTIVE_LOW;
  buttons[3].pull_cfg       = NRF_GPIO_PIN_PULLUP;
  buttons[3].button_handler = button_handler;

  error_code = app_button_init(buttons, NUM_BUTTONS, APP_TIMER_TICKS(50));
  if (error_code != NRF_SUCCESS) {
    return;
  }

  error_code = app_button_enable();
  if (error_code != NRF_SUCCESS) {
    return;
  }
}

/**
 * Uninitialize PWM
 */
static void uninit_pwm() {
  // Uninitialize pwm driver
  nrf_drv_pwm_uninit(&m_pwm0);
}

/**
 * Initialize PWM
 */
static void init_pwm(uint16_t top) {
  uint32_t error_code;
  nrf_drv_pwm_config_t pwm_config;

  NRF_LOG_INFO("init_pwm()");

  // Uninitialize pwm driver
  uninit_pwm();

  // Set pwm configuration
  pwm_config.output_pins[0] = PWM_OUTPUT0;
  pwm_config.output_pins[1] = PWM_OUTPUT1;
  pwm_config.output_pins[2] = NRF_DRV_PWM_PIN_NOT_USED;
  pwm_config.output_pins[3] = NRF_DRV_PWM_PIN_NOT_USED;
  pwm_config.irq_priority   = APP_IRQ_PRIORITY_LOWEST;
  pwm_config.base_clock     = PWM_NRF_CLK;
  pwm_config.count_mode     = NRF_PWM_MODE_UP;
  pwm_config.top_value      = top;
  pwm_config.load_mode      = NRF_PWM_LOAD_COMMON;
  pwm_config.step_mode      = NRF_PWM_STEP_AUTO;

  // Initialize pwm driver with new settings
  error_code = nrf_drv_pwm_init(&m_pwm0, &pwm_config, NULL);
  if (error_code != NRF_SUCCESS) {
    return;
  }
}

/**
 * Initialize Timer
 */
static void init_timer(nrf_drv_timer_t *timer_ptr) {
  uint32_t error_code;
  uint32_t timer_ticks;
  nrf_drv_timer_config_t timer_cfg = NRF_DRV_TIMER_DEFAULT_CONFIG;

  NRF_LOG_INFO("init_timer()");

  // Initialize app timer for PWM
  error_code = app_timer_init();
  if (error_code != NRF_SUCCESS) {
    return;
  }

  // Initilize timer with default configuration
  error_code = nrf_drv_timer_init(timer_ptr, &timer_cfg, timer_handler);
  if (error_code != NRF_SUCCESS) {
    return;
  }

  // Get number of timer ticks for PWM time
  timer_ticks = nrf_drv_timer_ms_to_ticks(timer_ptr, 1000*PWM_TIME);

  // Set timer compare channel 0 and clear & stop timer and interrupt on compare
  nrf_drv_timer_extended_compare(
      timer_ptr, NRF_TIMER_CC_CHANNEL0, timer_ticks,
      NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK | NRF_TIMER_SHORT_COMPARE0_STOP_MASK,
      true);
}

/**
 * Initialize Clock
 */
static void init_clock() {
  uint32_t error_code;

  NRF_LOG_INFO("init_clock()");

  error_code = nrf_drv_clock_init();
  if (error_code != NRF_SUCCESS) {
    return;
  }

  nrf_drv_clock_lfclk_request(NULL);
}

/**
 * Initialize log
 */
static void init_log() {
  NRF_LOG_INIT(NULL);
  NRF_LOG_DEFAULT_BACKENDS_INIT();

  NRF_LOG_INFO(PROGRAM_NAME);
}

/**
 * Start Timer
 */
static void timer_start(nrf_drv_timer_t *timer_ptr) {
  // Clear timer and start
  nrf_drv_timer_clear(timer_ptr);
  nrf_drv_timer_enable(timer_ptr);
}

/**
 * Start PWM sequence
 */
static void pwm_start(float freq) {
  static nrf_pwm_values_common_t duty_cycle[NUM_PWM_MAX_CYCLES];
  nrf_pwm_sequence_t pwm_seq;

  uint16_t pwm_clks_per_cycle;
  uint16_t pwm_cycles_per_period;

  // Number of system clocks per pwm cycle
  pwm_clks_per_cycle = (uint16_t) PWM_FCLK/PWM_FCYCLE;

  // Number of pwm cycles per period of input
  pwm_cycles_per_period = (uint16_t) (PWM_FCLK/pwm_clks_per_cycle)/freq;

  // Generate PWM duty cycle array
  for (uint16_t k = 0; k < pwm_cycles_per_period; k++) {
    float amplitude, cycles;

    // Calculate amplitude of sine function at input frequency
    amplitude = 0.5*sinf(2*PI*k/pwm_cycles_per_period) + 0.5;

    // Calculate number of PWM cycles that correspond to this amplitude
    cycles = roundf(pwm_clks_per_cycle*amplitude);

    // Value to duty cycle array
    duty_cycle[k] = (uint16_t) cycles;
  }

  // Initialize pwm with top counter
  init_pwm(pwm_clks_per_cycle);

  // Set pwm sequence
  pwm_seq.values.p_common = duty_cycle;
  pwm_seq.length          = pwm_cycles_per_period;
  pwm_seq.repeats         = 0;
  pwm_seq.end_delay       = 0;

  // Play pwm sequence and loop it pwm_loops number of times, then stop
  nrf_drv_pwm_simple_playback(&m_pwm0, &pwm_seq, 1, NRF_DRV_PWM_FLAG_LOOP);
}

/**
 * Stop PWM sequence
 */
static void pwm_stop() {
  uninit_pwm();
}

/**
 *  1. Initialize everything
 *  2. Wait for button press
 *      a. PWM_BUTTON_DEC/PWM_BUTTON_INC
 *          1. Increase/decrease frequency
 *          2. Deinit old pwm
 *          3. Create pwm vector and initialize
 *          4. Init new pwm
 *      b. PWM_BUTTON_START
 *          1. Start pwm sequence
 *  3. Wait for PWM sequence to end
 *      a. Clear output
 */
int main(void) {
  static float freq;
  nrf_drv_timer_t timer = NRF_DRV_TIMER_INSTANCE(0);

  // Initialize everything
  init_log();
  init_timer(&timer);
  init_clock();
  init_leds();
  init_buttons();

  // Set initial frequency
  freq = PWM_FIN;

  NRF_LOG_INFO("main(): start loop, freq=%f", freq);

  // Loop forever
  while (true) {
    // Stop pwm sequence
    if (event_stop) {
      pwm_stop();
      event_stop = 0;
      pwm_running = 0;

      // Clear LED
      // TODO make this fit defines better
      nrf_gpio_pin_set(BSP_LED_0);
    }

    // Start pwm sequence
    if (event_start) {
      NRF_LOG_INFO("main(): begin pwm sequence", freq);
      pwm_start(freq);
      timer_start(&timer);
      event_start = 0;
      pwm_running = 1;
    }

    // Increment frequency
    if (event_inc) {
      // Ensure freq is always less than PWM_FCLK/2
      if (freq <= PWM_FCLK/4) {
        freq *= 2;
        NRF_LOG_INFO("main(): increment freq to %f", freq);
      }
      // Reset event
      event_inc = 0;
      // Change frequency
      if (pwm_running) {
        pwm_start(freq);
      }
    }

    // Decrement frequency
    if (event_dec) {
      // Ensure freq is high enough that vector will fit into array
      if (freq > PWM_FCYCLE/NUM_PWM_MAX_CYCLES) {
        freq /= 2;
        NRF_LOG_INFO("main(): decrement freq to %f", freq);
      }
      // Reset event
      event_dec = 0;
      // Change frequency
      if (pwm_running) {
        pwm_start(freq);
      }
    }
  }
}
