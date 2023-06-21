#include "globe.h"

#define START_BIT (1 << 5)
#define SGL_DIFF_BIT (1 << 4)

static uint16_t mcp3208_read(uint8_t ctrl_bits) {
    const struct spi_device *dev = spi_mcp3208();
    spi_init(dev, SPI_MODE_0, 0xff, 1000);
    spi_select(dev);
    spi_write(dev, &ctrl_bits, 1);
    uint8_t rx[2];
    spi_read(dev, &rx, array_size(rx));
    spi_deselect(dev);
    spi_deinit(dev);
    uint16_t u16 = (((uint16_t)rx[0]) << 8) | rx[1];
    return (u16 >> 2) & 0xfff;
}

uint16_t mcp3208_single_ended_conversion(int channel) {
    assert(channel >= 0);
    assert(channel <= 7);
    return mcp3208_read(START_BIT | SGL_DIFF_BIT | channel);
}

uint16_t mcp3208_differential_conversion(int channel) {
    assert(channel >= 0);
    assert(channel <= 7);
    return mcp3208_read(START_BIT | channel);
}
