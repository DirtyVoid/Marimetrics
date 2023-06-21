#include "globe.h"

/**@brief Function for handling Peer Manager events.
 *
 * @param[in] p_evt  Peer Manager event.
 */
static void pm_evt_handler(pm_evt_t const *p_evt) {
    pm_handler_on_pm_evt(p_evt);
    pm_handler_flash_clean(p_evt);

    switch (p_evt->evt_id) {
    case PM_EVT_PEERS_DELETE_SUCCEEDED:
        advertising_start(false);
        break;

    default:
        break;
    }
}

/**@brief Function for the Peer Manager initialization.
 */
void peer_manager_init(void) {
    ret_code_t err_code;

    err_code = pm_init();
    assert(!err_code);

    ble_gap_sec_params_t sec_param = {
        .bond = 1,
        .mitm = 0,
        .lesc = 0,
        .keypress = 0,
        .io_caps = BLE_GAP_IO_CAPS_NONE,
        .oob = 0,
        .min_key_size = 7,
        .max_key_size = 16,
        .kdist_own.enc = 1,
        .kdist_own.id = 1,
        .kdist_peer.enc = 1,
        .kdist_peer.id = 1,
    };

    err_code = pm_sec_params_set(&sec_param);
    assert(!err_code);

    err_code = pm_register(pm_evt_handler);
    assert(!err_code);
}
