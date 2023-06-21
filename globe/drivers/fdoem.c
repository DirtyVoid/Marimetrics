#include "globe.h"

/* Max response size. MEA/WTM/RMR responses are large. */
#define RX_MAX_SIZE 255

static void parse_response(const char *rx_buf, int nargs, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int nmatched = vsscanf(rx_buf, format, args);
    assert(nmatched == nargs);
}

static void run_cmd_with_txbuf(enum fdoem_device device, const char *tx_buf,
                               size_t tx_buf_len, char *rx_buf, size_t rx_buf_len) {
    DEBUG(tx_buf);
    for (int attempts_remaining = 5;; attempts_remaining--) {
        assert(attempts_remaining > 0);

        size_t n_read = uart_fdoem_txrx(device, tx_buf, tx_buf_len, rx_buf, rx_buf_len);

        assert(n_read > 0);
        assert(n_read < RX_MAX_SIZE);
        rx_buf[n_read] = '\0';

        DEBUG(rx_buf);

        /* Resend commands that can fail due to COMM error. */
        int erro;
        if (sscanf(rx_buf, "#ERRO %d", &erro) == 1) {
            if (erro == -21 || erro == -22 || erro == -23) {
                continue;
            } else {
                assert(0 && "fdoem error");
            }
        }

        break;
    }
}

static void run_cmd_check_response_echo(enum fdoem_device device, const char *fmt,
                                        ...) {
#define FDOEM_RUN_CMD_WITH_VARIADIC_ARGS()                                             \
    char tx_buf[255];                                                                  \
    va_list args;                                                                      \
    va_start(args, fmt);                                                               \
    int tx_len = vsnprintf(tx_buf, sizeof(tx_buf), fmt, args);                         \
    assert(tx_len < (int)sizeof(tx_buf));                                              \
                                                                                       \
    run_cmd_with_txbuf(device, tx_buf, tx_len, rx_buf, rx_buf_len);

    char rx_buf[RX_MAX_SIZE];
    size_t rx_buf_len = array_size(rx_buf);
    FDOEM_RUN_CMD_WITH_VARIADIC_ARGS();

    parse_response(rx_buf, 0, tx_buf);
}

static void run_cmd(enum fdoem_device device, char *rx_buf, size_t rx_buf_len,
                    const char *fmt, ...) {
    FDOEM_RUN_CMD_WITH_VARIADIC_ARGS();
}

void fdoem_init(fdoem_device device) {

    /* The device is polled on initialization until an response is received. */
    char stat_cmd[] = "#IDNR\r"; /* Note: data in RAM */

    char rx_buf[RX_MAX_SIZE];

    size_t n_read = 0;
    for (int attempts_made = 0; n_read == 0; attempts_made++) {
        assert(attempts_made < 2);
        n_read = uart_fdoem_txrx(device, stat_cmd, strlen(stat_cmd), rx_buf,
                                 array_size(rx_buf));
    }

    assert(n_read < array_size(rx_buf));
    rx_buf[n_read] = '\0';
    DEBUG(rx_buf);

    /* Run again to ensure a valid response */
    int idnr;
    run_cmd(device, rx_buf, array_size(rx_buf), stat_cmd, &idnr);

    run_cmd(device, rx_buf, array_size(rx_buf), stat_cmd, &idnr);
}

struct fdoem_readings fdoem_read(fdoem_device device, float temp_c,
                                 float salinity_g_per_l) {
    char rx_buf[RX_MAX_SIZE];

    int temp = 1.0e3f * temp_c; /* mC */
    /* Pressure calculator (calculated for 5cm below sealevel):
     * http://www.calctool.org/CALC/other/games/depth_press */
    int pressure_ = 1.01828e6;             /* ubar */
    int salinity = 1e3 * salinity_g_per_l; /* mg/L */

    run_cmd_check_response_echo(device, "WTM 1 0 0 3 %d %d %d\r", temp, pressure_,
                                salinity);

    /* Make measurement. The available sensor types are provided as second
     * argument. The sensor types can be read with the #VER command. */
    int channel = 1;
    int enabled_sensors = 0x2f;
    run_cmd(device, rx_buf, array_size(rx_buf), "MEA %d %d\r", channel,
            enabled_sensors);

    struct fdoem_readings readings;

    int channel_rx;
    int enabled_sensors_rx;

    int status;
    int temp_sample;
    int temp_case;
    int signal_intensity;
    int ambient_light;
    int pressure;
    int humidity;
    int resistor_temp;
    int percent_o2;
    int ldev;
    int reserved[2];

    parse_response(
        rx_buf, 20, "MEA %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\r",
        &channel_rx, &enabled_sensors_rx, &status, &readings.dphi, &readings.nmolar,
        &readings.ubar, &readings.air_sat, &temp_sample, &temp_case, &signal_intensity,
        &ambient_light, &pressure, &humidity, &resistor_temp, &percent_o2,
        &readings.temp_optical, &readings.ph, &ldev, &reserved[0], &reserved[1]);

    assert(status == 0);

    return readings;
}

void fdoem_calibrate_o2_low(float calibration_temp) {
    int c = 1; /* optical channel */
    int t = calibration_temp * 1.0e3f;

    run_cmd_check_response_echo(fdoem_o2, "CLO %d %d\r", c, t);
}

void fdoem_calibrate_o2_high(float calibration_temp) {
    int c = 1;
    int t = calibration_temp * 1.0e3f;
    int p = 1.01828e6;  /* ubar */
    int h = 100 * 1000; /* 100% humidity because this is air-saturated
                         * water */

    run_cmd_check_response_echo(fdoem_o2, "CHI %d %d %d %d\r", c, t, p, h);
}

static void fdoem_calibrate_ph(int calib_point, float calib_temp, float calib_ph,
                               float calib_salinity) {
    int c = 1;
    int t = calib_temp * 1.0e3f;
    int s = 1e3 * calib_salinity;
    int ph = calib_ph * 1.0e3f;

    /* Clear calibration register prior to calibration */
    run_cmd_check_response_echo(fdoem_ph, "WTM %d 1 13 1 0\r", c);

    run_cmd_check_response_echo(fdoem_ph, "CPH %d %d %d %d %d\r", c, calib_point, ph, t,
                                s);
}

void fdoem_calibrate_ph_low(float calib_temp, float calib_ph, float calib_salinity) {
    fdoem_calibrate_ph(0, calib_temp, calib_ph, calib_salinity);
}
void fdoem_calibrate_ph_high(float calib_temp, float calib_ph, float calib_salinity) {
    fdoem_calibrate_ph(1, calib_temp, calib_ph, calib_salinity);
}
void fdoem_calibrate_ph_offset(float calib_temp, float calib_ph, float calib_salinity) {
    fdoem_calibrate_ph(2, calib_temp, calib_ph, calib_salinity);
}
static void fdoem_read_register_block(fdoem_device device, int t, int r, int n,
                                      int *buffer) {
    size_t rx_buf_size = 255;
    char *rx_buf = malloc(rx_buf_size);
    assert(rx_buf);

    int c = 1;

    run_cmd(device, rx_buf, rx_buf_size, "RMR %d %d %d %d\r", c, t, r, n);

    const char *ptr = rx_buf + strlen("RMR ");
    assert((int)strlen(rx_buf) > ptr - rx_buf);

    /* skip echoed parameters */
    for (int i = 0; i < 4; i++) {
        char *end_ptr;
        strtol(ptr, &end_ptr, 10);
        assert(ptr != end_ptr);
        ptr = end_ptr;
    }

    for (int i = 0; i < n; i++) {
        char *end_ptr;
        *buffer++ = strtol(ptr, &end_ptr, 10);
        assert(ptr != end_ptr);
        ptr = end_ptr;
    }

    free(rx_buf);
}

static void fdoem_write_register_block(fdoem_device device, int t, int r, int n,
                                       const int *buffer) {
    size_t tx_buf_size = 255;
    char *tx_buf = malloc(tx_buf_size);
    assert(tx_buf);

    char *tx_buf_end = tx_buf + tx_buf_size;

    int c = 1;

    char *ptr = tx_buf;
    int n_print = snprintf(ptr, tx_buf_end - ptr, "WTM %d %d %d %d", c, t, r, n);
    assert(n_print > 0);
    ptr += n_print;

    for (int i = 0; i < n; i++) {
        n_print = snprintf(ptr, tx_buf_end - ptr, " %d", buffer[i]);
        assert(n_print > 0);
        ptr += n_print;
    }

    snprintf(ptr, tx_buf_end - ptr, "\r");
    assert(n_print > 0);
    ptr += n_print;

    run_cmd_check_response_echo(device, tx_buf);

    free(tx_buf);
}

void fdoem_read_o2_calibration_register_block(
    struct fdoem_o2_calibration_register_block *block) {
    fdoem_read_register_block(fdoem_o2, 1, 0, sizeof(*block) / sizeof(int),
                              (int *)block);
}

void fdoem_read_ph_calibration_register_block(
    struct fdoem_ph_calibration_register_block *block) {
    fdoem_read_register_block(fdoem_ph, 1, 0, sizeof(*block) / sizeof(int),
                              (int *)block);
}

void fdoem_write_o2_calibration_register_block(
    const struct fdoem_o2_calibration_register_block *block) {
    fdoem_write_register_block(fdoem_o2, 1, 0, sizeof(*block) / sizeof(int),
                               (const int *)block);
}

void fdoem_write_ph_calibration_register_block(
    const struct fdoem_ph_calibration_register_block *block) {
    fdoem_write_register_block(fdoem_ph, 1, 0, sizeof(*block) / sizeof(int),
                               (const int *)block);
}


void fdoem_save_all_registers_to_flash(fdoem_device device) {
    int c = 1;
    run_cmd_check_response_echo(device, "SVS %d\r", c);
}

void fdoem_load_all_registers_from_flash(fdoem_device device) {
    int c = 1;
    run_cmd_check_response_echo(device, "LDS %d\r", c);
}
