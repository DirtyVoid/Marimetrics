#include "globe.h"

static nrfx_spim_t spi = NRFX_SPIM_INSTANCE(0); //< SPI instance.
#define MAX_SPI_TRANSFER ((((size_t)1) << SPIM0_EASYDMA_MAXCNT_SIZE) - 1)

static void check_nrfx_err(nrfx_err_t value) { assert(value == NRFX_SUCCESS); }

static nrf_spim_frequency_t get_spi_frequency(int max_freq_khz) {
    if (max_freq_khz >= 4000)
        return NRF_SPIM_FREQ_4M;
    else if (max_freq_khz >= 2000)
        return NRF_SPIM_FREQ_2M;
    else if (max_freq_khz >= 1000)
        return NRF_SPIM_FREQ_1M;
    else if (max_freq_khz >= 500)
        return NRF_SPIM_FREQ_500K;
    else if (max_freq_khz >= 250)
        return NRF_SPIM_FREQ_250K;
    else if (max_freq_khz >= 125)
        return NRF_SPIM_FREQ_125K;
    else {
        assert(0 && "max SPI frequency too low");
    }
}

static nrf_spim_mode_t get_spi_mode(enum spi_mode mode) {
    switch (mode) {
    case SPI_MODE_0:
        return NRF_SPIM_MODE_0;
    case SPI_MODE_1:
        return NRF_SPIM_MODE_1;
    case SPI_MODE_2:
        return NRF_SPIM_MODE_2;
    case SPI_MODE_3:
        return NRF_SPIM_MODE_3;
    }
    assert(0 && "invalid SPI mode");
}

void spi_init(const struct spi_device *dev, enum spi_mode mode, uint8_t mosi_read_char,
              int max_freq_khz) {
    nrfx_spim_uninit(&spi);

    nrfx_spim_config_t spi_config = NRFX_SPIM_DEFAULT_CONFIG;
    spi_config.ss_pin = NRFX_SPIM_PIN_NOT_USED;
    spi_config.bit_order = NRF_SPIM_BIT_ORDER_MSB_FIRST;
    spi_config.orc = mosi_read_char;
    spi_config.miso_pin = dev->bus->miso;
    spi_config.mosi_pin = dev->bus->mosi;
    spi_config.sck_pin = dev->bus->sck;
    spi_config.frequency = get_spi_frequency(max_freq_khz);
    spi_config.mode = get_spi_mode(mode);

    check_nrfx_err(nrfx_spim_init(&spi, &spi_config, NULL, NULL));
}

static void disable_spi_pin(pin_id pin) {
    nrf_gpio_pin_clear(pin);
    nrf_gpio_cfg_output(pin);
}

void spi_deinit(const struct spi_device *dev) {
    /* Nordic Info Center:
     *
     * When putting the system in low power and the peripheral is not needed,
     * lowest possible power consumption is achieved by stopping, and then
     * disabling the peripheral.
     *
     * The STOP task may not be always needed (the peripheral might already be
     * stopped), but if it is sent, software shall wait until the STOPPED event
     * was received as a response before disabling the peripheral through the
     * ENABLE register. */

    /* It is assumed that the SPI peripheral is already stopped. */

    nrfx_spim_uninit(&spi);
    /* Drive SPI lines low so peripherals can be shut down without power loss
     * over SPI lines. MISO can also be driven low because no slaves should
     * be selected. */
    disable_spi_pin(dev->bus->miso);
    disable_spi_pin(dev->bus->mosi);
    disable_spi_pin(dev->bus->sck);
}

#define ERRATA_58_PPI_CHANNEL 0
#define ERRATA_58_GPIOTE_CHANNEL 0

static void errata_58_fix_enable() {
    NRF_SPIM_Type *spim = spi.p_reg;

    // Create an event when SCK toggles.
    NRF_GPIOTE->CONFIG[ERRATA_58_GPIOTE_CHANNEL] =
        (GPIOTE_CONFIG_MODE_Event << GPIOTE_CONFIG_MODE_Pos) |
        (spim->PSEL.SCK << GPIOTE_CONFIG_PSEL_Pos) |
        (GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos);
    // Stop the spim instance when SCK toggles.
    NRF_PPI->CH[ERRATA_58_PPI_CHANNEL].EEP =
        (uint32_t)&NRF_GPIOTE->EVENTS_IN[ERRATA_58_GPIOTE_CHANNEL];
    NRF_PPI->CH[ERRATA_58_PPI_CHANNEL].TEP = (uint32_t)&spim->TASKS_STOP;
    NRF_PPI->CHENSET = 1U << ERRATA_58_PPI_CHANNEL;

    // The spim instance cannot be stopped mid-byte, so it will finish
    // transmitting the first byte and then stop. Effectively ensuring
    // that only 1 byte is transmitted.
}

static void errata_58_fix_disable() {
    NRF_GPIOTE->CONFIG[ERRATA_58_GPIOTE_CHANNEL] = 0;
    NRF_PPI->CHENCLR = 1U << ERRATA_58_PPI_CHANNEL;
}

static void spi_tx_rx(const void *tx_buf, size_t tx_buf_len, void *rx_buf,
                      size_t rx_buf_len) {

    const uint8_t *tx_ptr = tx_buf;
    uint8_t *rx_ptr = rx_buf;

    const uint8_t *tx_end = tx_ptr + tx_buf_len;
    uint8_t *rx_end = rx_buf + rx_buf_len;

    while (tx_ptr != tx_end || rx_ptr != rx_end) {

        size_t n_tx = MIN(tx_end - tx_ptr, MAX_SPI_TRANSFER);
        size_t n_rx = MIN(rx_end - rx_ptr, MAX_SPI_TRANSFER);

        nrfx_spim_xfer_desc_t transfer = {
            .p_tx_buffer = tx_ptr,
            .tx_length = n_tx,
            .p_rx_buffer = rx_ptr,
            .rx_length = n_rx,
        };

        bool apply_errata_58_fix = n_rx == 1 && n_tx <= 1;

        if (apply_errata_58_fix)
            errata_58_fix_enable();

        check_nrfx_err(nrfx_spim_xfer(&spi, &transfer, 0));

        if (apply_errata_58_fix)
            errata_58_fix_disable();

        tx_ptr += n_tx;
        rx_ptr += n_rx;
    }
}

void spi_read(const struct spi_device *dev, void *buf, size_t num_bytes) {
    (void)dev;
    spi_tx_rx(NULL, 0, buf, num_bytes);
}

void spi_write(const struct spi_device *dev, const void *buf, size_t num_bytes) {
    (void)dev;
    spi_tx_rx(buf, num_bytes, NULL, 0);
}

void spi_transfer(const struct spi_device *dev, const void *write_buf, void *read_buf,
                  size_t num_bytes) {
    (void)dev;
    spi_tx_rx(write_buf, num_bytes, read_buf, num_bytes);
}

void spi_select(const struct spi_device *dev) { nrf_gpio_pin_clear(dev->cs); }

void spi_deselect(const struct spi_device *dev) { nrf_gpio_pin_set(dev->cs); }
