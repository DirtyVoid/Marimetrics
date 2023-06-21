#include "globe.h"

void delay_ms(epoch_t duration_ms) {
    struct timer t;
    start_one_shot_timer(&t, duration_ms, NULL);
    while (timer_active(&t)) {
        wait_for_event();
    }
}

void wait_until(epoch_t resume_elapsed_ms) {
    delay_ms(resume_elapsed_ms - elapsed_ms());
}
