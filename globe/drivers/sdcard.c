#include "globe.h"

/* If the card is high capacity then the addressing mode is block-based (512
 * bytes), otherwise the addressing mode is byte-based. */
static bool block_addressing;

static uint8_t read_byte(const struct spi_device *dev) {
    uint8_t byte;
    spi_read(dev, &byte, 1);
    return byte;
}

static uint8_t read_byte_with_timeout(const struct spi_device *dev, epoch_t timeout) {
    assert(elapsed_ms() < timeout);
    return read_byte(dev);
}

static void wait_for_miso_high(const struct spi_device *dev) {

    /* Fast poll */
    uint8_t buf[8];
    spi_read(dev, buf, sizeof(buf));
    if (buf[sizeof(buf) - 1] == 0xff)
        return;

    /* Slow poll */
    epoch_t timeout = elapsed_ms() + 1000;
    while (read_byte_with_timeout(dev, timeout) != 0xff) {
        delay_ms(1);
    }
}

static void send_cmd(const struct spi_device *dev, uint8_t cmd, uint32_t arg) {
    DEBUG(cmd, arg);
    uint8_t crc = 0xff;
    if (cmd == 0)
        crc = 0x95;
    else if (cmd == 8)
        crc = 0x87;

    /* The argument is transfered in big-endian orientiation */
    uint8_t buf[] = {0x40 | cmd,          (uint8_t)(arg >> 24), (uint8_t)(arg >> 16),
                     (uint8_t)(arg >> 8), (uint8_t)(arg),       crc};

    spi_write(dev, buf, sizeof(buf));
}

static uint8_t read_r1_response(const struct spi_device *dev) {
    uint8_t r1;
    epoch_t timeout = elapsed_ms() + 1000;
    while (((r1 = read_byte_with_timeout(dev, timeout)) & 0x80))
        ;

    /* verify that all error flags (ignoring IN_IDLE_STATE) are cleared */
    assert((r1 & ~0x01) == 0);

    return r1;
}

/* Read r3/r7 response after r1 has been received */
static uint32_t read_u32_response(const struct spi_device *dev) {
    uint8_t buf[4];
    spi_read(dev, buf, sizeof(buf));
    uint32_t r7 = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
                  ((uint32_t)buf[2] << 8) | ((uint32_t)buf[3] << 0);
    return r7;
}

static uint8_t cmd(const struct spi_device *dev, uint8_t cmd, uint32_t arg) {
    DEBUG(cmd, arg);
    wait_for_miso_high(dev);
    send_cmd(dev, cmd, arg);
    return read_r1_response(dev);
}

static const struct spi_device *sdcard_setup() {
    const struct spi_device *dev = spi_sdcard();
    spi_init(dev, SPI_MODE_0, 0xff, 20000);
    spi_select(dev);
    return dev;
}

static void sdcard_finish(const struct spi_device *dev) {
    /* Note: SD cards do not release the MISO line when the CS line is pulled
     * high; the MISO line is released synchronously with the clock line. It is
     * therefore necessary to transfer a dummy byte after releasing the CS line in
     * a multi-slave configuration to ensure the SD card releases the line. */

    spi_deselect(dev);
    uint8_t buf;
    spi_read(dev, &buf, 1);
    spi_deinit(dev);
}

void sdcard_init() {
    const struct spi_device *dev = spi_sdcard();
    spi_init(dev, SPI_MODE_0, 0xff, 400);

    uint8_t dummy_buf[8];
    spi_read(dev, dummy_buf, sizeof(dummy_buf));

    /* Issue CMD0 for software reset (must have valid CRC). */
    spi_select(dev);
    cmd(dev, 0, 0);

    cmd(dev, 8, 0x1aa);
    uint32_t cmd8_r7 = read_u32_response(dev);
    assert(cmd8_r7 == 0x1aa);

    /*
     * Issue ACMD41 to start initialization process. The command is issued
     * until the IN_IDLE_STATE bit is cleared.
     *
     * ACMD41 consists of a command sequence CMD55 followed by CMD41.
     *
     * Note ACMD41 may be rejected for an MMC card (ILLEGAL_COMMAND), in which
     * case CMD1 should be used. This driver does not support MMC cards so this
     * case is not handled.
     */

    epoch_t t0 = elapsed_ms();

    for (;;) {

        cmd(dev, 55, 0);

        uint8_t r1 = cmd(dev, 41, (1uL << 30));

        if (r1 == 0x00)
            break;

        epoch_t t = elapsed_ms();
        assert(t - t0 <= 1250);
    }

    /* Read OCR register */
    cmd(dev, 58, 0);
    uint32_t ocr = read_u32_response(dev);
    block_addressing = ocr & (1UL << 30);

    sdcard_finish(dev);
}

void sdcard_start_multiblock_read(uint32_t addr) {
    const struct spi_device *dev = sdcard_setup();
    cmd(dev, 18, (block_addressing ? 1 : SDCARD_BLOCK_SIZE) * addr);
}

void sdcard_stop_multiblock_read() {
    const struct spi_device *dev = spi_sdcard();

    /* Send stop_transmission command right away. We want to make sure that the
     * command is sent before the transmission of the next data block so it is
     * important not to do any SPI data transfers after the last block was
     * received. */
    send_cmd(dev, 12, 0);

    /* Discard stuff byte */
    read_byte(dev);

    /* Get R1 response. The response should return 0x00, but I've found one
     * card that consistently returns 0x1f. The card functions properly despite
     * indication of error. The R1 response is therefore not validated when
     * reading. I also checked how multi-block termination is handled in the
     * nRF SD card library (app_sdcard.c) and they don't validate the response
     * from CMD12 either. */
    uint8_t r1;
    epoch_t r1_timeout = elapsed_ms() + 1000;
    while (((r1 = read_byte_with_timeout(dev, r1_timeout)) & 0x80))
        ;

    /* wait while busy (for R1b response) */
    wait_for_miso_high(dev);

    sdcard_finish(dev);
}

void sdcard_read_block(void *data_buffer) {
    const struct spi_device *dev = spi_sdcard();

    /* wait for packet */
    uint8_t block_token;
    epoch_t timeout = elapsed_ms() + 5000;
    while ((block_token = read_byte_with_timeout(dev, timeout)) == 0xff)
        ;
    assert(block_token == 0xfe);

    /* read data */
    spi_read(dev, data_buffer, SDCARD_BLOCK_SIZE);

    /* read CRC */
    uint8_t crc[2];
    spi_read(dev, crc, sizeof(crc));
}

void sdcard_start_multiblock_write(uint32_t addr) {
    const struct spi_device *dev = sdcard_setup();
    cmd(dev, 25, (block_addressing ? 1 : SDCARD_BLOCK_SIZE) * addr);
}

void sdcard_stop_multiblock_write() {
    const struct spi_device *dev = spi_sdcard();

    /* send stop-trans token and dummy byte. The stop-trans token is sent
     * without CRC.
     *
     * Note: the data is specified as non-static as the SPI transfer may
     * require the data memory to reside in RAM. */
    const uint8_t stop_data[] = {
        0xfd,
        0xff,
    };

    spi_write(dev, stop_data, sizeof(stop_data));

    /* MISO will be held low during programming */
    wait_for_miso_high(dev);

    /* use SEND_STATUS to validate write operation */
    cmd(dev, 13, 0);
    uint8_t status = read_byte(dev);
    assert(status == 0x00);

    sdcard_finish(dev);
}

void sdcard_write_block(const void *data_buffer) {
    const struct spi_device *dev = spi_sdcard();

    /* stuff data then data token */
    const uint8_t data_token[] = {
        0xff,
        0xfc,
    };
    spi_write(dev, data_token, sizeof(data_token));

    /* data */
    spi_write(dev, data_buffer, SDCARD_BLOCK_SIZE);

    /* CRC */
    const uint8_t crc[] = {0xff, 0xff};
    spi_write(dev, crc, sizeof(crc));

    /* read data response */
    uint8_t data_response = read_byte(dev) & 0x1f;
    assert(data_response == 0x05);

    /* Wait for programming */
    wait_for_miso_high(dev);
}
