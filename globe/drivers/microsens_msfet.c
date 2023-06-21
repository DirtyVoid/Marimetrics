#include "globe.h"

static float msfet_adc_count_to_v_sg_mv(uint16_t adc_count) {
    float v_ref_mv = 2500.0f; /* ADC reference voltage */
    float v_ref_count = 4096; /* Range of ADC (12 bits) */

    assert(adc_count <= v_ref_count);

    return (v_ref_mv / v_ref_count) * adc_count;
}

static float
msfet_apply_v_sg_temp_compensation(const struct msfet_channel_params *params,
                                   float v_sg_mv, float temperature_deg_c) {
    /* Notes from Microsens documentation:
     *
     * The ISFET sensor has a temperature dependency due to its semiconductor
     * nature. Normally, the ISFET working point is chosen to reduce the
     * influence of the temperature.
     *
     * For larger temperature variations (±10 degC) it is recommended to
     * compensate the temperature effect in the conversion of the ISFET output
     * voltage into pH or ion concentration.
     */

    /* A reference temperature is chosen when applying the compensation. The
     * reference temperature is arbitrary in a sense because it will be negated
     * when the voltage is converted to ion concentration. */

    float t_ref_deg_c = 22.0f;

    return v_sg_mv - params->t_dep * (temperature_deg_c - t_ref_deg_c);
}

static float msfet_linear_fit(const struct msfet_channel_params *params,
                              float v_sg_mv) {
    return params->scale * (v_sg_mv + params->offset);
}

static bool msfet_chan_is_ph(enum msfet_channel chan) {
    return ((chan == MSFET_PH_1) || (chan == MSFET_PH_2));
}

struct msfet_reading msfet_read(const struct msfet_params *params,
                                enum msfet_channel chan, float temp_deg_c) {
    assert(chan < MSFET_N_CHANS);
    struct msfet_reading reading = {};
    int adc_channel = chan;
    reading.adc_count = mcp3208_single_ended_conversion(adc_channel);
    reading.v_sg_mv = msfet_adc_count_to_v_sg_mv(reading.adc_count);

    float v_sg_temp_comp_mv = msfet_apply_v_sg_temp_compensation(
        &params->channel_params[chan], reading.v_sg_mv, temp_deg_c);
    float linear_fitted_val =
        msfet_linear_fit(&params->channel_params[chan], v_sg_temp_comp_mv);

    reading.concentration =
        msfet_chan_is_ph(chan) ? linear_fitted_val : powf(10.0f, linear_fitted_val);

    return reading;
}
