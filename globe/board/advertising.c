#include "globe.h"

BLE_ADVERTISING_DEF(m_advertising); /**< Advertising module instance. */

/**@brief Function for handling advertising events.
 *
 * @details This function will be called for advertising events which are passed to the
 * application.
 *
 * @param[in] ble_adv_evt  Advertising event.
 */
static void on_adv_evt(ble_adv_evt_t ble_adv_evt) {
    switch (ble_adv_evt) {
    default:
        break;
    }
}

/**@brief Function for initializing the Advertising functionality.
 */
void advertising_init() {

    ret_code_t err_code;

    static ble_uuid_t m_adv_uuids[] = /**< Universally unique service identifiers. */
        {
            {SERVICE_UUID, BLE_UUID_TYPE_VENDOR_BEGIN},
        };

    /* The vendor UUID is the only information sent in the important
     * advertising ID for device identification. The full device name is sent
     * in the scan response for identification prior to connection. */
    ble_advertising_init_t init = {
        .advdata =
            {
                .name_type = BLE_ADVDATA_NO_NAME,
                .include_appearance = false,
                .flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE,
                .uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]),
                .uuids_complete.p_uuids = m_adv_uuids,
            },
        .srdata =
            {
                .name_type = BLE_ADVDATA_FULL_NAME,
                .include_appearance = false,
            },
        .config =
            {
                .ble_adv_fast_enabled = true,
                /* The advertising interval (in units of 0.625 ms). */
                .ble_adv_fast_interval = (unsigned)(2000.0f / 0.625f),
                .ble_adv_fast_timeout = 0,
            },
        .evt_handler = on_adv_evt};

    err_code = ble_advertising_init(&m_advertising, &init);
    assert(!err_code);

    ble_advertising_conn_cfg_tag_set(&m_advertising, APP_BLE_CONN_CFG_TAG);

    err_code = sd_ble_gap_tx_power_set(BLE_GAP_TX_POWER_ROLE_ADV,
                                       m_advertising.adv_handle, ble_tx_power());
    assert(!err_code);
}

/**@brief Function for starting advertising.
 */
void advertising_start(bool erase_bonds) {
    if (erase_bonds == true) {
        ret_code_t err_code;
        err_code = pm_peers_delete();
        assert(!err_code);
        // Advertising is started by PM_EVT_PEERS_DELETED_SUCEEDED event
    } else {
        ret_code_t err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
        assert(!err_code);
    }
}

void advertising_reinit() {
    /* Store Current connection state od adv lib */
    uint16_t current_slave_link_conn_handle_saved =
        m_advertising.current_slave_link_conn_handle;
    advertising_init();
    m_advertising.current_slave_link_conn_handle = current_slave_link_conn_handle_saved;
}
