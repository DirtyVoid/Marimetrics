#include "globe.h"

BLE_NUS_DEF(m_nus, NRF_SDH_BLE_TOTAL_LINK_COUNT); /**< BLE NUS service instance. */

int notifications_in_progress = 0;

static void nus_data_handler(ble_nus_evt_t *p_evt) {
    switch (p_evt->type) {
    case BLE_NUS_EVT_COMM_STARTED:
        nus_handle_connect();
        break;
    case BLE_NUS_EVT_COMM_STOPPED:
        nus_handle_disconnect();
        break;
    case BLE_NUS_EVT_RX_DATA: {
        nus_handle_receive_data((const char *)p_evt->params.rx_data.p_data,
                                p_evt->params.rx_data.length);
        break;
    }
    default:
        break;
    }
}

void nus_init() {
    ble_nus_init_t nus_init = {.data_handler = nus_data_handler};
    ret_code_t err_code = ble_nus_init(&m_nus, &nus_init);
    assert(!err_code);
}

int ble_uart_send(const char *data, int len) {
    const char *end = data + len;
    while (data != end) {
        uint16_t n_to_send = MIN(end - data, BLE_NUS_MAX_DATA_LEN);
        for (;;) {
            /* ble_nus_data_send may fail with NRF_ERROR_RESOURCES if the
             * notifcations haven't been received by the central yet as the
             * transmit buffer will be full. */
            uint32_t err = ble_nus_data_send(&m_nus, (uint8_t *)data, &n_to_send,
                                             ble_interface.cur_conn_handle);
            if (err == NRF_SUCCESS) {
                assert(notifications_in_progress >= 0);
                notifications_in_progress++;
                break;
            }
            /* If the central has not enabled notifications then the call fails
             * with BLE_ERROR_GATTS_SYS_ATTR_MISSING or
             * NRF_ERROR_INVALID_STATE. In this case we can exit (central does
             * not receive data).
             *
             * BLE_ERROR_INVALID_CONN_HANDLE can be returned if the connection
             * is broken.
             */
            if (err == BLE_ERROR_GATTS_SYS_ATTR_MISSING ||
                err == NRF_ERROR_INVALID_STATE ||
                err == BLE_ERROR_INVALID_CONN_HANDLE) {
                return 1;
            }
            assert(err == NRF_ERROR_RESOURCES);
        }
        data += n_to_send;
    }
    return 0;
}

static bool any_notifications_in_progress() {
    CRITICAL_SCOPE();
    return notifications_in_progress > 0;
}

int ble_uart_sync() {
    while (any_notifications_in_progress())
        wait_for_event();
    CRITICAL_SCOPE();
    return notifications_in_progress < 0;
}
