#include "globe.h"

void system_reset_init();
void stack_usage_init();
void device_id_init();
void clock_init();
void i2c_board_init();
void uart_init();
void peripheral_init();
void power_management_init();

void board_init() {

    enum board_config rev = board_revision();

    assert(rev == BOARD_CONFIG_BARE || rev == BOARD_CONFIG_R4_2 ||
           rev == BOARD_CONFIG_R4_3 || rev == BOARD_CONFIG_DEV);

    /* Enable SoftDevice. Must be done prior to clock initialization because
     * the SoftDevice must start the low-frequency clock, which is required for
     * the RTC. */
    ret_code_t err_code = nrf_sdh_enable_request();
    assert(err_code == NRF_SUCCESS);

    clock_init();

    system_reset_init(); /* Must be called after clock_init. Should be called
                          * as soon as possible to minimize time tracking error
                          * across reset. */
    timer_module_init();

    stack_usage_init(); /* Should be called as soon as possible to start stack
                         * usgage monitoring (execution prior to this call is
                         * not monitored). May take a small bit of time as
                         * memory needs to be cleared. */

    NRF_LOG_INIT(NULL);
    NRF_LOG_DEFAULT_BACKENDS_INIT();

    NRF_LOG_INFO("Entering Application");

    device_id_init();
    peripheral_init();

    uart_init();

    power_management_init(); /* Must be called after comm inits */

    ble_init();
}
