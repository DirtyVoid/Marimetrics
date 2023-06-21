#include "globe.h"

static void configure_cs_line(uint32_t pin, bool peripheral_powered) {
    if (peripheral_powered) {
        /* Initialize CS line unselected */
        nrf_gpio_pin_set(pin);
        nrf_gpio_cfg_output(pin);
    } else {
        nrf_gpio_cfg_default(pin);
    }
}

void power_management_init() {
    /* Configure the power control pins. The pins must be set high before the
     * pins are enabled, otherwise the peripherals will be powered for a brief
     * moment */
    nrf_gpio_pin_set(sd_card_en_pin());
    nrf_gpio_cfg_output(sd_card_en_pin());
    nrf_gpio_pin_set(sensors_gpios_pin());
    nrf_gpio_cfg_output(sensors_gpios_pin());

    /* Start with all peripherals disabled */
    power_management_configure(0);
}

static bool pm_check(periph_mask_t req, periph_mask_t *enabled_out,
                     periph_mask_t periph) {
    bool enabled = periph & req;
    if (enabled) {
        *enabled_out |= periph;
    }
    return enabled;
}

static void set_subsystem_power(uint32_t pin, bool enabled) {
    /* Power switching is implemented with high-side MOSFETs which have
     * inverted logic. Short delays are added before providing power to each
     * subcircuit so the initial current surge does not cause a brownout. */

    if (enabled) {
        pwm_ramp_pin(pin, false, 25);
    } else {
        nrf_gpio_pin_write(pin, true);
    }
}

bool fdoems_plugged_in = true;

periph_mask_t power_management_configure(periph_mask_t req) {
    periph_mask_t enabled = 0;

    bool sd_power_enabled = false;
    bool peripheral_power_enabled = false;

    if (board_revision() == BOARD_CONFIG_R4_2) {
        sd_power_enabled =
            pm_check(req, &enabled, PERIPH_MASK_SD_CARD | PERIPH_MASK_COMM);
        peripheral_power_enabled = pm_check(
            req, &enabled,
            PERIPH_MASK_BMX160_SPI | PERIPH_MASK_FDOEMO2 | PERIPH_MASK_FDOEMPH |
                PERIPH_MASK_MAX31856 | PERIPH_MASK_MCP3208 | PERIPH_MASK_THERMISTOR |
                PERIPH_MASK_89BSD | PERIPH_MASK_PHONY_SENSOR | PERIPH_MASK_COMM);
    } else if (board_revision() == BOARD_CONFIG_R4_3 ||
               board_revision() == BOARD_CONFIG_DEV) {
        sd_power_enabled = pm_check(req, &enabled, PERIPH_MASK_SD_CARD);
        peripheral_power_enabled = pm_check(
            req, &enabled,
            PERIPH_MASK_FDOEMO2 | PERIPH_MASK_MCP3208 | PERIPH_MASK_FDOEMPH |
                PERIPH_MASK_MAX31856 | PERIPH_MASK_MCP3208 | PERIPH_MASK_THERMISTOR |
                PERIPH_MASK_89BSD | PERIPH_MASK_PHONY_SENSOR);
        enabled |= PERIPH_MASK_COMM | PERIPH_MASK_BMX160_I2C;
    }

    assert((enabled & req) == req);

    if (fdoems_plugged_in) {
        /* Make sure UART lines are pulled low when unpowered (UART lines are by
         * default held high). */
        if (!peripheral_power_enabled)
            uart_disconnect_pins();
        else
            uart_reconnect_pins();
    }

    set_subsystem_power(sd_card_en_pin(), sd_power_enabled);
    set_subsystem_power(sensors_gpios_pin(), peripheral_power_enabled);

    /* Configure SPI CS lines. */
    configure_cs_line(sd_card_cs_pin(), sd_power_enabled);
    configure_cs_line(ntc1_cs_pin(), peripheral_power_enabled);
    if (board_peripheral_support() & PERIPH_MASK_BMX160_SPI)
        configure_cs_line(accel_cs_pin(), peripheral_power_enabled);
    if (board_peripheral_support() & PERIPH_MASK_MCP3208)
        configure_cs_line(mcp3208_cs_pin(), peripheral_power_enabled);
    if (board_peripheral_support() & PERIPH_MASK_EEPROM)
        configure_cs_line(eeprom_cs_pin(), peripheral_power_enabled);

    /* Make sure SPI lines are pulled low when unpowered (including MISO, which
     * may have internal pull-up when active).
     *
     * Note: this is handled already in SPI driver.
     */

    /* Make sure I2C lines are pulled low when unpowered (which have internal
     * pull-up when active)
     *
     * Note: this should not be done on 4.2 boards. The only peripherals
     * connected to I2C are the fuel gauge (always powered), and GPIO header
     * for pressure sensor (controlled by peripheral power MOSFET), which are
     * on the same bus. Because the fuel gauge must always be powered, the I2C
     * lines should be pulled high. The GPIO header should not be used with 4.2
     * to cause a current leak over I2C lines when peripheral power is turned
     * off.
     */
    if (!peripheral_power_enabled)
        i2c_disconnect_pins();
    else
        i2c_reconnect_pins();

    if (sd_power_enabled | peripheral_power_enabled)
        delay_ms(5);

    return enabled;
}
