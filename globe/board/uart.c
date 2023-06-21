#include "globe.h"

#define UARTE_INSTANCE NRF_UARTE0

struct uart_config {
    uint32_t tx_pin;
    uint32_t rx_pin;
};

static struct uart_config get_config(fdoem_device dev) {
    switch (dev) {
    case fdoem_o2:
        /* header CN7 */
        return (struct uart_config){header_gpio_1_pin(), header_gpio_2_pin()};
    case fdoem_ph:
        /* header CN8 */
        return (struct uart_config){header_gpio_3_pin(), header_gpio_4_pin()};
    default:
        assert(0 && "bad device");
    }
}

fdoem_device current_device = fdoem_undefined;

static void uart_validate() {
    assert(!nrf_uarte_event_check(UARTE_INSTANCE, NRF_UARTE_EVENT_ERROR));
}

void configure_uart(fdoem_device device) {
    if (device == current_device)
        return;

    const struct uart_config conf = get_config(device);
    nrf_uarte_txrx_pins_set(UARTE_INSTANCE, conf.tx_pin, conf.rx_pin);
    current_device = device;
}

void uart_disconnect_pins() {
    struct uart_config o2_conf = get_config(fdoem_o2), ph_conf = get_config(fdoem_ph);

    nrf_gpio_cfg_default(o2_conf.tx_pin);
    nrf_gpio_cfg_default(o2_conf.rx_pin);
    nrf_gpio_cfg_default(ph_conf.tx_pin);
    nrf_gpio_cfg_default(ph_conf.rx_pin);
}

void uart_reconnect_pins() {
    struct uart_config o2_conf = get_config(fdoem_o2), ph_conf = get_config(fdoem_ph);

    nrf_gpio_pin_set(o2_conf.tx_pin);
    nrf_gpio_cfg_output(o2_conf.tx_pin);
    nrf_gpio_cfg_input(o2_conf.rx_pin, NRF_GPIO_PIN_PULLUP);

    nrf_gpio_pin_set(ph_conf.tx_pin);
    nrf_gpio_cfg_output(ph_conf.tx_pin);
    nrf_gpio_cfg_input(ph_conf.rx_pin, NRF_GPIO_PIN_PULLUP);
}

void uart_init() {
    nrf_uarte_event_clear(UARTE_INSTANCE, NRF_UARTE_EVENT_ERROR);

    /* Note: pins are configured in power_management module and do not need to
     * be initialized here. */

    /* Configure UARTE instance */
    nrf_uarte_baudrate_set(UARTE_INSTANCE, NRF_UARTE_BAUDRATE_19200);
    nrf_uarte_configure(UARTE_INSTANCE, NRF_UARTE_PARITY_EXCLUDED,
                        NRF_UARTE_HWFC_DISABLED);

    /* Leave UARTE in disabled state when not in use */
    nrf_uarte_disable(UARTE_INSTANCE);

    uart_validate();
}

static bool active_rx_has_cr(const char *rx_buf, size_t rx_len) {
    memory_barrier();
    for (size_t i = 0; i < rx_len; ++i) {
        if (rx_buf[i] == '\r') {
            return true;
        }
    }
    return false;
}

size_t uart_fdoem_txrx(fdoem_device device, const void *tx_buf, size_t tx_len,
                       void *rx_buf, size_t rx_len) {
    /* Ensure that values fit in MAXCNT registers */
    assert(tx_len < 256);
    assert(rx_len < 256);

    configure_uart(device);
    uart_validate();

    /* Enable UARTE */
    nrf_uarte_enable(UARTE_INSTANCE);

    /* clear RX buffer */
    memset(rx_buf, 0, rx_len);

    /* Set RX buffer */
    nrf_uarte_rx_buffer_set(UARTE_INSTANCE, rx_buf, rx_len);
    /* Clear rx end event */
    nrf_uarte_event_clear(UARTE_INSTANCE, NRF_UARTE_EVENT_ENDRX);
    /* start reception */
    nrf_uarte_task_trigger(UARTE_INSTANCE, NRF_UARTE_TASK_STARTRX);

    /* Set TX buffer */
    nrf_uarte_tx_buffer_set(UARTE_INSTANCE, tx_buf, tx_len);
    /* Clear tx end event */
    nrf_uarte_event_clear(UARTE_INSTANCE, NRF_UARTE_EVENT_ENDTX);
    /* Start transmission */
    nrf_uarte_task_trigger(UARTE_INSTANCE, NRF_UARTE_TASK_STARTTX);

    uart_validate();

    /* Wait for transmission to end */
    while (!nrf_uarte_event_check(UARTE_INSTANCE, NRF_UARTE_EVENT_ENDTX))
        ;

    /* Wait for response with timeout. */
    epoch_t start_time = elapsed_ms();
    /* calibration can take up to 6 seconds */
    epoch_t timeout = start_time + 6000;
    bool did_timeout = false;
    for (epoch_t t = start_time; !active_rx_has_cr(rx_buf, rx_len);) {
        if (t > timeout) {
            did_timeout = true;
            break;
        }
        t += 5;
        wait_until(t);
    }

    /* Nordic Infocenter UARTE:
     *
     * The UARTE receiver is stopped by triggering the STOPRX task. An RXTO
     * event is generated when the UARTE has stopped. The UARTE will make sure
     * that an impending ENDRX event will be generated before the RXTO event is
     * generated. This means that the UARTE will guarantee that no ENDRX event
     * will be generated after RXTO, unless the UARTE is restarted or a FLUSHRX
     * command is issued after the RXTO event is generated. */

    /* stop transmission */
    nrf_uarte_task_trigger(UARTE_INSTANCE, NRF_UARTE_TASK_STOPRX);

    /* Wait for reception to end */
    while (!nrf_uarte_event_check(UARTE_INSTANCE, NRF_UARTE_EVENT_ENDRX))
        ;

    size_t n_rx = did_timeout ? 0 : nrf_uarte_rx_amount_get(UARTE_INSTANCE);

    nrf_uarte_disable(UARTE_INSTANCE);

    return n_rx;
}
