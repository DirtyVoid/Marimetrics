#include "globe.h"

static float steinhart_hart_conversion(const struct steinhart_hart_coeffs *p,
                                       float ntc_resistance_ohm) {
    float lnR = logf(ntc_resistance_ohm);
    float Tinv = p->A + p->B * lnR + p->C * (lnR * lnR * lnR);
    return 1 / Tinv - 273.15;
}

struct steinhart_hart_coeffs steinhart_hart_coeffs_calculate(
    const struct steinhart_hart_calibration_data *calib_data) {

    float L_[STEINHART_HART_NUM_CALIB_POINTS];
    float Y_[STEINHART_HART_NUM_CALIB_POINTS];

    for (int i = 0; i < STEINHART_HART_NUM_CALIB_POINTS; i++) {
        struct steinhart_hart_calibration_point pt = calib_data->calib_point_arr[i];
        L_[i] = logf(pt.resistance_ohm);
        Y_[i] = 1 / (pt.temp_degrees_c + 273.15);
    }

    const float *L = L_ - 1; /* one-indexed for consistency with equations */
    const float *Y = Y_ - 1;

    const float gamma2 = (Y[2] - Y[1]) / (L[2] - L[1]);
    const float gamma3 = (Y[3] - Y[1]) / (L[3] - L[1]);

    struct steinhart_hart_coeffs coeffs = {};
    coeffs.C = (gamma3 - gamma2) / (L[3] - L[2]) / (L[1] + L[2] + L[3]);
    coeffs.B = gamma2 - coeffs.C * (L[1] * L[1] + L[1] * L[2] + L[2] * L[2]);
    coeffs.A = Y[1] - (coeffs.B + L[1] * L[1] * coeffs.C) * L[1];
    return coeffs;
}

float thermistor_resistance_read() {
    const float ntc_mv = 1.0e3 * adc_read(adc_thermistor());

    const float vdiv_resistance_ohm = 6.2e3;
    const float vdiv_vcc_mv = 3.3e3;
    return vdiv_resistance_ohm * ntc_mv / (vdiv_vcc_mv - ntc_mv);
}

float thermistor_read(struct steinhart_hart_coeffs *conv_coeffs) {
    const float ntc_resistance_ohm = thermistor_resistance_read();
    return steinhart_hart_conversion(conv_coeffs, ntc_resistance_ohm);
}
