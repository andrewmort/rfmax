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

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#ifndef PI
  #define PI 3.141592653589793
#endif


#define PWM_NRF_CLK  NRF_PWM_CLK_125kHz
#define PWM_FCLK                  125e3
#define PWM_FCYCLE                0.10e3
#define PWM_FIN                    0.25 
#define PWM_TIME                   30.0

#define NUM_DUTY_CYCLE 1024

static nrf_drv_pwm_t m_pwm0 = NRF_DRV_PWM_INSTANCE(0);

int main(void) {
  uint32_t error_code;

  static nrf_pwm_values_common_t duty_cycle[NUM_DUTY_CYCLE];
  nrf_drv_pwm_config_t pwm_config;
  nrf_pwm_sequence_t pwm_seq;

  uint16_t pwm_clks_per_cycle;
  uint16_t pwm_cycles_per_period;
  uint16_t pwm_loops;

  // Initialize everything
  nrf_drv_clock_init();
  nrf_drv_clock_lfclk_request(NULL);
  app_timer_init();
  bsp_board_leds_init();
  NRF_LOG_INIT(NULL);
  NRF_LOG_DEFAULT_BACKENDS_INIT();

  // Set initial log info
  NRF_LOG_INFO("Base");

  // Calculate number of clocks and cycles
  pwm_clks_per_cycle = (uint16_t) PWM_FCLK/PWM_FCYCLE;
  pwm_cycles_per_period = (uint16_t) (PWM_FCLK/pwm_clks_per_cycle)/PWM_FIN;
  pwm_loops = (uint16_t) PWM_TIME*PWM_FCLK/(pwm_clks_per_cycle*pwm_cycles_per_period);

  // Generate PWM duty cycle array
  for (uint16_t k = 0; k < pwm_cycles_per_period; k++) {
    float amplitude, cycles;

    // Calculate amplitude of sine function at input frequency
    amplitude = sinf(2*PI*k/pwm_cycles_per_period);

    // Calculate number of PWM cycles that correspond to this amplitude
    cycles = roundf(pwm_clks_per_cycle*amplitude);

    // Value to duty cycle array
    duty_cycle[k] = (uint16_t) cycles;
  }

  // Set pwm configuration
  pwm_config.output_pins[0] = BSP_LED_0;
  pwm_config.output_pins[1] = NRF_DRV_PWM_PIN_NOT_USED;
  pwm_config.output_pins[2] = NRF_DRV_PWM_PIN_NOT_USED;
  pwm_config.output_pins[3] = NRF_DRV_PWM_PIN_NOT_USED;
  pwm_config.irq_priority   = APP_IRQ_PRIORITY_LOWEST;
  pwm_config.base_clock     = PWM_NRF_CLK;
  pwm_config.count_mode     = NRF_PWM_MODE_UP;
  pwm_config.top_value      = pwm_clks_per_cycle;
  pwm_config.load_mode      = NRF_PWM_LOAD_COMMON;
  pwm_config.step_mode      = NRF_PWM_STEP_AUTO;

  // Set pwm sequence
  pwm_seq.values.p_common = duty_cycle;
  pwm_seq.length          = pwm_cycles_per_period;
  pwm_seq.repeats         = 0;
  pwm_seq.end_delay       = 0;

  // Initialize pwm
  error_code = nrf_drv_pwm_init(&m_pwm0, &pwm_config, NULL);

  if (error_code != NRF_SUCCESS) {
    return 1;
  }

  while (true) {
    // Playback sequence
    nrf_drv_pwm_simple_playback(&m_pwm0, &pwm_seq, pwm_loops, NRF_DRV_PWM_FLAG_STOP);

    // Delay for 1 minute
    nrf_delay_ms(60 * 1000);
  }
}
