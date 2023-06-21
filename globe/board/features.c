#include "globe.h"

periph_mask_t board_peripheral_support() {
    periph_mask_t r4_base_support = PERIPH_MASK_COMM | PERIPH_MASK_SD_CARD |
                                    PERIPH_MASK_FDOEMO2 | PERIPH_MASK_FDOEMPH |
                                    PERIPH_MASK_MAX31856 | PERIPH_MASK_MCP3208 |
                                    PERIPH_MASK_THERMISTOR | PERIPH_MASK_EEPROM |
                                    PERIPH_MASK_89BSD | PERIPH_MASK_PHONY_SENSOR;

    switch (board_revision()) {
    case BOARD_CONFIG_R4_2:
        return r4_base_support | PERIPH_MASK_BMX160_SPI;
    case BOARD_CONFIG_R4_3:
        return r4_base_support | PERIPH_MASK_BMX160_I2C;
    case BOARD_CONFIG_DEV:
        return r4_base_support | PERIPH_MASK_BMX160_I2C;
    case BOARD_CONFIG_BARE:
    default:
        return 0;
    }
}
