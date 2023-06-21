#include "globe.h"

#define PROM_LENGTH 7

static void _89bsd_read_prom(uint8_t *output_buf) {

    const struct i2c_device *dev = i2c_89bsd();

    i2c_init(dev);

    for (int i = 0; i < PROM_LENGTH; i++) {

        uint8_t register_address = 0xA0 + 2 * (i + 1);

        i2c_write(dev, &register_address, 1, true);
        i2c_read(dev, &output_buf[2 * i], 2);
    }

    i2c_deinit(dev);
}

static void _89bsd_reset() {

    const struct i2c_device *dev = i2c_89bsd();

    i2c_init(dev);

    uint8_t data[] = {0x1E};
    i2c_write(dev, data, sizeof(data), false);

    i2c_deinit(dev);

    delay_ms(10);
}

static int _89bsd_run_conversion_sequence(uint8_t conversion_command) {

    const struct i2c_device *dev = i2c_89bsd();

    i2c_init(dev);

    i2c_write(dev, &conversion_command, 1, false);

    delay_ms(10);
    uint8_t adc_read_command = 0x00;
    i2c_write(dev, &adc_read_command, 1, true);
    uint8_t buf[3] = {};
    i2c_read(dev, buf, 3);

    i2c_deinit(dev);
    return (buf[0] << 16) | (buf[1] << 8) | buf[2];
}

static int extract_int(const uint8_t *ptr, int msb_idx, int length) {

    uint32_t x = ((uint32_t)ptr[3] << 0) + ((uint32_t)ptr[2] << 8) +
                 ((uint32_t)ptr[1] << 16) + ((uint32_t)ptr[0] << 24);

    int32_t unsigned_value = x >> (msb_idx - length - 1);
    int32_t mask = (1 << length) - 1;
    int32_t zero_offset = 1 << (length - 1);

    int signed_value = ((unsigned_value + zero_offset) & mask) - zero_offset;

    return signed_value;
}

static struct _89bsd_params extract_parameters_from_prom(const uint8_t *prom) {
    return (struct _89bsd_params){
        .c0 = extract_int(prom + 2 * (1 - 1), 15 + 18, 14),
        .c1 = extract_int(prom + 2 * (1 - 1), 1 + 18, 14),
        .c2 = extract_int(prom + 2 * (2 - 1), 3 + 18, 10),
        .c3 = extract_int(prom + 2 * (3 - 1), 9 + 18, 10),
        .c4 = extract_int(prom + 2 * (4 - 1), 15 + 18, 10),
        .c5 = extract_int(prom + 2 * (4 - 1), 5 + 18, 10),
        .c6 = extract_int(prom + 2 * (5 - 1), 11 + 18, 10),
        .a0 = extract_int(prom + 2 * (5 - 1), 1 + 18, 10),
        .a1 = extract_int(prom + 2 * (6 - 1), 7 + 18, 10),
        .a2 = extract_int(prom + 2 * (6 - 1), 15, 10),
    };
}

struct _89bsd_params _89bsd_params;

void _89bsd_init() {
    _89bsd_reset();
    uint8_t prom[2 * PROM_LENGTH];
    _89bsd_read_prom(prom);
    _89bsd_params = extract_parameters_from_prom(prom);
}

static int _89bsd_read_d1() { return _89bsd_run_conversion_sequence(0x48); }

static int _89bsd_read_d2() { return _89bsd_run_conversion_sequence(0x58); }

static float two_pow(int i) { return 1 << i; }

static float pow2(float x) { return x * x; }

static struct _89bsd_reading _89bsd_calculate_readings(const struct _89bsd_params *p,
                                                       int d1, int d2) {

    float temp = p->a0 / 3.0 + p->a1 * 2.0 * d2 / two_pow(24) +
                 p->a2 * 2 * pow2(d2 / two_pow(24));

    float q0 = 9;
    float q1 = 11;
    float q2 = 9;
    float q3 = 15;
    float q4 = 15;
    float q5 = 16;
    float q6 = 16;

    float y = (d1 + p->c0 * two_pow(q0) + p->c3 * two_pow(q3) * d2 / two_pow(24) +
               p->c4 * two_pow(q4) * pow2(d2 / two_pow(24))) /
              (p->c1 * two_pow(q1) + p->c5 * two_pow(q5) * d2 / two_pow(24) +
               p->c6 * two_pow(q6) * pow2(d2 / two_pow(24)));

    float p_ = y * (1 - p->c2 * two_pow(q2) / two_pow(24)) +
               p->c2 * two_pow(q2) / two_pow(24) * pow2(y);

    float p_min = 0.0;
    float p_max = 6.0; /* product dependant */

    float pressure = (p_ - 0.1) / 0.8 * (p_max - p_min) + p_min;

    return (struct _89bsd_reading){
        .d1 = d1,
        .d2 = d2,
        .temp = temp,
        .y = y,
        .p = p_,
        .pressure = pressure,
    };
}

struct _89bsd_reading _89bsd_read() {
    int d1 = _89bsd_read_d1();
    int d2 = _89bsd_read_d2();

    return _89bsd_calculate_readings(&_89bsd_params, d1, d2);
}
