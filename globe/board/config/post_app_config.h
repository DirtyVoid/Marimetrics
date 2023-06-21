#ifndef POST_APP_CONFIG_H
#define POST_APP_CONFIG_H

/*
 * App configuration file for storing sdk configuration changes, to avoid
 * modifying sdk_config.h
 */

#undef SPI_ENABLED

#undef NRFX_SPIM_ENABLED
#define NRFX_SPIM_ENABLED 1

#undef NRFX_SPIM0_ENABLED
#define NRFX_SPIM0_ENABLED 1

#undef BLE_NUS_ENABLED
#define BLE_NUS_ENABLED 1

#undef NRF_SDH_BLE_VS_UUID_COUNT
#define NRF_SDH_BLE_VS_UUID_COUNT 2

#undef NRF_SDH_BLE_GATT_MAX_MTU_SIZE
#define NRF_SDH_BLE_GATT_MAX_MTU_SIZE 247

#undef NRFX_SPIM_MISO_PULL_CFG
#define NRFX_SPIM_MISO_PULL_CFG 3

#define SPI_SS_PIN 31
#define SPI_IRQ_PRIORITY 6

#undef NRFX_UARTE_ENABLED
#undef NRFX_UARTE0_ENABLED

#undef NRFX_UART_ENABLED
#undef NRFX_UART0_ENABLED
#undef NRFX_UART1_ENABLED

#undef UART_LEGACY_SUPPORT

#define UART_LEGACY_SUPPORT 0
#define NRFX_UART_ENABLED 0
#define NRFX_UART0_ENABLED 0
#define NRFX_UART1_ENABLED 0

#define NRFX_UARTE_ENABLED 1
#define NRFX_UARTE0_ENABLED 1

#define UARTE_TX_PIN 3
#define UARTE_RX_PIN 9

#undef PPI_ENABLED
#undef NRFX_PPI_ENABLED
#define NRFX_PPI_ENABLED 1

/* Get rid of legacy TWI so it doesn't screw with NRFX TWI defs */
#undef TWI_ENABLED

#undef NRFX_TWI_ENABLED
#define NRFX_TWI_ENABLED 1

#undef NRFX_TWI1_ENABLED
#define NRFX_TWI1_ENABLED 1

#undef NRFX_TEMP_ENABLED
#define NRFX_TEMP_ENABLED 1

#undef APP_TIMER_KEEPS_RTC_ACTIVE
#define APP_TIMER_KEEPS_RTC_ACTIVE 1

/*Experimentally determined minimum RAM usage for BLE attributes*/
#undef NRF_SDH_BLE_GATTS_ATTR_TAB_SIZE
#define NRF_SDH_BLE_GATTS_ATTR_TAB_SIZE (3000)

#undef PWM_ENABLED

#define NRF_DFU_SETTINGS_VERSION 2

#undef SAADC_ENABLED

#endif
