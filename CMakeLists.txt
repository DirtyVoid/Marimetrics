cmake_minimum_required(VERSION 3.13.4)
cmake_policy(SET CMP0048 NEW)

project(firmware LANGUAGES C ASM)

set(GLOBE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/globe")

include(${GLOBE_DIR}/third_party/arm-cmake-toolchains/utils.cmake)

# The git_revision library contains a string describing the current git state.
#
# The implementation ensures that the string is checked during each build but
# the library is only recompiled if it changes.

find_package(Git)

set(GIT_REVISION_INC_DIR "${CMAKE_CURRENT_BINARY_DIR}")
set(GIT_REVISION_FILE "${GIT_REVISION_INC_DIR}/git_revision.h")

# Toolchain Configuration
set(CMAKE_C_FLAGS_DEBUG "-O0 -ggdb3")
set(CMAKE_C_FLAGS_RELEASE "-O3 -ggdb3")
set(CMAKE_C_FLAGS_MINSIZEREL "-Os -ggdb3")
set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")

# Generic embedded flags
set(CMAKE_C_FLAGS "-ffile-prefix-map=${PROJECT_SOURCE_DIR}/= -ffunction-sections -fdata-sections -Wno-psabi -fno-builtin -fshort-enums --specs=nano.specs")
set(CMAKE_EXE_LINKER_FLAGS "-Wl,-gc-sections --specs=nosys.specs")

# Flags for nRF52
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mthumb -mabi=aapcs -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16")

# Other language flags
set(CMAKE_ASM_FLAGS "${CMAKE_C_FLAGS}")

add_subdirectory(${GLOBE_DIR}/third_party)

set(USE_DEBUG_DFU_KEY OFF CACHE BOOL "")
set(STATIC_BUILD_SETTINGS OFF CACHE BOOL "")

add_library(build_settings
  ${CMAKE_CURRENT_BINARY_DIR}/cmake_strings.c
  ${GLOBE_DIR}/misc/git_revision.c
  )
target_compile_definitions(build_settings PRIVATE
  STATIC_BUILD_SETTINGS=$<IF:$<BOOL:${STATIC_BUILD_SETTINGS}>,1,0>)
add_custom_target(build_settings_defs
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/dev-utils generate-build-settings ${GIT_REVISION_FILE})
add_dependencies(build_settings build_settings_defs)
target_include_directories(build_settings PRIVATE ${GIT_REVISION_INC_DIR})

configure_file(${GLOBE_DIR}/misc/cmake_strings.c.in cmake_strings.c @ONLY)

add_executable(application
  ${GLOBE_DIR}/app/config.c
  ${GLOBE_DIR}/app/error_handling.c
  ${GLOBE_DIR}/app/logging.c
  ${GLOBE_DIR}/app/main.c
  ${GLOBE_DIR}/app/peripheral_management.c
  ${GLOBE_DIR}/board/adc.c
  ${GLOBE_DIR}/board/advertising.c
  ${GLOBE_DIR}/board/ble.c
  ${GLOBE_DIR}/board/board_test.c
  ${GLOBE_DIR}/board/clock.c
  ${GLOBE_DIR}/board/connection_parameters.c
  ${GLOBE_DIR}/board/critical_section.c
  ${GLOBE_DIR}/board/delay.c
  ${GLOBE_DIR}/board/device_id.c
  ${GLOBE_DIR}/board/features.c
  ${GLOBE_DIR}/board/i2c.c
  ${GLOBE_DIR}/board/init.c
  ${GLOBE_DIR}/board/nus.c
  ${GLOBE_DIR}/board/peer_manager.c
  ${GLOBE_DIR}/board/peripheral.c
  ${GLOBE_DIR}/board/power_management.c
  ${GLOBE_DIR}/board/pwm.c
  ${GLOBE_DIR}/board/spi.c
  ${GLOBE_DIR}/board/stack.c
  ${GLOBE_DIR}/board/system_reset.c
  ${GLOBE_DIR}/board/temperature.c
  ${GLOBE_DIR}/board/timer.c
  ${GLOBE_DIR}/board/uart.c
  ${GLOBE_DIR}/drivers/89bsd.c
  ${GLOBE_DIR}/drivers/fdoem.c
  ${GLOBE_DIR}/drivers/lc709203f.c
  ${GLOBE_DIR}/drivers/max31856.c
  ${GLOBE_DIR}/drivers/mcp3208.c
  ${GLOBE_DIR}/drivers/microsens_msfet.c
  ${GLOBE_DIR}/drivers/motion.c
  ${GLOBE_DIR}/drivers/sdcard.c
  ${GLOBE_DIR}/drivers/sky66112.c
  ${GLOBE_DIR}/drivers/thermistor.c
  ${GLOBE_DIR}/misc/debug.c
  ${GLOBE_DIR}/misc/escape_string.c
  ${GLOBE_DIR}/misc/filesystem/fatfs_bindings.c
  ${GLOBE_DIR}/misc/filesystem/filesystem.c
  ${GLOBE_DIR}/misc/filesystem/newlib_bindings.c
  ${GLOBE_DIR}/misc/malloc_lock.c
  ${GLOBE_DIR}/misc/task.c
  ${GLOBE_DIR}/test/tests.c
  "$<IF:$<BOOL:${USE_DEBUG_DFU_KEY}>,${GLOBE_DIR}/bootloader/debug_pk.c,${GLOBE_DIR}/bootloader/pk.c>"
  )
target_include_directories(application PRIVATE ${GLOBE_DIR})

set(HARDWARE_TYPE "nrf52832" CACHE STRING "")
set(APP "${HARDWARE_TYPE}_app")
set(BOOTLOADER "${HARDWARE_TYPE}_bootloader")
set(BOOTLOADER_START_ADDR "0x77000" CACHE STRING "")

target_link_libraries(application ${APP} bmi160 fatfs cjson boost_preprocessor build_settings)
target_compile_options(application PUBLIC -Wall -Wextra)
target_link_options(application PUBLIC -u _printf_float)
generate_object(application .hex ihex)

add_executable(bootloader ${GLOBE_DIR}/bootloader/bootloader_main.c
  "$<IF:$<BOOL:${USE_DEBUG_DFU_KEY}>,${GLOBE_DIR}/bootloader/debug_pk.c,${GLOBE_DIR}/bootloader/pk.c>")

target_link_libraries(bootloader "${BOOTLOADER}")
target_compile_options(bootloader PRIVATE -Wall -Wextra)
generate_object(bootloader .hex ihex)

add_custom_target(lint)
add_custom_command(TARGET lint
  COMMAND find ${GLOBE_DIR}/src -name *.[ch] | xargs clang-format --Werror --dry-run
  VERBATIM)
