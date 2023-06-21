#include "globe.h"


enum fdoem_calibration {
    fdoem_calib_none,
    fdoem_o2_calib_0_pct,
    fdoem_o2_calib_100_pct,
    fdoem_ph_calib_low,
    fdoem_ph_calib_high,
    fdoem_ph_calib_offset,
};

static bool enable_temperature_calibration_characteristics = false;

struct app {
    struct app_config conf;
    FILE *active_log, *active_motion_log;

    struct timer sample_timer;
    struct timer fuel_gauge_calibration_timer;
    struct timer motion_service_timer;

    struct ble_char_string serial_number, dev_label;
    struct ble_char_float temperature, dissolved_oxygen, ph, peak_accel, rsoc,
        temperature_calib_0, temperature_calib_1, temperature_calib_2, pressure,
        salinity, reference_ph_char;
    struct ble_char_int ssbe_duty_cycle_char, firmware_version, sampling_period,
        logging_state, active, calibration, last_o2_calib, last_ph_calib;

    struct fdoem_o2_calibration_register_block o2_calib_regs;
    struct fdoem_ph_calibration_register_block ph_calib_regs;

    struct steinhart_hart_coeffs thermistor_coeffs;

    uint32_t motion_norm_max;

    int ssbe_duty_cycle;
    float reference_ph, last_calibration_reference_ph;
};

static void refresh_app(struct app *app);

static void stop_app_timers(struct app *app) {
    stop_timer(&app->motion_service_timer);
    stop_timer(&app->fuel_gauge_calibration_timer);
    stop_timer(&app->sample_timer);
}

static void try_to_log_dump(struct app *app) {
    /* The logs are only dumped if we are not currently logging so as to not
     * influence the data sampling. */
    if (!app->active_log && !app->active_motion_log) {
        stop_app_timers(app);
        log_dump();
        refresh_app(app);
    }
}

static void on_uart_connect(void *context) {
    struct app *app = context;
    try_to_log_dump(app);
}

static void on_uart_rx(void *context, const char *string) {
    (void)context;
    cJSON *request = cJSON_Parse(string);
    assert(request != NULL);

    cJSON *command_item = cJSON_GetObjectItemCaseSensitive(request, "command");
    assert(command_item != NULL);
    const char *command = cJSON_GetStringValue(command_item);
    assert(command != NULL);

    cJSON *response = cJSON_CreateObject();
    assert(response != NULL);

    if (strcmp(command, "getConfig") == 0) {
        assert(0 && "not implemented");
    } else if (strcmp(command, "setConfig") == 0) {
        assert(0 && "not implemented");
    } else if (strcmp(command, "echo") == 0) {
        cJSON *data_item = cJSON_GetObjectItemCaseSensitive(request, "data");
        assert(data_item != NULL);
        const char *data = cJSON_GetStringValue(data_item);
        assert(data != NULL);

        cJSON *response_data = cJSON_AddStringToObject(response, "data", data);
        assert(response_data != NULL);
    } else {
        assert(0);
    }

    cJSON_Delete(request);
    const char *response_string = cJSON_Print(response);
    cJSON_Delete(response);

    int send_response_return_code =
        ble_uart_send(response_string, strlen(response_string));
    assert(send_response_return_code == 0);

    free((char *)response_string);

    int send_newline_return_code = ble_uart_send("\n", 1);
    assert(send_newline_return_code == 0);
}

struct csv_printer {
    bool first;
    FILE *file;
};

static struct csv_printer init_csv_printer(FILE *f) {
    return (struct csv_printer){
        .first = true,
        .file = f,
    };
}

static void print_csv(struct csv_printer *p, float val) {
    if (!p->first) {
        fputs(",", p->file);
    }

    float abs = fabsf(val);

    if (abs < 0.1 && abs > 0) {
        fprintf(p->file, "%e", val);
    } else if (abs >= 1e6) {
        fprintf(p->file, "%e", val);
    } else {
        fprintf(p->file, "%.3f", val);
    }

    p->first = false;
}

static void print_csv_newline(struct csv_printer *p) {
    fprintf(p->file, "\n");
    p->first = true;
}

static void motion_service(struct app *app) {

    peripheral_require(PERIPH_MASK_COMM);
    struct motion_reading buf[MOTION_READ_MAX];
    size_t n = motion_read(buf, array_size(buf));

    for (size_t i = 0; i < n; i++) {
        const struct motion_reading a = buf[i];
        /* note the cast from int16 to int32 prior to multiplication to avoid
         * overflow. The squared values are divided by two prior to addition to
         * avoid overflow. */
#define MOTION_SQUARE(X) (uint32_t)(((int32_t)(X) * (int32_t)(X)) / 2)
        uint32_t norm = MOTION_SQUARE(a.x) + MOTION_SQUARE(a.y) + MOTION_SQUARE(a.z);
        app->motion_norm_max = max(app->motion_norm_max, norm);

        if (app->active_motion_log) {
            fprintf(app->active_motion_log, "%d,%d,%d\n", a.x, a.y, a.z);
        }
    }
}

static float motion_peak_accel(struct app *app) {
    motion_service(app); /* Service one last time */

    const float g_per_unit = 16.0 / (1U << 15);
    float res = sqrtf((float)(2.0f * g_per_unit * g_per_unit) * app->motion_norm_max);
    app->motion_norm_max = 0;
    return res;
}

static float external_temperature(struct app *app) {

    bool read_thermistor = app->conf.thermistor_enabled;

    if (read_thermistor) {
        peripheral_require(PERIPH_MASK_COMM | PERIPH_MASK_THERMISTOR);
        return thermistor_read(&app->thermistor_coeffs);
    } else {
        peripheral_require(PERIPH_MASK_COMM | PERIPH_MASK_MAX31856);
        return max31856_read();
    }
}

static void fdoem_update_ph_registers_from_config(
    struct fdoem_ph_calibration_register_block *blk, struct app_config *conf) {
    blk->pka = conf->pka_ph_calib;
    blk->pka_is1 = conf->pka_is1_calib;
    blk->pka_is2 = conf->pka_is2_calib;
}



static void make_sensor_readings(struct app *app) {
    periph_mask_t periph_support = board_peripheral_support();

    periph_mask_t active_periph_mask =
        (PERIPH_MASK_COMM                                      /* Required for comm */
         | (app->conf.motion_enabled ? PERIPH_MASK_BMX160 : 0) /* motion */
         | (app->conf.o2_enabled ? PERIPH_MASK_FDOEMO2 : 0)    /* oxygen */
         | (app->conf.microsens_enabled ? PERIPH_MASK_MCP3208 : 0) /* microsens */
         | (app->conf.pressure_enabled ? PERIPH_MASK_89BSD : 0)    /* pressure */
         | (app->conf.ph_enabled ? PERIPH_MASK_FDOEMPH : 0)        /* ph */
         ) &
        periph_support; /* Only supported devices may be active */

    peripheral_require(active_periph_mask);

    /* Make sensor readings */
    float rsoc = lc709203f_read_rsoc();
    float battery_voltage = 1.0e-3f * lc709203f_read_voltage();

    float external_temp = external_temperature(app);

    float internal_temp =
        (1.0f / BOARD_TEMPERATURE_UNITS_PER_DEGREE) * board_temperature();

    struct fdoem_readings fdoemo2_readings = {};
    if (app->conf.o2_enabled) {
        fdoemo2_readings =
            fdoem_read(fdoem_o2, external_temp, app->conf.salinity_g_per_l);
    }
    float oxygen_umol_per_L = 1.0e-3f * fdoemo2_readings.nmolar;
    float oxygen_mgram_per_L = 0.032f * oxygen_umol_per_L;

    float pH = 0.0f, dphi = 0;
    if (app->conf.ph_enabled) {
        struct fdoem_readings fdoemph_readings =
            fdoem_read(fdoem_ph, external_temp, app->conf.salinity_g_per_l);
        pH = 1.0e-3f * fdoemph_readings.ph;
        dphi = 1.0e-3f * fdoemph_readings.dphi;
    }

    float peak_accel = app->conf.motion_enabled ? motion_peak_accel(app) : 0.0f;

    struct msfet_reading msfet_readings[MSFET_N_CHANS] = {};

    if (app->conf.microsens_enabled) {
        for (int chan_id = 0; chan_id < MSFET_N_CHANS; chan_id++) {
            static const struct msfet_params msfet_params = {
                {{-0.0173, -2869.2, -0.4},   /* pH 1 */
                 {0.0196, -2121.3, -0.4},    /* K */
                 {0.0208, -2037.5480, -0.4}, /* Na */
                 {-0.0216, -1626.6, -0.4},   /* NO3 */
                 {-0.0173, -2869.2, -0.4}},  /* pH 2 */
            };
            msfet_readings[chan_id] = msfet_read(&msfet_params, chan_id, external_temp);
        }
    }

    struct _89bsd_reading pressure_reading = {};

    if (app->conf.pressure_enabled) {
        pressure_reading = _89bsd_read();
    }

    /* Update BLE sensor readings */
    set_ble_char_float(&app->temperature, external_temp);
    set_ble_char_float(&app->dissolved_oxygen, oxygen_mgram_per_L);
    set_ble_char_float(&app->rsoc, rsoc);
    set_ble_char_float(&app->ph, pH);
    set_ble_char_float(&app->peak_accel, peak_accel);
    set_ble_char_float(&app->pressure, pressure_reading.pressure);

    /* Update log file sensor readings */
    if (app->active_log) {
        struct csv_printer c = init_csv_printer(app->active_log);
        if (app->conf.o2_enabled)
            print_csv(&c, oxygen_umol_per_L);
        print_csv(&c, external_temp);
        print_csv(&c, internal_temp);
        if (app->conf.motion_enabled)
            print_csv(&c, peak_accel);
        print_csv(&c, rsoc);
        print_csv(&c, battery_voltage);
        if (app->conf.ph_enabled) {
            print_csv(&c, dphi);
            print_csv(&c, pH);
        }

        if (app->conf.microsens_enabled) {
            for (int chan_id = 0; chan_id < MSFET_N_CHANS; chan_id++) {
                print_csv(&c, msfet_readings[chan_id].adc_count);
                print_csv(&c, msfet_readings[chan_id].v_sg_mv);
                print_csv(&c, msfet_readings[chan_id].concentration);
            }
        }

        if (app->conf.pressure_enabled) {
            print_csv(&c, pressure_reading.d1);
            print_csv(&c, pressure_reading.d2);
            print_csv(&c, pressure_reading.temp);
            print_csv(&c, 1e3 * pressure_reading.pressure);
        }

        print_csv_newline(&c);
        flush_log(app->active_log);
    }
}

static void sample_timer_callback(struct timer *timer) {
    struct app *app = container_of(timer, struct app, sample_timer);
    make_sensor_readings(app);
}

static void ssbe_duty_cycle_update(struct ble_char_int *c, int64_t value) {
    struct app *app = container_of(c, struct app, ssbe_duty_cycle_char);
    app->ssbe_duty_cycle = value;
    refresh_app(app);
}

static void logging_state_update(struct ble_char_int *c, int64_t value) {
    struct app *app = container_of(c, struct app, logging_state);

    if (value) {
        set_epoch_ms(value);
    }
    app->conf.logging_enabled = value;
    save_app_config(&app->conf);
    refresh_app(app);
    try_to_log_dump(app);
}

static void active_update(struct ble_char_int *c, int64_t value) {
    struct app *app = container_of(c, struct app, active);

    if (value) {
        set_epoch_ms(value);
    }

    app->conf.active = (bool)value;
    save_app_config(&app->conf);
    refresh_app(app);
}

static void sampling_period_update(struct ble_char_int *c, int64_t value) {
    struct app *app = container_of(c, struct app, sampling_period);

    app->conf.log_period_seconds = value;
    save_app_config(&app->conf);
    refresh_app(app);
}

static void dev_label_update(struct ble_char_string *c, const char *value) {
    struct app *app = container_of(c, struct app, dev_label);

    size_t buf_sz = array_size(app->conf.device_label);
    strncpy(app->conf.device_label, value, buf_sz);
    app->conf.device_label[buf_sz - 1] = '\0';
    save_app_config(&app->conf);
    refresh_app(app);
}

static void firmware_version_update(struct ble_char_int *c, int64_t value) {
    (void)c;
    (void)value;
    reset_into_dfu_mode();
}

static void calibration_update(struct ble_char_int *c, int64_t value) {
    struct app *app = container_of(c, struct app, calibration);
    float temp = external_temperature(app);

    bool is_o2_calib =
        (value == fdoem_o2_calib_0_pct) || (value == fdoem_o2_calib_100_pct);
    bool is_ph_calib = (value == fdoem_ph_calib_low) ||
                       (value == fdoem_ph_calib_high) ||
                       (value == fdoem_ph_calib_offset);

    if (is_o2_calib) {
        peripheral_require(PERIPH_MASK_FDOEMO2 | PERIPH_MASK_COMM);
        fdoem_write_o2_calibration_register_block(&app->o2_calib_regs);
    }

    if (is_ph_calib) {
        peripheral_require(PERIPH_MASK_FDOEMPH | PERIPH_MASK_COMM);
        fdoem_write_ph_calibration_register_block(&app->ph_calib_regs);
    }
    switch (value) {
    case fdoem_calib_none:
        /* clear existing registers */
        if (app->conf.o2_enabled) {
            peripheral_require(PERIPH_MASK_FDOEMO2 | PERIPH_MASK_COMM);
            fdoem_read_o2_calibration_register_block(&app->o2_calib_regs);
        }
        if (app->conf.ph_enabled) {
            peripheral_require(PERIPH_MASK_FDOEMPH | PERIPH_MASK_COMM);
            fdoem_write_ph_calibration_register_block(&app->ph_calib_regs);
        }
        break;
    case fdoem_o2_calib_0_pct:
        fdoem_calibrate_o2_low(temp);
        break;
    case fdoem_o2_calib_100_pct:
        fdoem_calibrate_o2_high(temp);
        break;
    case fdoem_ph_calib_low: {
        float reference_ph = app->reference_ph > 0.0f ? app->reference_ph : 2.0f;
        fdoem_calibrate_ph_low(temp, reference_ph, app->conf.salinity_g_per_l);
        app->last_calibration_reference_ph = reference_ph;
        break;
    }
    case fdoem_ph_calib_high: {
        float reference_ph = app->reference_ph > 0.0f ? app->reference_ph : 11.0f;
        fdoem_calibrate_ph_high(temp, reference_ph, app->conf.salinity_g_per_l);
        app->last_calibration_reference_ph = reference_ph;
        break;
    }

    case fdoem_ph_calib_offset: {
        float reference_ph = app->reference_ph > 0.0f ? app->reference_ph : 8.0f;
        fdoem_calibrate_ph_offset(temp, reference_ph, app->conf.salinity_g_per_l);
        app->last_calibration_reference_ph = reference_ph;       
        break;
    }
    default:
        break;
    }
    refresh_app(app); // finalize current file and start logging after calibration
    int64_t fdoem_calib_status = (int64_t)fdoem_calib_none;
    if (is_o2_calib) {
        fdoem_read_o2_calibration_register_block(&app->o2_calib_regs);
        /* Restore original registers to device */
        fdoem_load_all_registers_from_flash(fdoem_o2);
    }

    if (is_ph_calib) {
        fdoem_write_ph_calibration_register_block(&app->ph_calib_regs);
        fdoem_load_all_registers_from_flash(fdoem_ph);
    }
    set_ble_char_int(&app->calibration, fdoem_calib_status);
}

static void temperature_calib_update(struct app *app, int *conf_therm_calib_T,
                                     int *conf_therm_calib_res,
                                     float temperature_value) {
    peripheral_require(PERIPH_MASK_THERMISTOR);
    *conf_therm_calib_T = 1.0e3 * temperature_value;
    *conf_therm_calib_res = thermistor_resistance_read();
    save_app_config(&app->conf);
    refresh_app(app);
}

static void temperature_calib_0_update(struct ble_char_float *c, float value) {
    struct app *app = container_of(c, struct app, temperature_calib_0);
    temperature_calib_update(app, &app->conf.therm_calib_T0_mdeg_c,
                             &app->conf.therm_calib_res0_ohm, value);
}

static void temperature_calib_1_update(struct ble_char_float *c, float value) {
    struct app *app = container_of(c, struct app, temperature_calib_1);
    temperature_calib_update(app, &app->conf.therm_calib_T1_mdeg_c,
                             &app->conf.therm_calib_res1_ohm, value);
}

static void temperature_calib_2_update(struct ble_char_float *c, float value) {
    struct app *app = container_of(c, struct app, temperature_calib_2);
    temperature_calib_update(app, &app->conf.therm_calib_T2_mdeg_c,
                             &app->conf.therm_calib_res2_ohm, value);
}

static void salinity_update(struct ble_char_float *c, float value) {
    struct app *app = container_of(c, struct app, salinity);
    app->conf.salinity_g_per_l = value;
    save_app_config(&app->conf);
}

static void reference_ph_update(struct ble_char_float *c, float value) {
    struct app *app = container_of(c, struct app, reference_ph_char);
    app->reference_ph = value;
}

static void last_o2_calib_update(struct ble_char_int *c, int64_t value) {
    struct app *app = container_of(c, struct app, last_o2_calib);

    peripheral_require(PERIPH_MASK_FDOEMO2 | PERIPH_MASK_COMM);

    fdoem_write_o2_calibration_register_block(&app->o2_calib_regs);
    fdoem_save_all_registers_to_flash(fdoem_o2);

    app->conf.last_o2_calib = value;
    save_app_config(&app->conf);
}

static void last_ph_calib_update(struct ble_char_int *c, int64_t value) {
    struct app *app = container_of(c, struct app, last_ph_calib);

    peripheral_require(PERIPH_MASK_FDOEMPH | PERIPH_MASK_COMM);

    fdoem_write_ph_calibration_register_block(&app->ph_calib_regs);
    fdoem_save_all_registers_to_flash(fdoem_ph);

    app->conf.last_ph_calib = value;
    save_app_config(&app->conf);
}

/* Halt logging on low charge. The battery voltage is used instead of RSOC.
 * Note that the voltage being read here is VBAT, not VCC. */
static void check_battery_voltage() {
    float vbat = 1.0e-3f * lc709203f_read_voltage();
    float vcutoff = 3.3 + 0.310; /* Add buffer based on regulator dropout */

    if (vbat < vcutoff) {
        assert(0 && "Critical battery voltage");
    }
}

static void fuel_gauge_calibration_callback(struct timer *timer) {
    (void)timer;
    check_battery_voltage();
    lc709203f_calibrate();
}

static void motion_service_callback(struct timer *timer) {
    struct app *app = container_of(timer, struct app, motion_service_timer);
    motion_service(app);
}

void handle_fault();

static void refresh_app(struct app *app) {

    /* Load calibration coefficients */
    struct steinhart_hart_calibration_data therm_calib = {{
        {1.0e-3f * app->conf.therm_calib_T0_mdeg_c, app->conf.therm_calib_res0_ohm},
        {1.0e-3f * app->conf.therm_calib_T1_mdeg_c, app->conf.therm_calib_res1_ohm},
        {1.0e-3f * app->conf.therm_calib_T2_mdeg_c, app->conf.therm_calib_res2_ohm},
    }};
    app->thermistor_coeffs = steinhart_hart_coeffs_calculate(&therm_calib);

    if (app->conf.force_dfu_mode) {
        reset_into_dfu_mode();
    }

    set_ble_char_string(&app->serial_number, device_id);
    set_ble_char_string(&app->dev_label, app->conf.device_label);
    set_ble_char_int(&app->firmware_version, revision_number);
    set_ble_char_int(&app->sampling_period, app->conf.log_period_seconds);
    set_ble_char_int(&app->logging_state, (int64_t)app->conf.logging_enabled);
    set_ble_char_int(&app->active, (int64_t)app->conf.active);
    set_ble_char_int(&app->calibration, 0);
    if (enable_temperature_calibration_characteristics) {
        set_ble_char_float(&app->temperature_calib_0,
                           therm_calib.calib_point_arr[0].temp_degrees_c);
        set_ble_char_float(&app->temperature_calib_1,
                           therm_calib.calib_point_arr[1].temp_degrees_c);
        set_ble_char_float(&app->temperature_calib_2,
                           therm_calib.calib_point_arr[2].temp_degrees_c);
    }
    set_ble_char_float(&app->salinity, app->conf.salinity_g_per_l);
    app->reference_ph = -1.0f;
    set_ble_char_float(&app->reference_ph_char, app->reference_ph);
    if (app->conf.o2_enabled)
        set_ble_char_int(&app->last_o2_calib, app->conf.last_o2_calib);
    if (app->conf.ph_enabled) {
        set_ble_char_int(&app->last_ph_calib, app->conf.last_ph_calib);
    }

    ble_update_label(app->conf.device_label);

    if (app->active_log) {
        fclose(app->active_log);
    }

    bool enable_logging = app->conf.logging_enabled && app->conf.active;

    if (!enable_logging) {
        app->active_log = NULL;
    } else {
        const struct log_column_def log_cols[] = {
            /* clang-format off */
            {app->conf.o2_enabled, "oxygen_umol_per_liter", "f", "umol/L"},
            {true, "temperature_degrees_celsius", "f", "degrees C"},
            {true, "internal_temperature_celsius", "f", "degrees C"},
            {app->conf.motion_enabled, "max_acceleration_g", "f", "g"},
            {true, "battery_rsoc_percent", "f", "percent"},
            {true, "battery_voltage_level_volts", "f", "V"},
            {app->conf.ph_enabled, "dphi", "f", "degrees"},
            {app->conf.ph_enabled, "pH", "f", "pH"},
            {app->conf.microsens_enabled, "nutrisens_ph_1_counts", "f", "na"},
            {app->conf.microsens_enabled, "nutrisens_ph_1_v_sg_mv", "f", "mV"},
            {app->conf.microsens_enabled, "nutrisens_ph_1", "f", "pH"},
            {app->conf.microsens_enabled, "nutrisens_potassium_ion_1_counts", "f", "na"},
            {app->conf.microsens_enabled, "nutrisens_potassium_ion_1_v_sg_mv", "f", "mV"},
            {app->conf.microsens_enabled, "nutrisens_potassium_ion_1_mol_per_liter", "f", "mol/L"},
            {app->conf.microsens_enabled, "nutrisens_sodium_ion_1_counts", "f", "na"},
            {app->conf.microsens_enabled, "nutrisens_sodium_ion_1_v_sg_mv", "f", "mV"},
            {app->conf.microsens_enabled, "nutrisens_sodium_ion_1_mol_per_liter", "f", "mol/L"},
            {app->conf.microsens_enabled, "nutrisens_ph_2_counts", "f", "na"},
            {app->conf.microsens_enabled, "nutrisens_ph_2_v_sg_mv", "f", "mV"},
            {app->conf.microsens_enabled, "nutrisens_ph_2", "f", "pH"},
            {app->conf.microsens_enabled, "nutrisens_nitrate_ion_1_counts", "f", "na"},
            {app->conf.microsens_enabled, "nutrisens_nitrate_ion_1_v_sg_mv", "f", "mV"},
            {app->conf.microsens_enabled, "nutrisens_nitrate_ion_1_mol_per_liter", "f", "mol/L"},
            {app->conf.pressure_enabled, "d1_counts", "f", "na"},
            {app->conf.pressure_enabled, "d2_counts", "f", "na"},
            {app->conf.pressure_enabled, "pressure_sensor_temperature_c", "f", "degrees C"},
            {app->conf.pressure_enabled, "pressure", "f", "mbarA"},
            /* clang-format on */
        };
        app->active_log = log_start(&app->conf, log_cols, array_size(log_cols),
                                    app->last_calibration_reference_ph);
        app->last_calibration_reference_ph = -1.0f;
    }

    if (app->active_motion_log) {
        fclose(app->active_motion_log);
    }

    bool enable_motion_logging =
        enable_logging && app->conf.log_motion_enabled && app->conf.motion_enabled;

    if (!enable_motion_logging) {
        app->active_motion_log = NULL;
    } else {
        const struct log_column_def log_cols[] = {
            /* clang-format off */
            {true, "accel_x", "f", "counts"},
            {true, "accel_y", "f", "counts"},
            {true, "accel_z", "f", "counts"},
            /* clang-format on */
        };
        app->active_motion_log = log_start(&app->conf, log_cols, array_size(log_cols),
                                           app->last_calibration_reference_ph);
    }

    periph_mask_t periph_support = board_peripheral_support();

    stop_app_timers(app);

    bool enable_motion_service = app->conf.motion_enabled && app->conf.active;

    if (enable_motion_service) {
        start_periodic_timer(&app->motion_service_timer, 1000 * 10,
                             motion_service_callback);
        peripheral_require((periph_support & PERIPH_MASK_BMX160) | PERIPH_MASK_COMM);
        motion_service(app);
    } else {
        peripheral_unrequire((periph_support & PERIPH_MASK_BMX160));
    }

    if (app->conf.active) {
        start_periodic_timer(&app->fuel_gauge_calibration_timer, 1000 * 60 * 10,
                             fuel_gauge_calibration_callback);
        check_battery_voltage();
        lc709203f_calibrate();

        start_periodic_timer(&app->sample_timer, 1000 * app->conf.log_period_seconds,
                             sample_timer_callback);
        make_sensor_readings(app);
    }

    if (app->conf.ssbe_enabled) {
        set_ssbe_duty_cycle(app->ssbe_duty_cycle);
    }
}

int main(void) {
    board_init();

    /* Ensure battery is above minimum voltage before continuing */
    for (;;) {
        /* Note: calibration is not required here as only voltage is read */
        float init_vbat = 1.0e-3f * lc709203f_read_voltage();
        float init_vbat_cutoff = 3.3 + 0.310 + 0.05;
        if (init_vbat >= init_vbat_cutoff)
            break;
        delay_ms(1000);
    }

    /* load config before opening log file to reduce number of concurrent open
     * files */

    struct app app = {.ssbe_duty_cycle = 0,
                      .reference_ph = -1.0f,
                      .last_calibration_reference_ph = -1.0f,
                      .conf = load_app_config()};

    lc709203f_init(app.conf.battery_capacity);

    DEBUG(app.conf.log_period_seconds, app.conf.ph_enabled, app.conf.motion_enabled,
          app.conf.device_label);

    if (app.conf.ssbe_enabled) {
        assert(!app.conf.ph_enabled);
        assert(!app.conf.o2_enabled);
        extern bool fdoems_plugged_in;
        fdoems_plugged_in = false;
        init_ble_char_int(&app.ssbe_duty_cycle_char, 0x92de, "Auxiliary 3",
                          ssbe_duty_cycle_update);
    }

    init_ble_char_string(&app.serial_number, 0x72c4, "Serial Number", NULL);
    init_ble_char_string(&app.dev_label, 0xa5dd, "Device Label", dev_label_update);
    init_ble_char_int(&app.firmware_version, 0x9c4d, "Firmware Version",
                      firmware_version_update);
    init_ble_char_int(&app.sampling_period, 0x92f3, "Sampling Period",
                      sampling_period_update);
    init_ble_char_int(&app.logging_state, 0x2c7b, "Logging State",
                      logging_state_update);
    init_ble_char_int(&app.active, 0x3f75, "Active", active_update);
    init_ble_char_int(&app.calibration, 0xe113, "Calibration", calibration_update);
    if (enable_temperature_calibration_characteristics) {
        init_ble_char_float(&app.temperature_calib_0, 0x3d3c,
                            "Temperature Calibration 0", temperature_calib_0_update);
        init_ble_char_float(&app.temperature_calib_1, 0xf912,
                            "Temperature Calibration 1", temperature_calib_1_update);
        init_ble_char_float(&app.temperature_calib_2, 0xe7cf,
                            "Temperature Calibration 2", temperature_calib_2_update);
    }
    init_ble_char_float(&app.salinity, 0x045d, "Salinity (g/L)", salinity_update);
    init_ble_char_float(&app.reference_ph_char, 0x4b89, "Reference pH",
                        reference_ph_update);

    init_ble_char_float(&app.temperature, 0x9eba, "Temperature", NULL);
    if (app.conf.o2_enabled) {
        init_ble_char_float(&app.dissolved_oxygen, 0x974f, "Dissolved Oxygen", NULL);

        peripheral_require(PERIPH_MASK_FDOEMO2 | PERIPH_MASK_COMM);
        fdoem_read_o2_calibration_register_block(&app.o2_calib_regs);
        init_ble_char_int(&app.last_o2_calib, 0x7fca, "Last O2 Calibration",
                          last_o2_calib_update);
    }
    if (app.conf.ph_enabled) {
        init_ble_char_float(&app.ph, 0x4b2f, "pH", NULL);
        peripheral_require(PERIPH_MASK_FDOEMPH | PERIPH_MASK_COMM);
        fdoem_read_ph_calibration_register_block(&app.ph_calib_regs);
        fdoem_update_ph_registers_from_config(&app.ph_calib_regs, &app.conf);
        fdoem_write_ph_calibration_register_block(&app.ph_calib_regs);
        fdoem_save_all_registers_to_flash(fdoem_ph);
        init_ble_char_int(&app.last_ph_calib, 0x8c8e, "Last pH Calibration",
                          last_ph_calib_update);
    }
    if (app.conf.motion_enabled)
        init_ble_char_float(&app.peak_accel, 0x2cd4, "Peak Acceleration", NULL);
    init_ble_char_float(&app.rsoc, 0x98ef, "Battery RSOC", NULL);
    if (app.conf.pressure_enabled) {
        init_ble_char_float(&app.pressure, 0x4e58, "Pressure", NULL);
    }

    app.active_log = NULL;
    app.active_motion_log = NULL;
    refresh_app(&app);

    start_ble(&app, app.conf.device_label, on_uart_connect, NULL, on_uart_rx);

    /* Handle fault after initialization to prevent excessive logging from
     * errors generated in init. */
    handle_fault();

    for (;;) {
        if (tasks_pending()) {
            handle_pending_tasks();

            if (app.conf.low_power_enabled) {
                peripheral_unrequire(PERIPH_MASK_PHONY_SENSOR);
            } else {
                peripheral_require(PERIPH_MASK_PHONY_SENSOR);
            }

            /* Can put everything to sleep except the motion sensor and the
             * phony sensor which facilitates disabling low power mode. */
            peripheral_unrequire((board_peripheral_support() & ~PERIPH_MASK_BMX160 &
                                  ~PERIPH_MASK_PHONY_SENSOR));
        }

        wait_for_event();
    }

    volatile int dummy = 0;
    if (dummy == 1) {
        /* This will never be evaluated, but the use of a volatile prevents the
         * compiler from removing this branch so functions can be kept in the
         * final executable and executed from the debugger. */

        void run_unit_tests();
        void run_stress_tests();

        run_unit_tests();
        run_stress_tests();
    }

    assert(0);

    return 0;
}
