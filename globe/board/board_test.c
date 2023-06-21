#include "globe.h"

static void pwm_test() {
    uint32_t pin = sd_card_en_pin();

    pwm_ramp_pin(pin, false, 5);
    pwm_ramp_pin(pin, true, 5);
    pwm_ramp_pin(pin, true, 5); /* Should not do anything */
}

void board_test() { pwm_test(); }
