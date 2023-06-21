#ifndef CONFIG_H
#define CONFIG_H

#include "globe.h"

typedef char config_string[32];
typedef bool config_bool;

/* last_ph_calib/last_o2_calib: The calibration time is stored in MS since
 * epoch. If 0 (default), then a calibration was never performed. */

/* force_dfu_mode: Request DFU mode on startup. For DFU development. */

/* format: type, name, string, default */
#define CONFIG_EXPANSION(M)                                                            \
    M(int, log_period_seconds, "loggingPeriod", 60)                                    \
    M(config_bool, o2_enabled, "o2Enabled", true)                                      \
    M(config_bool, ph_enabled, "phEnabled", false)                                     \
    M(config_bool, motion_enabled, "motionEnabled", true)                              \
    M(config_bool, microsens_enabled, "auxiliaryEnabled", false)                       \
    M(config_bool, low_power_enabled, "lowPowerEnabled", true)                         \
    M(config_bool, pressure_enabled, "auxiliary2Enabled", false)                       \
    M(config_bool, ssbe_enabled, "auxiliary3Enabled", false)                           \
    M(config_bool, logging_enabled, "loggingEnabled", false)                           \
    M(config_bool, active, "active", true)                                             \
    M(config_string, device_label, "deviceLabel", device_id)                           \
    M(float, salinity_g_per_l, "salinityGperL", 28.0f)                                 \
    M(epoch_t, last_ph_calib, "lastPhCalib", 0)                                        \
    M(epoch_t, last_o2_calib, "lastO2Calib", 0)                                        \
    M(config_bool, force_dfu_mode, "forceDFUMode", false)                              \
    M(config_bool, thermistor_enabled, "thermistorEnabled", true)                      \
    M(int, battery_capacity, "batteryCapacity", 2500)                                  \
    M(int, therm_calib_T0_mdeg_c, "thermCalibT0mDegC", 0)                              \
    M(int, therm_calib_res0_ohm, "thermCalibRes0Ohm", 28224)                           \
    M(int, therm_calib_T1_mdeg_c, "thermCalibT1mDegC", 15000)                          \
    M(int, therm_calib_res1_ohm, "thermCalibRes1Ohm", 14820)                           \
    M(int, therm_calib_T2_mdeg_c, "thermCalibT2mDegC", 25000)                          \
    M(int, therm_calib_res2_ohm, "thermCalibRes2Ohm", 10000)                           \
    M(config_bool, log_motion_enabled, "logMotionEnabled", false)                      \
    M(int, pka_ph_calib, "pkaPhCalib", 8050)                                           \
    M(int, pka_is1_calib, "pkaIsOne", 2540000)                                         \
    M(int, pka_is2_calib, "pkaIsTwo", 250000)

struct app_config {
#define APP_CONFIG_MEMBER_EXPANSION(type, name, string, default) type name;
    CONFIG_EXPANSION(APP_CONFIG_MEMBER_EXPANSION)
};

cJSON *app_config_to_json(const struct app_config *config);
struct app_config app_config_from_json(cJSON *json);

cJSON *load_app_config_json();
struct app_config load_app_config();
void save_app_config(const struct app_config *config);
cJSON *load_app_config_json();

#endif
