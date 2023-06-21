#ifndef PIN_DEFS_H
#define PIN_DEFS_H

#include "globe.h"

#define DEF_PIN_1(r, data, X)                                                          \
    case BOOST_PP_CAT(BOARD_CONFIG_, BOOST_PP_SEQ_ELEM(0, X)):                         \
        return BOOST_PP_SEQ_ELEM(1, X);

#define DEF_PIN(NAME, ...)                                                             \
    static inline int NAME() {                                                         \
        switch (board_revision()) {                                                    \
            BOOST_PP_SEQ_FOR_EACH(DEF_PIN_1, , BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__)); \
        default:                                                                       \
            assert(0 && #NAME " unavailable");                                         \
        }                                                                              \
    }

/* These functions map a pin net (as defined in the schematic) to the
 * corresponding pin index. Nets with the same name across different boards do
 * not necessarily have the same functionality. For instance, 3V3_SENSORS_GPIO
 * controls power to the BMX160 in R4.2 but not in R4.3. */

/* External GPIO pins */
DEF_PIN(header_gpio_1_pin, (R4_2)(3), (R4_3)(3), (DEV)(3));
DEF_PIN(header_gpio_2_pin, (R4_2)(9), (R4_3)(9), (DEV)(9));
DEF_PIN(header_gpio_3_pin, (R4_2)(10), (R4_3)(24), (DEV)(24));
DEF_PIN(header_gpio_4_pin, (R4_2)(20), (R4_3)(25), (DEV)(25));
DEF_PIN(header_gpio_5_pin, (R4_2)(18), (R4_3)(10), (DEV)(10));
DEF_PIN(header_gpio_6_pin, (R4_2)(11), (R4_3)(29), (DEV)(29));
DEF_PIN(header_gpio_7_pin, (R4_3)(30), (DEV)(30));
DEF_PIN(header_gpio_8_pin, (R4_3)(31), (DEV)(31));

DEF_PIN(mcp3208_cs_pin, (R4_2)(18), (R4_3)(30), (DEV)(30));
DEF_PIN(eeprom_cs_pin, (R4_2)(11), (R4_3)(31), (DEV)(31));

/* Power control pins */
DEF_PIN(sd_card_en_pin, (R4_2)(4), (R4_3)(4), (DEV)(4));
/* This is 3.3V_SENSORS_GPIOS in the schematics */
DEF_PIN(sensors_gpios_pin, (R4_2)(16), (R4_3)(14), (DEV)(14));

/* SPI pins */
DEF_PIN(spi_miso_pin, (R4_2)(15), (R4_3)(15), (DEV)(15));
DEF_PIN(spi_mosi_pin, (R4_2)(12), (R4_3)(12), (DEV)(12));
DEF_PIN(spi_sck_pin, (R4_2)(23), (R4_3)(11), (DEV)(11));
DEF_PIN(sd_card_spi_miso, (R4_3)(5), (DEV)(5));
DEF_PIN(sd_card_spi_mosi, (R4_3)(8), (DEV)(8));
DEF_PIN(sd_card_spi_sck, (R4_3)(7), (DEV)(7));

/* CS pins */
DEF_PIN(sd_card_cs_pin, (R4_2)(28), (R4_3)(28), (DEV)(28));
DEF_PIN(accel_cs_pin, (R4_2)(30), (DEV)(30));
DEF_PIN(ntc1_cs_pin, (R4_2)(5), (R4_3)(23), (DEV)(23));

/* I2C pins */
DEF_PIN(i2c_sda_pin, (R4_2)(26), (R4_3)(26), (DEV)(26));
DEF_PIN(i2c_scl_pin, (R4_2)(27), (R4_3)(27), (DEV)(27));

/* LED pins */
/* This net is not named in the schematics */
DEF_PIN(error_led_pin, (R4_2)(24), (R4_3)(16), (DEV)(16));

/* ADC pins */
DEF_PIN(ntc2_out, (R4_2)(1), (R4_3)(1), (DEV)(1));

static inline bool board_has_separate_sdcard_spi_bus() {
    return board_revision() >= BOARD_CONFIG_R4_3;
}

#endif
