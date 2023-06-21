#include "globe.h"

static void check_nrfx(nrfx_err_t err) { assert(err == NRFX_SUCCESS); }

static const nrfx_twi_t twi = NRFX_TWI_INSTANCE(1);

void i2c_init(const struct i2c_device *dev) {
    nrfx_twi_config_t twi_conf = {
        .scl = dev->bus->scl,
        .sda = dev->bus->sda,
        .frequency = TWI_FREQUENCY_FREQUENCY_K100,
        .interrupt_priority = NRFX_TWI_DEFAULT_CONFIG_IRQ_PRIORITY,
        .hold_bus_uninit = true, /* Doesn't matter unless we uninit */
    };

    check_nrfx(nrfx_twi_init(&twi, &twi_conf, NULL, NULL));

    nrfx_twi_enable(&twi);
}

void i2c_deinit(const struct i2c_device *dev) {
    (void)dev;
    nrfx_twi_disable(&twi);
    nrfx_twi_uninit(&twi);
}

void i2c_read(const struct i2c_device *dev, void *buf, size_t num_bytes) {
    check_nrfx(nrfx_twi_rx(&twi, dev->addr, buf, num_bytes));
}

void i2c_write(const struct i2c_device *dev, const void *buf, size_t num_bytes,
               bool no_stop) {
    check_nrfx(nrfx_twi_tx(&twi, dev->addr, buf, num_bytes, no_stop));
}
