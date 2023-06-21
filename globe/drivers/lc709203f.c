#include "globe.h"

static uint8_t crc8(const uint8_t *buf, size_t n) {
    uint8_t crc = 0x00;

    for (size_t i = 0; i < n; i++) {
        crc ^= buf[i];
        for (int bit = 8; bit > 0; bit--) {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x07;
            else
                crc = (crc << 1);
        }
    }
    return crc;
}

static uint16_t read_reg(uint8_t reg) {
    const struct i2c_device *dev = i2c_lc709203f();
    i2c_init(dev);
    i2c_write(dev, &reg, 1, true);
    /* read buf is 2 bytes of data (little endian), followed by 1 byte CRC8 (3
     * bytes total) */
    uint8_t buf[3];
    i2c_read(dev, buf, sizeof(buf));
    i2c_deinit(dev);

    uint8_t crc_buf[] = {
        0x16, reg, 0x17, buf[0], buf[1],
    };

    uint8_t crc = crc8(crc_buf, sizeof(crc_buf));
    assert(crc == buf[2]);

    return buf[0] + (buf[1] << 8);
}

static void write_reg(uint8_t reg, uint16_t value) {
    /* written data is 1 byte register, 2 bytes of data (little endian),
     * followed by 1 byte CRC8 (4 bytes total) */
    uint8_t buf[] = {
        0x16, reg, (uint8_t)(value & 0xff), (uint8_t)((value >> 8) & 0xff), 0,
    };
    buf[4] = crc8(buf, 4);

    const struct i2c_device *dev = i2c_lc709203f();
    i2c_init(dev);
    i2c_write(dev, &buf[1], 4, false);
    i2c_deinit(dev);
}

static int calculate_apa(int capacity_mah) {
    /* Table 7 of datasheet */
    static const struct {
        int capacity;
        int apa;
    } apa_table[] = {
        {0, 0x00},    {100, 0x08},  {200, 0x0B},  {500, 0x10},
        {1000, 0x19}, {2000, 0x2D}, {3000, 0x36},
    };

    int i;
    for (i = 1; i < (int)array_size(apa_table); i++) {
        if (capacity_mah <= apa_table[i].capacity) {
            break;
        }
    }
    assert(i < (int)array_size(apa_table));
    float c0 = apa_table[i - 1].capacity;
    float c1 = apa_table[i].capacity;
    float apa0 = apa_table[i - 1].apa;
    float apa1 = apa_table[i].apa;
    float c = capacity_mah;

    float apa = ((c - c0) / (c1 - c0)) * (apa1 - apa0) + apa0;

    return (int)(apa + 0.5); /* add 0.5 for even rounding */
}

void lc709203f_init(int capacity_mah) {
    /* See Figure 21 of the datasheet for initialization sequence */

    /* Battery profile */
    uint16_t number_of_the_parameter = read_reg(0x1a);

    /* The fuel gauge needs to be calibrated based on the battery type. Check
     * the datasheet for more details.
     *
     * It is assumed we are using a battery with 3.7V operating voltage
     * (Type-01).
     */
    uint16_t change_of_the_parameter;
    if (number_of_the_parameter == 0x0504) {
        change_of_the_parameter = 0x0000;
    } else if (number_of_the_parameter == 0x0301) {
        change_of_the_parameter = 0x0001;
    } else {
        assert(0);
    }

    /* Set APA based on Design Capacity of Battery. */
    uint16_t adjustment_pack_application = calculate_apa(capacity_mah);

    write_reg(0x0B, adjustment_pack_application);
    write_reg(0x12, change_of_the_parameter);

    lc709203f_calibrate();
}

int lc709203f_read_voltage() { return read_reg(0x09); }
int lc709203f_read_rsoc() { return read_reg(0x0D); }

void lc709203f_calibrate() {
    int board_temp = board_temperature();

    /* convert to .1C scale with offset */
    float zero_degrees = 0x0AAC;
    float s = 10.0f / (float)BOARD_TEMPERATURE_UNITS_PER_DEGREE;
    float temp = s * board_temp + zero_degrees;
    assert(temp >= 0);
    assert(temp <= 0xffff);
    write_reg(0x08, (uint16_t)temp);
}
