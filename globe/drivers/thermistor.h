#ifndef THERMISTOR_H
#define THERMISTOR_H

#include "globe.h"

struct steinhart_hart_coeffs {
    float A, B, C;
};

#define STEINHART_HART_NUM_CALIB_POINTS 3

struct steinhart_hart_calibration_data {
    struct steinhart_hart_calibration_point {
        float temp_degrees_c, resistance_ohm;
    } calib_point_arr[STEINHART_HART_NUM_CALIB_POINTS];
};

struct steinhart_hart_coeffs steinhart_hart_coeffs_calculate(
    const struct steinhart_hart_calibration_data *calib_data);

const struct adc_channel *adc_thermistor();

float thermistor_resistance_read();

/* Return the calculated thermistor temperature in degrees C.  */
float thermistor_read(struct steinhart_hart_coeffs *conv_coeffs);

#endif
