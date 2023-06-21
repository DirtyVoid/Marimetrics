#ifndef THIRD_PARTY_H
#define THIRD_PARTY_H

#include <bmi160.h>
#include <boost/preprocessor.hpp>
#include <cJSON.h>
#include <cJSON_Utils.h>

/* ff.h must be included before diskio.h as definitions are required in
 * diskio.h */
#include "ff.h"

#include "diskio.h"
#include "ffconf.h"

#include <app_error.h>
#include <app_pwm.h>
#include <app_timer.h>
#include <app_util_platform.h>
#include <ble.h>
#include <ble_advertising.h>
#include <ble_conn_params.h>
#include <ble_conn_state.h>
#include <ble_nus.h>
#include <hardfault.h>
#include <nrf.h>
#include <nrf_ble_gatt.h>
#include <nrf_ble_qwr.h>
#include <nrf_bootloader_info.h>
#include <nrf_gpio.h>
#include <nrf_gpiote.h>
#include <nrf_log.h>
#include <nrf_log_ctrl.h>
#include <nrf_log_default_backends.h>
#include <nrf_nvic.h>
#include <nrf_pwr_mgmt.h>
#include <nrf_rtc.h>
#include <nrf_sdh.h>
#include <nrf_sdh_ble.h>
#include <nrf_soc.h>
#include <nrf_uarte.h>
#include <nrfx.h>
#include <nrfx_clock.h>
#include <nrfx_gpiote.h>
#include <nrfx_ppi.h>
#include <nrfx_pwm.h>
#include <nrfx_saadc.h>
#include <nrfx_spim.h>
#include <nrfx_timer.h>
#include <nrfx_twi.h>
#include <peer_manager_handler.h>

#endif
