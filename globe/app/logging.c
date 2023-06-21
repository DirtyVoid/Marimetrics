#include "globe.h"

int syncfs(int fd);

static int dump_file(FILE *fil) {
    char data[128];

    ssize_t nread;
    while ((nread = fread(data, 1, sizeof(data), fil)) > 0) {
        if (ble_uart_send(data, nread)) {
            return 1;
        }
    }

    strcpy(data, "\b"); // terminating character
    if (ble_uart_send(data, strlen(data)))
        return 1;

    if (ble_uart_sync())
        return 1;

    return 0;
}

static void check_fresult(FRESULT res) { assert(res == FR_OK); }

/* Dump data over UART. */
void log_dump() {
    DIR dir;
    FILINFO fno;
    char file_name[32];

    peripheral_require(PERIPH_MASK_COMM | PERIPH_MASK_SD_CARD);

    FRESULT open_res = f_opendir(&dir, "/logs");
    if (open_res == FR_NO_PATH) {
        /* No log files produced yet, so the log directory has not yet been
         * created. */
        return;
    }
    check_fresult(open_res);
    /* all items in this directory should be valid log files */
    for (;;) {
        check_fresult(f_readdir(&dir, &fno));
        if (fno.fname[0] == '\0')
            /* Finished reading directory */
            break;

        snprintf(file_name, sizeof(file_name), "/logs/%s", fno.fname);

        FILE *fil = fopen(file_name, "r");
        assert(fil);
        int err = dump_file(fil);
        fclose(fil);

        if (err) {
            break;
        }

        /* delete the file if transmission succeeded */
        check_fresult(f_unlink(file_name));
    }

    f_closedir(&dir);
}

static void print_header_line(FILE *active_log, const struct log_column_def *defs,
                              size_t n_cols, const char *header_name,
                              size_t field_offset) {
    fprintf(active_log, "#%s=", header_name);
    bool first = true;
    for (size_t i = 0; i < n_cols; i++) {
        const struct log_column_def *col = &defs[i];
        if (!col->enabled)
            continue;
        /* Optionally print a comma seperator and get the field name string
         * based on the offset into the structure */
        fprintf(active_log, "%s%s", first ? "" : ",",
                *(const char **)((const char *)col + field_offset));
        first = false;
    }
    fprintf(active_log, "\n");
}

void flush_log(FILE *active_log) {
    int err = fflush(active_log);
    assert(!err);
    err = syncfs(fileno(active_log));
    assert(!err);
}

FILE *log_start(const struct app_config *conf, const struct log_column_def *cols,
                size_t n_cols, float last_calibration_reference_ph) {

    FRESULT mkdir_res = f_mkdir("logs");
    if (mkdir_res != FR_EXIST)
        check_fresult(mkdir_res);

    /* find the lowest numbered available log file */
    FILE *active_log = NULL;
    for (int idx = 0; !active_log; idx++) {
        assert(idx < 100);
        char file_name[32];
        snprintf(file_name, sizeof(file_name), "/logs/data_%d.csv", idx);
        active_log = fopen(file_name, "wx");
        assert(active_log || errno == EEXIST);
    }

    if (is_epoch_set()) {
        struct tm tm;

        epoch_t epoch_start = get_epoch_ms();
        time_t epoch_sec = epoch_start / 1000;

        gmtime_r(&epoch_sec, &tm);

        int year = tm.tm_year + 1900;
        int month = tm.tm_mon + 1;
        int day = tm.tm_mday;
        int hour = tm.tm_hour;
        int min = tm.tm_min;
        int sec = tm.tm_sec;
        int ms = epoch_start % 1000;

        /* write the header */
        fprintf(active_log, "#FILE_START_TIME_UTC=%d/%02d/%02dT%02d:%02d:%02d.%03d\n",
                year, month, day, hour, min, sec, ms);
    }
  
    fprintf(active_log, "#DATA_FILE_REVISION=C\n");
    fprintf(active_log, "#DELIMITER=,\n");
    fprintf(active_log, "#FIRMWARE_REVISION=%s\n", revision);
    fprintf(active_log, "#FIRMWARE_BUILD_TYPE=%s\n", build_type);
    fprintf(active_log, "#FIRMWARE_APPLICATION=%s\n", application);
    fprintf(active_log, "#DEVICE_ID=%s\n", device_id);
    fprintf(active_log, "#LABEL=%s\n", conf->device_label);
    fprintf(active_log, "#SAMPLE_PERIOD_SECONDS=%d\n", conf->log_period_seconds);
    fprintf(active_log, "#MAXIMUM_STACK_USAGE=%d\n", (int)stack_maximum_usage());
    fprintf(active_log, "#STACK_CAPACITY=%d\n", (int)stack_capacity());
    fprintf(active_log, "#RESET_CAUSE=%s\n", reset_cause());

    if (conf->pressure_enabled) {
        /* To add the pressure data to the log file header, the pressure sensor
         * must first be initialized */
        peripheral_require(PERIPH_MASK_COMM | PERIPH_MASK_89BSD);
        fprintf(active_log, "#param_89BSD_C0=%d\n", (int)_89bsd_params.c0);
        fprintf(active_log, "#param_89BSD_C1=%d\n", (int)_89bsd_params.c1);
        fprintf(active_log, "#param_89BSD_C2=%d\n", (int)_89bsd_params.c2);
        fprintf(active_log, "#param_89BSD_C3=%d\n", (int)_89bsd_params.c3);
        fprintf(active_log, "#param_89BSD_C4=%d\n", (int)_89bsd_params.c4);
        fprintf(active_log, "#param_89BSD_C5=%d\n", (int)_89bsd_params.c5);
        fprintf(active_log, "#param_89BSD_C6=%d\n", (int)_89bsd_params.c6);

        fprintf(active_log, "#param_89BSD_A0=%d\n", (int)_89bsd_params.a0);
        fprintf(active_log, "#param_89BSD_A1=%d\n", (int)_89bsd_params.a1);
        fprintf(active_log, "#param_89BSD_A2=%d\n", (int)_89bsd_params.a2);
    }
    if (conf->ph_enabled) {
        peripheral_require(PERIPH_MASK_COMM | PERIPH_MASK_FDOEMPH);
        struct fdoem_ph_calibration_register_block blk;
        fdoem_read_ph_calibration_register_block(&blk);
        fprintf(active_log, "#LAST_CALIBRATION_REFERENCE_PH=%0.3f\n",
                last_calibration_reference_ph);
        fprintf(active_log, "#FDOEM_PH_OFFSET_PH=%0.3f\n", 1.0e-03f * blk.offset);
        fprintf(active_log, "#FDOEM_PH_DPHI1_DEGREES=%0.3f\n", 1.0e-03f * blk.dPhi1);
        fprintf(active_log, "#FDOEM_PH_DPHI2_DEGREES=%0.3f\n", 1.0e-03f * blk.dPhi2);
        fprintf(active_log, "#FDOEM_PH_PKA_PH=%0.3f\n", 1.0e-03f * blk.pka);
        fprintf(active_log, "#FDOEM_PH_SLOPE=%0.3f\n", 1.0e-06f * blk.slope);
        fprintf(active_log, "#FDOEM_PH_BOTTOM_T_1_K=%0.6f\n", 1.0e-06f * blk.bottom_t);
        fprintf(active_log, "#FDOEM_PH_DYN_T_1_K=%0.6f\n", 1.0e-06f * blk.dyn_t);
        fprintf(active_log, "#FDOEM_PH_PKA_T_PH_K=%0.4f\n", 1.0e-06f * blk.pka_t);
        fprintf(active_log, "#FDOEM_PH_PKA_IS1=%0.2f\n", 1.0e-06f * blk.pka_is1);
        fprintf(active_log, "#FDOEM_PH_PKA_IS2=%0.2f\n", 1.0e-06f * blk.pka_is2);
    }

    if (conf->thermistor_enabled) {

        fprintf(active_log, "#THERM_CALIB_T0_mDEG_C=%d\n", conf->therm_calib_T0_mdeg_c);
        fprintf(active_log, "#THERM_CALIB_T1_mDEG_C=%d\n", conf->therm_calib_T1_mdeg_c);
        fprintf(active_log, "#THERM_CALIB_T2_mDEG_C=%d\n", conf->therm_calib_T2_mdeg_c);
        fprintf(active_log, "#THERM_CALIB_RES0_OHM=%d\n", conf->therm_calib_res0_ohm);
        fprintf(active_log, "#THERM_CALIB_RES1_OHM=%d\n", conf->therm_calib_res1_ohm);
        fprintf(active_log, "#THERM_CALIB_RES2_OHM=%d\n", conf->therm_calib_res2_ohm);
        fprintf(active_log, "#THERM_REFERENCE_VOLTAGE_VOLTS=%0.1f\n", 3.3);
        fprintf(active_log, "#THERM_REFERENCE_RESISTANCE_kOHM=%0.1f\n", 6.2);
    }

    /* Print debug to header */
    int debug_line = 0;
    bool last_char_was_newline = true;
    for (const char *p = reset_debug_info(); *p; ++p) {
        if (last_char_was_newline) {
            fprintf(active_log, "#DEBUG_%02d=", debug_line);
            debug_line++;
        }
        fputc(*p, active_log);
        last_char_was_newline = (*p == '\n');
    }
    if (!last_char_was_newline)
        fputc('\n', active_log);

    print_header_line(active_log, cols, n_cols, "FIELD_NAMES",
                      offsetof(struct log_column_def, field_name));
    print_header_line(active_log, cols, n_cols, "FIELD_DATA_TYPES",
                      offsetof(struct log_column_def, data_type));
    print_header_line(active_log, cols, n_cols, "FIELD_UNITS",
                      offsetof(struct log_column_def, units));

    flush_log(active_log);

    return active_log;
}
