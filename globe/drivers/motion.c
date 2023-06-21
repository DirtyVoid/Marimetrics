#include "globe.h"

static bool spi_interface_supported() {
    if (board_peripheral_support() & PERIPH_MASK_BMX160_SPI)
        return true;
    assert(board_peripheral_support() & PERIPH_MASK_BMX160_I2C);
    return false;
}

static int8_t reg_read(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data,
                       uint16_t len) {
    assert(dev_addr == 0);

    /* DATASHEET: an interface idle time of at least 2 μs is required following
     * a write operation when the device operates in normal mode. In suspend
     * mode an interface idle time of least 450 μs is required */
    delay_ms(1);

    if (spi_interface_supported()) {
        const struct spi_device *dev = spi_bmx160();
        spi_init(dev, SPI_MODE_0, 0xff, 10000);
        spi_select(dev);
        spi_write(dev, &reg_addr, sizeof(reg_addr));
        spi_read(dev, data, len);
        spi_deselect(dev);
        spi_deinit(dev);
    } else {
        const struct i2c_device *dev = i2c_bmx160();
        i2c_init(dev);
        i2c_write(dev, &reg_addr, 1, true);
        i2c_read(dev, data, len);
        i2c_deinit(dev);
    }

    return 0;
}

static void motion_check(int8_t err) { assert(err == 0); }

static int8_t reg_write(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data,
                        uint16_t len) {
    assert(dev_addr == 0);

    if (spi_interface_supported()) {
        const struct spi_device *dev = spi_bmx160();
        spi_init(dev, SPI_MODE_0, 0xff, 10000);
        spi_select(dev);
        spi_write(dev, &reg_addr, sizeof(reg_addr));
        spi_write(dev, data, len);
        spi_deselect(dev);
        spi_deinit(dev);
    } else {
        const struct i2c_device *dev = i2c_bmx160();
        i2c_init(dev);
        uint8_t out_buf[8];
        assert(len + 1U <= sizeof(out_buf));
        out_buf[0] = reg_addr;
        memcpy(out_buf + 1, data, len);
        i2c_write(dev, out_buf, len + 1, false);
        i2c_deinit(dev);
    }

    return 0;
}

static void motion_delay(uint32_t period_ms) { delay_ms(period_ms); }

static uint8_t fifo_buf[1024]; /* 1024 is the max fifo length */

static struct bmi160_fifo_frame fifo = {
    .data = fifo_buf,
    .length = sizeof(fifo_buf),
};

static struct bmi160_dev dev = {
    .id = 0,
    .fifo = &fifo,
    .read = reg_read,
    .write = reg_write,
    .delay_ms = motion_delay,
};

enum motion_mode {
    MOTION_MODE_SUSPEND,
    MOTION_MODE_RUN,
};

static void motion_config(enum motion_mode mode) {
    if (spi_interface_supported()) {
        dev.interface = BMI160_SPI_INTF;
    } else {
        dev.interface = BMI160_I2C_INTF;
    }

    motion_check(bmi160_init(&dev));

    int accel_power;
    int accel_bw;
    switch (mode) {
    case MOTION_MODE_SUSPEND:
        accel_power = BMI160_ACCEL_SUSPEND_MODE;
        accel_bw = BMI160_ACCEL_BW_OSR4_AVG1;
        break;
    case MOTION_MODE_RUN:
        accel_bw = BMI160_ACCEL_BW_OSR2_AVG2;
        accel_power = BMI160_ACCEL_LOWPOWER_MODE;
        break;
    }

    dev.accel_cfg = (struct bmi160_cfg){
        .power = accel_power,
        .odr = BMI160_ACCEL_ODR_12_5HZ,
        .range = BMI160_ACCEL_RANGE_16G,
        .bw = accel_bw,
    };

    dev.gyro_cfg = (struct bmi160_cfg){
        .power = BMI160_GYRO_SUSPEND_MODE,
        .odr = BMI160_GYRO_ODR_25HZ,
        .range = BMI160_GYRO_RANGE_2000_DPS,
        .bw = BMI160_GYRO_BW_OSR4_MODE,
    };

    /* Set power mode, range, and bandwidth of sensor according to accel_cfg
     * and gyro_cfg. */
    motion_check(bmi160_set_sens_conf(&dev));

    /* set the fifo configuration */
    motion_check(bmi160_set_fifo_config(BMI160_FIFO_ACCEL, BMI160_ENABLE, &dev));

    /* clear the fifo */
    motion_check(bmi160_set_fifo_flush(&dev));
}

void motion_suspend() { motion_config(MOTION_MODE_SUSPEND); }

void motion_start() {
    motion_config(MOTION_MODE_RUN);

    struct bmi160_pmu_status stat;
    motion_check(bmi160_get_power_mode(&stat, &dev));
    /* assert(stat.accel_pmu_status == BMI160_AUX_PMU_LOW_POWER); */
}

size_t motion_read(struct motion_reading *buf, size_t buf_size) {
    /* The input fifo length is the capacity of the buffer. The output fifo
     * length after calling get_fifo_data is the amount read */
    dev.fifo->length = sizeof(fifo_buf);

    /* From the datasheet:
     *
     * In low power mode the FIFO is not accessible. For read outs of the FIFO
     * the sensor has to be set to normal mode, after the read-out the sensor
     * can be set back to low power mode.
     */
    dev.accel_cfg.power = BMI160_ACCEL_NORMAL_MODE;
    motion_check(bmi160_set_power_mode(&dev));
    int8_t err = bmi160_get_fifo_data(&dev);
    dev.accel_cfg.power = BMI160_ACCEL_LOWPOWER_MODE;
    motion_check(bmi160_set_power_mode(&dev));

    /* This error may occur if the FIFO is empty, in which case we can exit
     * early */
    if (err == BMI160_READ_WRITE_LENGHT_INVALID)
        return 0;

    motion_check(err);

    static struct bmi160_sensor_data accel_data[MOTION_READ_MAX];

    /* The input len is the capacity, and the output is number of samples */
    uint8_t len = min((size_t)MOTION_READ_MAX, buf_size);
    motion_check(bmi160_extract_accel(accel_data, &len, &dev));

    for (uint8_t i = 0; i < len; ++i) {
        const struct bmi160_sensor_data d = accel_data[i];

        buf[i].x = d.x;
        buf[i].y = d.y;
        buf[i].z = d.z;
    }

    return len;
}
