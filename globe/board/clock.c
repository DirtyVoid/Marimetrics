#include "globe.h"

void clock_init() {
    /* The LF-CLK should be started by the SoftDevice if it has already been
     * enabled. */
    assert(nrfx_clock_lfclk_is_running());

    /* Stop RTC1. The RTC may not be reset across a system reset so it is
     * manually done here. If this is skipped the timer may not work. */

    /* Clear counter compare */
    nrf_rtc_cc_set(NRF_RTC1, 0, 0);
    nrf_rtc_task_trigger(NRF_RTC1, NRF_RTC_TASK_CLEAR);
    /* clear event associated with counter compare */
    nrf_rtc_event_clear(NRF_RTC1, NRF_RTC_EVENT_COMPARE_0);

    NVIC_ClearPendingIRQ(RTC1_IRQn);
    int pending = NVIC_GetPendingIRQ(RTC1_IRQn);
    assert(pending == 0);
}
