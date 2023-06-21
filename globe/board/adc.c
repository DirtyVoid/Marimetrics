#include "globe.h"

static void event_handler(nrfx_saadc_evt_t const *p_event) {
    (void)p_event;
    assert(0);
}

static void nrfx_check(nrfx_err_t err) { assert(err == NRFX_SUCCESS); }

float adc_read(const struct adc_channel *chan) {
    nrfx_saadc_config_t saadc_conf = {
        .resolution = NRF_SAADC_RESOLUTION_14BIT,
        .oversample = NRF_SAADC_OVERSAMPLE_32X,
        .interrupt_priority = 6,
        .low_power_mode = false,
    };

    nrfx_check(nrfx_saadc_init(&saadc_conf, &event_handler));

    nrf_saadc_channel_config_t channel_conf =
        NRFX_SAADC_DEFAULT_CHANNEL_CONFIG_SE(chan->pin);
    channel_conf.acq_time = NRF_SAADC_ACQTIME_40US;
    channel_conf.burst = NRF_SAADC_BURST_ENABLED;

    nrfx_check(nrfx_saadc_channel_init(0, &channel_conf));
    nrf_saadc_value_t val;
    nrfx_check(nrfx_saadc_sample_convert(0, &val));
    nrfx_check(nrfx_saadc_channel_uninit(0));
    nrfx_saadc_uninit();

    const float bits_per_full_scale = 1UL << 14;
    const float adc_reference_voltage = 0.6;
    const float adc_gain = 1.0 / 6;
    const float volts_per_full_scale = adc_reference_voltage / adc_gain;
    const float volts_per_bit = volts_per_full_scale / bits_per_full_scale;

    return volts_per_bit * val;
}
