#ifndef LC709203F_H
#define LC709203F_H

#include "globe.h"

const struct i2c_device *i2c_lc709203f();

void lc709203f_init(int capacity_mah);

/* voltage (mV) */
int lc709203f_read_voltage();

/* relative state of charge (%) */
int lc709203f_read_rsoc();

/* calibrate the fuel gauge. This should be run periodically for accurate RSOC
 * reporting. */
void lc709203f_calibrate();

#endif
