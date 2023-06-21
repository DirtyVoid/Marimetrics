set(CONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../board/config")
set(SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/nRF5_SDK_16.0.0")
set(NRFX_ROOT "${SDK_ROOT}/modules/nrfx")

# The m_fs symol name needs to be renamed in this source file because the same
# symbol name is used in another nRF source file.
set_property(SOURCE ${SDK_ROOT}/components/libraries/bootloader/dfu/nrf_dfu_flash.c
  PROPERTY COMPILE_DEFINITIONS m_fs=m_dfu_fs)

file(GLOB NRF5_INC_DIRS
  # NRFX libraries
  ${NRFX_ROOT}/drivers/include
  ${NRFX_ROOT}
  ${NRFX_ROOT}/hal
  ${NRFX_ROOT}/mdk

  # NRFX Integration
  ${SDK_ROOT}/integration/nrfx
  # SDK libraries
  ${SDK_ROOT}/components
  ${SDK_ROOT}/components/ble/*
  ${SDK_ROOT}/components/ble/ble_services/*
  ${SDK_ROOT}/components/boards
  ${SDK_ROOT}/components/libraries/*
  ${SDK_ROOT}/components/libraries/bootloader/ble_dfu
  ${SDK_ROOT}/components/libraries/bootloader/dfu
  ${SDK_ROOT}/components/libraries/crypto/backend/*
  ${SDK_ROOT}/components/libraries/log/src
  ${SDK_ROOT}/components/softdevice/common
  ${SDK_ROOT}/components/toolchain/cmsis/include
  ${SDK_ROOT}/external/fprintf
  ${SDK_ROOT}/external/nano-pb
  ${SDK_ROOT}/external/nrf_cc310_bl/include
  ${SDK_ROOT}/external/segger_rtt
  ${SDK_ROOT}/external/utf_converter
  ${SDK_ROOT}/integration/nrfx
  ${SDK_ROOT}/integration/nrfx/legacy
  ${CONFIG_DIR}
  )

macro(add_nrf_library target
    flash_start flash_end
    mbr_params_page_start
    bootloader_settings_page_start
    device_define
    softdevice
    softdevice_define
    startup_file
    src_files)
  # The library is defined as an object library so weakly linked symbols can be
  # resolved properly.
  add_library(${target} OBJECT ${startup_file} ${src_files})
  target_compile_definitions(${target} PUBLIC
    BLE_STACK_SUPPORT_REQD
    BLE_DB_DISCOVERY_ENABLED
    APP_TIMER_V2
    APP_TIMER_V2_RTC1_ENABLED
    BOARD_CUSTOM
    CONFIG_GPIO_AS_PINRESET
    CONFIG_NFCT_PINS_AS_GPIOS
    FLOAT_ABI_HARD
    ${device_define}
    NRF_SD_BLE_API_VERSION=7
    NRF_DFU_SETTINGS_VERSION=2
    NRF_DFU_SVCI_ENABLED          # required?
    SVC_INTERFACE_CALL_AS_NORMAL_FUNCTION # required?
    ${softdevice_define}
    SOFTDEVICE_PRESENT
    )
  target_include_directories(${target} PRIVATE
    ${NRF5_INC_DIRS}
    ${SDK_ROOT}/components/softdevice/${softdevice}
    ${SDK_ROOT}/components/softdevice/${softdevice}/headers
    ${SDK_ROOT}/components/softdevice/${softdevice}/headers/nrf52
    )
  target_include_directories(${target} SYSTEM INTERFACE
    ${NRF5_INC_DIRS}
    ${SDK_ROOT}/components/softdevice/${softdevice}
    ${SDK_ROOT}/components/softdevice/${softdevice}/headers
    ${SDK_ROOT}/components/softdevice/${softdevice}/headers/nrf52
    )
  target_link_libraries(${target} PUBLIC micro-ecc)
  target_link_directories(${target} INTERFACE ${NRFX_ROOT}/mdk)
  target_link_directories(${target} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_options(${target} INTERFACE "-Wl,--script=nrf.ld,--defsym=SOFTDEVICE_SIZE=0x5968,--defsym=FLASH_START=${flash_start},--defsym=FLASH_END=${flash_end},--defsym=MBR_PARAMS_PAGE_START=${mbr_params_page_start},--defsym=BOOTLOADER_SETTINGS_PAGE_START=${bootloader_settings_page_start}")
endmacro()

macro(add_nrf52832_library target flash_start flash_end src_files)
  add_nrf_library(
    ${target}
    ${flash_start} ${flash_end}
    0x0007E000       # mbr_params_page_start
    0x0007F000       # bootloader_settings_page_start
    NRF52832_XXAA
    s132
    S132
    ${NRFX_ROOT}/mdk/gcc_startup_nrf52.S
    "${src_files}")
endmacro()

macro(add_nrf52840_library target flash_start flash_end src_files)
  add_nrf_library(
    ${target}
    ${flash_start} ${flash_end}
    0x000FE000       # mbr_params_page_start
    0x000FF000       # bootloader_settings_page_start
    NRF52840_XXAA
    s140
    S140
    ${NRFX_ROOT}/mdk/gcc_startup_nrf52840.S
    "${src_files}")
endmacro()

file(GLOB_RECURSE NRF5_SRC_FILES
  # NRFX src files
  ${NRFX_ROOT}/drivers/src/*.c
  ${NRFX_ROOT}/drivers/src/prs/*.c
  ${NRFX_ROOT}/hal/*.c
  ${NRFX_ROOT}/mdk/system_nrf52.c

  # NRF5 src files
  ${SDK_ROOT}/components/ble/ble_advertising/*.c
  ${SDK_ROOT}/components/ble/ble_db_discovery/*.c
  ${SDK_ROOT}/components/ble/ble_link_ctx_manager/*.c
  ${SDK_ROOT}/components/ble/ble_racp/*.c
  ${SDK_ROOT}/components/ble/ble_radio_notification/*.c
  ${SDK_ROOT}/components/ble/ble_services/ble_cts_c/*.c
  ${SDK_ROOT}/components/ble/ble_services/ble_nus/*.c
  ${SDK_ROOT}/components/ble/ble_services/dis/*.c
  ${SDK_ROOT}/components/ble/common/*.c
  ${SDK_ROOT}/components/ble/nrf_ble_gatt/*.c
  ${SDK_ROOT}/components/ble/nrf_ble_gq/*.c
  ${SDK_ROOT}/components/ble/nrf_ble_qwr/*.c
  ${SDK_ROOT}/components/ble/nrf_ble_scan/*.c
  ${SDK_ROOT}/components/ble/peer_manager/*.c
  ${SDK_ROOT}/components/libraries/atomic/*.c
  ${SDK_ROOT}/components/libraries/atomic_fifo/*.c
  ${SDK_ROOT}/components/libraries/atomic_flags/*.c
  ${SDK_ROOT}/components/libraries/balloc/*.c
  ${SDK_ROOT}/components/libraries/button/*.c
  ${SDK_ROOT}/components/libraries/crc32/*.c
  ${SDK_ROOT}/components/libraries/experimental_section_vars/*.c
  ${SDK_ROOT}/components/libraries/fds/*.c
  ${SDK_ROOT}/components/libraries/fstorage/*.c
  ${SDK_ROOT}/components/libraries/hardfault/hardfault_implementation.c
  ${SDK_ROOT}/components/libraries/hardfault/nrf52/handler/hardfault_handler_gcc.c
  ${SDK_ROOT}/components/libraries/log/*.c
  ${SDK_ROOT}/components/libraries/low_power_pwm/*.c
  ${SDK_ROOT}/components/libraries/pwm/*.c
  ${SDK_ROOT}/components/libraries/memobj/*.c
  ${SDK_ROOT}/components/libraries/pwr_mgmt/*.c
  ${SDK_ROOT}/components/libraries/queue/*.c
  ${SDK_ROOT}/components/libraries/ringbuf/*.c
  ${SDK_ROOT}/components/libraries/scheduler/app_scheduler.c
  ${SDK_ROOT}/components/libraries/sensorsim/*.c
  ${SDK_ROOT}/components/libraries/sortlist/*.c
  ${SDK_ROOT}/components/libraries/strerror/*.c
  ${SDK_ROOT}/components/libraries/timer/app_timer2.c
  ${SDK_ROOT}/components/libraries/timer/drv_rtc.c
  ${SDK_ROOT}/components/libraries/util/app_error.c
  ${SDK_ROOT}/components/libraries/util/app_error_handler_gcc.c
  ${SDK_ROOT}/components/libraries/util/app_error_weak.c
  ${SDK_ROOT}/components/libraries/util/app_util_platform.c
  ${SDK_ROOT}/components/libraries/util/nrf_assert.c
  ${SDK_ROOT}/components/softdevice/common/nrf_sdh.c
  ${SDK_ROOT}/components/softdevice/common/nrf_sdh_ble.c
  ${SDK_ROOT}/components/softdevice/common/nrf_sdh_soc.c
  ${SDK_ROOT}/external/fprintf/*.c
  ${SDK_ROOT}/external/nano-pb/*.c
  ${SDK_ROOT}/external/segger_rtt/*.c
  ${SDK_ROOT}/external/utf_converter/*.c
  ${SDK_ROOT}/integration/nrfx/legacy/nrf_drv_clock.c
  ${SDK_ROOT}/integration/nrfx/legacy/nrf_drv_uart.c
  ${SDK_ROOT}/integration/nrfx/legacy/nrf_drv_ppi.c
  )

# A separate nRF library is used for the bootloader. This library flashes the
# UICR with the start address of the bootloader and MBR params page address.
file(GLOB_RECURSE NRF5_BOOTLOADER_SRC_FILES
  ${SDK_ROOT}/components/ble/common/ble_srv_common.c
  ${SDK_ROOT}/components/libraries/atomic/nrf_atomic.c
  ${SDK_ROOT}/components/libraries/atomic_fifo/nrf_atfifo.c
  ${SDK_ROOT}/components/libraries/balloc/nrf_balloc.c
  ${SDK_ROOT}/components/libraries/bootloader/ble_dfu/nrf_dfu_ble.c
  ${SDK_ROOT}/components/libraries/bootloader/dfu/dfu-cc.pb.c
  ${SDK_ROOT}/components/libraries/bootloader/dfu/nrf_dfu.c
  ${SDK_ROOT}/components/libraries/bootloader/dfu/nrf_dfu_flash.c
  ${SDK_ROOT}/components/libraries/bootloader/dfu/nrf_dfu_handling_error.c
  ${SDK_ROOT}/components/libraries/bootloader/dfu/nrf_dfu_mbr.c
  ${SDK_ROOT}/components/libraries/bootloader/dfu/nrf_dfu_req_handler.c
  ${SDK_ROOT}/components/libraries/bootloader/dfu/nrf_dfu_settings.c
  ${SDK_ROOT}/components/libraries/bootloader/dfu/nrf_dfu_settings_svci.c
  ${SDK_ROOT}/components/libraries/bootloader/dfu/nrf_dfu_svci.c
  ${SDK_ROOT}/components/libraries/bootloader/dfu/nrf_dfu_svci_handler.c
  ${SDK_ROOT}/components/libraries/bootloader/dfu/nrf_dfu_transport.c
  ${SDK_ROOT}/components/libraries/bootloader/dfu/nrf_dfu_utils.c
  ${SDK_ROOT}/components/libraries/bootloader/dfu/nrf_dfu_validation.c
  ${SDK_ROOT}/components/libraries/bootloader/dfu/nrf_dfu_ver_validation.c
  ${SDK_ROOT}/components/libraries/bootloader/nrf_bootloader.c
  ${SDK_ROOT}/components/libraries/bootloader/nrf_bootloader_app_start.c
  ${SDK_ROOT}/components/libraries/bootloader/nrf_bootloader_app_start_final.c
  ${SDK_ROOT}/components/libraries/bootloader/nrf_bootloader_dfu_timers.c
  ${SDK_ROOT}/components/libraries/bootloader/nrf_bootloader_fw_activation.c
  ${SDK_ROOT}/components/libraries/bootloader/nrf_bootloader_info.c
  ${SDK_ROOT}/components/libraries/bootloader/nrf_bootloader_wdt.c
  ${SDK_ROOT}/components/libraries/crc32/crc32.c
  ${SDK_ROOT}/components/libraries/crypto/backend/micro_ecc/micro_ecc_backend_ecc.c
  ${SDK_ROOT}/components/libraries/crypto/backend/micro_ecc/micro_ecc_backend_ecdh.c
  ${SDK_ROOT}/components/libraries/crypto/backend/micro_ecc/micro_ecc_backend_ecdsa.c
  ${SDK_ROOT}/components/libraries/crypto/backend/nrf_sw/nrf_sw_backend_hash.c
  ${SDK_ROOT}/components/libraries/crypto/backend/oberon/oberon_backend_chacha_poly_aead.c
  ${SDK_ROOT}/components/libraries/crypto/backend/oberon/oberon_backend_ecc.c
  ${SDK_ROOT}/components/libraries/crypto/backend/oberon/oberon_backend_ecdh.c
  ${SDK_ROOT}/components/libraries/crypto/backend/oberon/oberon_backend_ecdsa.c
  ${SDK_ROOT}/components/libraries/crypto/backend/oberon/oberon_backend_eddsa.c
  ${SDK_ROOT}/components/libraries/crypto/backend/oberon/oberon_backend_hash.c
  ${SDK_ROOT}/components/libraries/crypto/backend/oberon/oberon_backend_hmac.c
  ${SDK_ROOT}/components/libraries/crypto/nrf_crypto_ecc.c
  ${SDK_ROOT}/components/libraries/crypto/nrf_crypto_ecdsa.c
  ${SDK_ROOT}/components/libraries/crypto/nrf_crypto_hash.c
  ${SDK_ROOT}/components/libraries/crypto/nrf_crypto_init.c
  ${SDK_ROOT}/components/libraries/crypto/nrf_crypto_shared.c
  ${SDK_ROOT}/components/libraries/experimental_section_vars/nrf_section_iter.c
  ${SDK_ROOT}/components/libraries/fstorage/nrf_fstorage.c
  ${SDK_ROOT}/components/libraries/fstorage/nrf_fstorage_nvmc.c
  ${SDK_ROOT}/components/libraries/fstorage/nrf_fstorage_sd.c
  ${SDK_ROOT}/components/libraries/log/src/nrf_log_frontend.c
  ${SDK_ROOT}/components/libraries/log/src/nrf_log_str_formatter.c
  ${SDK_ROOT}/components/libraries/mem_manager/mem_manager.c
  ${SDK_ROOT}/components/libraries/memobj/nrf_memobj.c
  ${SDK_ROOT}/components/libraries/queue/nrf_queue.c
  ${SDK_ROOT}/components/libraries/ringbuf/nrf_ringbuf.c
  ${SDK_ROOT}/components/libraries/scheduler/app_scheduler.c
  ${SDK_ROOT}/components/libraries/sha256/sha256.c
  ${SDK_ROOT}/components/libraries/strerror/nrf_strerror.c
  ${SDK_ROOT}/components/libraries/svc/nrf_svc_handler.c
  ${SDK_ROOT}/components/libraries/util/app_error_weak.c
  ${SDK_ROOT}/components/libraries/util/app_util_platform.c
  ${SDK_ROOT}/components/libraries/util/nrf_assert.c
  ${SDK_ROOT}/components/softdevice/common/nrf_sdh.c
  ${SDK_ROOT}/components/softdevice/common/nrf_sdh_ble.c
  ${SDK_ROOT}/components/softdevice/common/nrf_sdh_soc.c
  ${SDK_ROOT}/external/fprintf/nrf_fprintf.c
  ${SDK_ROOT}/external/fprintf/nrf_fprintf_format.c
  ${SDK_ROOT}/external/nano-pb/pb_common.c
  ${SDK_ROOT}/external/nano-pb/pb_decode.c
  ${SDK_ROOT}/modules/nrfx/hal/nrf_nvmc.c
  ${SDK_ROOT}/modules/nrfx/mdk/system_nrf52.c
  ${SDK_ROOT}/modules/nrfx/soc/nrfx_atomic.c
  )

# Bootloader+Application Memory Layout:
# https://infocenter.nordicsemi.com/index.jsp?topic=%2Fsdk_nrf5_v17.1.0%2Flib_bootloader.html&anchor=lib_bootloader_memory
#
# The general layout across nRF devices is that there is a block of memory for
# the application and the bootloader. Devices come with a default paritioning
# within this block, but we can configure it.
#
# After the partioning is set, we can't change it via a DFU over air, but we
# can change by reflashing the device. I increased the amount of memory
# allocated for the bootloader, because it was full with the bootloader image
# when build in debug mode.

set(NRF52832_MEM_BLOCK_START 0x26000)
set(NRF52832_BOOTLOADER_START ${BOOTLOADER_START_ADDR})
set(NRF52832_MEM_BLOCK_END 0x7E000)

set(NRF52840_MEM_BLOCK_START 0x27000)
set(NRF52840_BOOTLOADER_START ${BOOTLOADER_START_ADDR})
set(NRF52840_MEM_BLOCK_END 0xFE000)

add_nrf52832_library(nrf52832_bootloader
  ${NRF52832_BOOTLOADER_START}
  ${NRF52832_MEM_BLOCK_END}
  "${NRF5_BOOTLOADER_SRC_FILES}")
add_nrf52832_library(nrf52832_app
  ${NRF52832_MEM_BLOCK_START}
  ${NRF52832_BOOTLOADER_START}
  "${NRF5_SRC_FILES}")

add_nrf52840_library(nrf52840_bootloader
  ${NRF52840_BOOTLOADER_START}
  ${NRF52840_MEM_BLOCK_END}
  "${NRF5_BOOTLOADER_SRC_FILES}")
add_nrf52840_library(nrf52840_app
  ${NRF52840_MEM_BLOCK_START}
  ${NRF52840_BOOTLOADER_START}
  "${NRF5_SRC_FILES}")
