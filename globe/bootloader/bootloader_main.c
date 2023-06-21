/**
 * Copyright (c) 2016 - 2019, Nordic Semiconductor ASA
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/** @file
 *
 * @defgroup bootloader_secure_ble main.c
 * @{
 * @ingroup dfu_bootloader_api
 * @brief Bootloader project main file for secure DFU.
 *
 */

#include <app_error.h>
#include <app_error_weak.h>
#include <boards.h>
#include <nrf_bootloader.h>
#include <nrf_bootloader_app_start.h>
#include <nrf_bootloader_dfu_timers.h>
#include <nrf_bootloader_info.h>
#include <nrf_delay.h>
#include <nrf_dfu.h>
#include <nrf_log.h>
#include <nrf_log_ctrl.h>
#include <nrf_log_default_backends.h>
#include <nrf_mbr.h>
#include <stdint.h>

void app_error_handler_bare(ret_code_t error_code) { (void)error_code; }

/**
 * @brief Function notifies certain events in DFU process.
 */
static void dfu_observer(nrf_dfu_evt_type_t evt_type) {
    switch (evt_type) {
    case NRF_DFU_EVT_DFU_FAILED:
        NRF_LOG_INFO("DFU failed");
        break;
    case NRF_DFU_EVT_DFU_ABORTED:
        NRF_LOG_INFO("DFU aborted");
        break;
    case NRF_DFU_EVT_DFU_INITIALIZED:
        NRF_LOG_INFO("DFU initialized");
        break;
    case NRF_DFU_EVT_TRANSPORT_ACTIVATED:
        NRF_LOG_INFO("DFU transport activated");
        break;
    case NRF_DFU_EVT_DFU_STARTED:
        NRF_LOG_INFO("DFU started");
        break;
    default:
        break;
    }
}

/**@brief Function for application main entry. */
int main(void) {
    uint32_t ret_val;

    NRF_LOG_INIT(NULL);
    /* NRF_LOG_DEFAULT_BACKENDS_INIT(); */

    NRF_LOG_INFO("Entering bootloader");

    /* // Must happen before flash protection is applied, since it edits a protected
     * page. */
    nrf_bootloader_mbr_addrs_populate();

    // Protect MBR and bootloader code from being overwritten.
    ret_val = nrf_bootloader_flash_protect(0, MBR_SIZE, false);
    APP_ERROR_CHECK(ret_val);
    ret_val =
        nrf_bootloader_flash_protect(BOOTLOADER_START_ADDR, BOOTLOADER_SIZE, false);
    APP_ERROR_CHECK(ret_val);

    ret_val = nrf_bootloader_init(dfu_observer);
    APP_ERROR_CHECK(ret_val);

    APP_ERROR_CHECK_BOOL(false);
}

/**
 * @}
 */
