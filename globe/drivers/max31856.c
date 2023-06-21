#include "globe.h"

static void spi_max31856_write_reg(uint8_t reg, uint8_t value) {
    const struct spi_device *dev = spi_max31856();
    spi_init(dev, SPI_MODE_1, 0x00, 5000);
    spi_select(dev);
    const uint8_t tx_buf[] = {0x80 | reg, value};
    spi_write(dev, tx_buf, sizeof(tx_buf));
    spi_deselect(dev);
    spi_deinit(dev);
}

static uint8_t spi_max31856_read_reg(uint8_t reg) {
    const struct spi_device *dev = spi_max31856();
    spi_init(dev, SPI_MODE_1, 0x00, 5000);
    spi_select(dev);
    const uint8_t tx_buf[] = {reg, 0x00};
    uint8_t rx_buf[2];
    spi_transfer(dev, tx_buf, rx_buf, 2);
    spi_deselect(dev);
    spi_deinit(dev);
    return rx_buf[1];
}

void max31856_init() {

    /* Set the thermocouple type to type T in CR1 */
    spi_max31856_write_reg(0x01, 0x07);
}

float max31856_read() {

    /* clear any existing faults */
    spi_max31856_write_reg(0x0f, 0x00);

    /* Write the 1SHOT bit in CR0 to start conversion */
    spi_max31856_write_reg(0x00, 0x40);
    epoch_t t0 = elapsed_ms();

    /* wait for conversion. ensure 1shot bit is cleared */
    epoch_t t = t0;
    for (;;) {
        assert(t < t0 + 275);

        uint8_t r00 = spi_max31856_read_reg(0x00);
        uint8_t r0f = spi_max31856_read_reg(0x0f);
        DEBUG(r00, r0f);
        assert(r0f == 0x00);
        if (r00 == 0x00)
            break;
        t += 5;
        wait_until(t);
    }

    /* Read the temperature registers */
    uint32_t b0 = spi_max31856_read_reg(0x0E);
    uint32_t b1 = spi_max31856_read_reg(0x0D);
    uint32_t b2 = spi_max31856_read_reg(0x0C);

    /* Temperature conversion format from MAX31856 datasheet table 3 */

    int32_t temp = b0 + 256 * b1 + 256 * 256 * b2;

    /* preserve the 2s complement value if negative */
    temp = temp << 8;
    temp = temp >> 8;

    /* conversion factor of .0625/256 from datasheet */
    return (float)(0.0625 / 256.0) * temp;
}
