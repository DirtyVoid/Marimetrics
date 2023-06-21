#include "globe.h"

void sky66112_init(const struct sky66112 *dev) {
    nrf_gpio_cfg_output(dev->cps_gpio_idx);
    nrf_gpio_pin_clear(dev->cps_gpio_idx); // enable

    ret_code_t err_code;

    err_code = nrfx_gpiote_init();
    if (err_code != NRF_ERROR_INVALID_STATE)
        assert(!err_code);

    nrf_ppi_channel_t ppi_set_ch, ppi_clr_ch;

    err_code = nrfx_ppi_channel_alloc(&ppi_set_ch);
    assert(!err_code);

    err_code = nrfx_ppi_channel_alloc(&ppi_clr_ch);
    assert(!err_code);

    nrfx_gpiote_out_config_t config = NRFX_GPIOTE_CONFIG_OUT_TASK_TOGGLE(false);
    err_code = nrfx_gpiote_out_init(dev->ctx_gpio_idx, &config);
    assert(!err_code);
    uint32_t gpiote_ch = nrfx_gpiote_out_task_addr_get(dev->ctx_gpio_idx);

    ble_opt_t opt = {
        .common_opt = {.pa_lna = {.pa_cfg =
                                      {
                                          .active_high = 1,
                                          .enable = 1,
                                          .gpio_pin = dev->ctx_gpio_idx,
                                      },
                                  .lna_cfg =
                                      {
                                          .active_high = 1,
                                          .enable = 1,
                                          .gpio_pin = dev->crx_gpio_idx,
                                      },

                                  /* GPIOTE channel used for radio pin toggling */
                                  .gpiote_ch_id = (gpiote_ch - NRF_GPIOTE_BASE) >> 2,

                                  /* PPI channel used for radio pin clearing */
                                  .ppi_ch_id_clr = ppi_clr_ch,

                                  /* PPI channel used for radio pin setting */
                                  .ppi_ch_id_set = ppi_set_ch}}};

    err_code = sd_ble_opt_set(BLE_COMMON_OPT_PA_LNA, &opt);
    assert(!err_code);
}
