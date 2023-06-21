#include "globe.h"

static struct spi_bus sensor_bus, sdcard_bus;
static struct spi_device max31856, sdcard, bmx160_spi, mcp3208;

static struct i2c_bus main_i2c_bus, _4pin_connector_i2c_bus;
static struct i2c_device lc709203f, bmx160_i2c, _89bsd_i2c;

static struct adc_channel thermistor_adc_channel;

void peripheral_init() {
    sensor_bus.miso = spi_miso_pin();
    sensor_bus.mosi = spi_mosi_pin();
    sensor_bus.sck = spi_sck_pin();

    if (board_revision() >= BOARD_CONFIG_R4_3) {
        sdcard_bus.miso = sd_card_spi_miso();
        sdcard_bus.mosi = sd_card_spi_mosi();
        sdcard_bus.sck = sd_card_spi_sck();
        sdcard.bus = &sdcard_bus;
    } else {
        sdcard.bus = &sensor_bus;
    }

    max31856.cs = ntc1_cs_pin();
    max31856.bus = &sensor_bus;

    sdcard.cs = sd_card_cs_pin();

    if (board_peripheral_support() & PERIPH_MASK_BMX160_SPI) {
        bmx160_spi.cs = accel_cs_pin();
        bmx160_spi.bus = &sensor_bus;
    }

    if (board_peripheral_support() & PERIPH_MASK_MCP3208) {
        mcp3208.cs = mcp3208_cs_pin();
        mcp3208.bus = &sensor_bus;
    }

    main_i2c_bus.scl = i2c_scl_pin();
    main_i2c_bus.sda = i2c_sda_pin();

    _4pin_connector_i2c_bus.sda = header_gpio_5_pin();
    _4pin_connector_i2c_bus.scl = header_gpio_6_pin();

    lc709203f.addr = 0x0b;
    lc709203f.bus = &main_i2c_bus;

    bmx160_i2c.addr = 0x68;
    bmx160_i2c.bus = &main_i2c_bus;

    _89bsd_i2c.addr = 0x77;
    _89bsd_i2c.bus = &_4pin_connector_i2c_bus;

    thermistor_adc_channel.pin = ntc2_out();
}

const struct spi_device *spi_max31856() { return &max31856; }

const struct spi_device *spi_sdcard() { return &sdcard; }

const struct spi_device *spi_bmx160() {
    assert(board_peripheral_support() & PERIPH_MASK_BMX160_SPI);
    return &bmx160_spi;
}

const struct spi_device *spi_mcp3208() {
    assert(board_peripheral_support() & PERIPH_MASK_MCP3208);
    return &mcp3208;
}

const struct i2c_device *i2c_lc709203f() { return &lc709203f; }

const struct i2c_device *i2c_bmx160() {
    assert(board_peripheral_support() & PERIPH_MASK_BMX160_I2C);
    return &bmx160_i2c;
}

const struct i2c_device *i2c_89bsd() {
    assert(board_peripheral_support() & PERIPH_MASK_89BSD);
    return &_89bsd_i2c;
}

const struct adc_channel *adc_thermistor() { return &thermistor_adc_channel; }

static void i2c_disconnect_bus_pins(struct i2c_bus *bus) {
    nrf_gpio_input_disconnect(bus->scl);
    nrf_gpio_input_disconnect(bus->sda);
}

static void i2c_reconnect_bus_pins(struct i2c_bus *bus) {
    nrf_gpio_cfg_input(bus->scl, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_cfg_input(bus->sda, NRF_GPIO_PIN_PULLUP);
}

void i2c_disconnect_pins() {
    i2c_disconnect_bus_pins(&main_i2c_bus);
    i2c_disconnect_bus_pins(&_4pin_connector_i2c_bus);
}

void i2c_reconnect_pins() {
    i2c_reconnect_bus_pins(&main_i2c_bus);
    i2c_reconnect_bus_pins(&_4pin_connector_i2c_bus);
}
