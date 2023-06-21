#include "globe.h"

static nrfx_pwm_t pwm = NRFX_PWM_INSTANCE(0);

static void nrfx_check(nrfx_err_t err) { assert(err == NRFX_SUCCESS); }

void pwm_ramp_pin(uint32_t pin, bool value, int ramp_duration_ms) {
    assert(ramp_duration_ms > 0);
    assert(ramp_duration_ms < 100);

    if (nrf_gpio_pin_out_read(pin) == value)
        return;

    int duty_cycle_count = 100;

    nrfx_pwm_config_t config = {
        .output_pins = {NRFX_PWM_PIN_NOT_USED, NRFX_PWM_PIN_NOT_USED,
                        NRFX_PWM_PIN_NOT_USED, NRFX_PWM_PIN_NOT_USED},
        .irq_priority = NRFX_PWM_DEFAULT_CONFIG_IRQ_PRIORITY,
        .base_clock = NRF_PWM_CLK_16MHz,
        .count_mode = NRF_PWM_MODE_UP,
        /* The counter is automatically reset to zero when COUNTERTOP is
         * reached */
        .top_value = duty_cycle_count,
        .load_mode = NRF_PWM_LOAD_COMMON, /* single output */
        .step_mode = NRF_PWM_STEP_AUTO,
    };

    assert(array_size(config.output_pins) == 4);

    nrfx_check(nrfx_pwm_init(&pwm, &config, NULL));

    /* Value of i means an "off" duty cycle proportion of i percent. Note: must
     * be in RAM (no const). */
    uint16_t ramp_down_values[] = {0, 1, 5, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
    uint16_t ramp_up_values[] = {100, 99, 95, 90, 80, 70, 60, 50, 40, 30, 20, 10, 0};

    uint16_t *values = value ? ramp_up_values : ramp_down_values;
    int num_values = value ? array_size(ramp_up_values) : array_size(ramp_down_values);

    float clock_freq_MHz = 16.0f;
    float duty_cycle_duration_us = duty_cycle_count / clock_freq_MHz;
    float sum_duty_cycle_duration_us = duty_cycle_duration_us * num_values;
    float ramp_duration_us = 1000.0f * ramp_duration_ms;
    int num_duty_cycles = (int)(ramp_duration_us / sum_duty_cycle_duration_us);
    assert(num_duty_cycles > 0);

    nrf_pwm_sequence_t seq = {
        .values.p_common = values,
        .length = num_values,
        .repeats = num_duty_cycles - 1,
        .end_delay = 0,
    };

    nrfx_pwm_simple_playback(&pwm, &seq, 1, 0);

    /* Note: the pin is set after the PWM is started because the pin is
     * otherwise forced low for a few microseconds during initialization.
     * According to datasheet, this update takes effect immediately. */
    pwm.p_registers->PSEL.OUT[0] = pin;

    delay_ms(ramp_duration_ms + 1);

    /* Note:
     *
     * The PSEL.OUT[n] registers and their configurations are only used as long
     * as the PWM module is enabled and PWM generation is active (wave counter
     * started), and retained only as long as the device is in System ON mode,
     * see POWER chapter for more information about power modes.
     */

    nrf_gpio_pin_write(pin, value);

    nrfx_pwm_stop(&pwm, true);
    nrfx_pwm_uninit(&pwm);
}

static int pwm_pin() { return header_gpio_3_pin(); }
static int pwm_enable_pin() { return header_gpio_2_pin(); }

APP_PWM_INSTANCE(PWM1, 1); // Create the instance "PWM1" using TIMER1.

void pwm_ready_callback(uint32_t pwm_id) { (void)pwm_id; }

void set_ssbe_duty_cycle(int duty_cycle_pct) {
    assert(duty_cycle_pct >= 0);
    assert(duty_cycle_pct <= 100);

    bool enable_ssbe_driver = duty_cycle_pct != 0;
    nrf_gpio_pin_write(pwm_enable_pin(), enable_ssbe_driver);

    static bool initialized = false;

    if (!initialized) {
        nrf_gpio_cfg_output(pwm_enable_pin());
        initialized = true;
    }

    bool should_run = duty_cycle_pct > 0 && duty_cycle_pct < 100;

    static bool running = false;

    if (!running && should_run) {
        app_pwm_config_t pwm1_cfg = APP_PWM_DEFAULT_CONFIG_1CH(5000L, pwm_pin());
        nrfx_check(app_pwm_init(&PWM1, &pwm1_cfg, pwm_ready_callback));
        app_pwm_enable(&PWM1);
    } else if (running && !should_run) {
        nrfx_check(app_pwm_uninit(&PWM1));
    }

    running = should_run;

    if (running) {
        ret_code_t err;
        while ((err = app_pwm_channel_duty_set(&PWM1, 0, 100 - duty_cycle_pct)) ==
               NRF_ERROR_BUSY)
            ;
        nrfx_check(err);
    } else {
        bool pin_value = duty_cycle_pct > 50;
        nrf_gpio_pin_write(pwm_pin(), pin_value);
        nrf_gpio_cfg_output(pwm_pin());
    }
}
