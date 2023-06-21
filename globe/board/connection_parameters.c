#include "globe.h"

/**@brief Function for handling the Connection Parameters Module.
 *
 * @details This function will be called for all events in the Connection Parameters
 * Module which are passed to the application.
 *          @note All this function does is to disconnect. This could have been done by
 * simply setting the disconnect_on_fail config parameter, but instead we use the event
 *                handler mechanism to demonstrate its use.
 *
 * @param[in] p_evt  Event received from the Connection Parameters Module.
 */
static void on_conn_params_evt(ble_conn_params_evt_t *p_evt) {
    ret_code_t err_code;

    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED) {
        err_code = sd_ble_gap_disconnect(ble_interface.cur_conn_handle,
                                         BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
        assert(!err_code);
    }
}

/**@brief Function for handling a Connection Parameters error.
 *
 * @param[in] nrf_error  Error code containing information about what went wrong.
 */
static void conn_params_error_handler(uint32_t nrf_error) {
    APP_ERROR_HANDLER(nrf_error);
}

/**@brief Function for initializing the Connection Parameters module.
 */
void conn_params_init(void) {
    ble_conn_params_init_t cp_init = {
        .p_conn_params = NULL,
        .first_conn_params_update_delay = APP_TIMER_TICKS(5000),
        .next_conn_params_update_delay = APP_TIMER_TICKS(30000),
        .max_conn_params_update_count = 3,
        .start_on_notify_cccd_handle = BLE_GATT_HANDLE_INVALID,
        .disconnect_on_fail = false,
        .evt_handler = on_conn_params_evt,
        .error_handler = conn_params_error_handler,
    };
    ret_code_t err_code = ble_conn_params_init(&cp_init);
    assert(!err_code);
}
