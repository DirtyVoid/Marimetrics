#ifndef _89BSD_H
#define _89BSD_H

#include "globe.h"

const struct i2c_device *i2c_89bsd();

struct _89bsd_params {
    int16_t c0, c1, c2, c3, c4, c5, c6;
    int16_t a0, a1, a2;
};

extern struct _89bsd_params _89bsd_params;

void _89bsd_init();

struct _89bsd_reading {
    uint32_t d1, d2;
    float temp;
    float y;
    float p;
    float pressure; /* final reading */
};

struct _89bsd_reading _89bsd_read();

#endif
