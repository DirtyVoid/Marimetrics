#ifndef BOARD_INTERNAL_H
#define BOARD_INTERNAL_H

#include "globe.h"

void pwm_ramp_pin(uint32_t pin, bool value, int ramp_duration_ms);

void peer_manager_init();
void conn_params_init();
void nus_init();
void nus_handle_connect();
void nus_handle_disconnect();
void nus_handle_connect();
void nus_handle_receive_data(const char *data, int length);
void advertising_init();
void ble_init();
void advertising_reinit();
void advertising_start(bool erase_bonds);
int ble_tx_power();

void uart_disconnect_pins();
void uart_reconnect_pins();

void i2c_disconnect_pins();
void i2c_reconnect_pins();

struct interface_data {
    uint16_t cur_conn_handle;
    uint16_t service_handle;
    uint8_t uuid_type;
};

extern struct interface_data ble_interface;

/* Number of notifications currently in progress. Set to -1 if a disconnect
 * occurred with active notifications. */
extern int notifications_in_progress;
extern bool uart_connected_evt;

#define SERVICE_UUID 0x00bd
#define APP_BLE_CONN_CFG_TAG                                                           \
    1 /**< A tag identifying the SoftDevice BLE configuration. */

void timer_module_init();

#endif
