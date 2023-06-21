#ifndef MOTION_H
#define MOTION_H

#include "globe.h"

const struct spi_device *spi_bmx160();
const struct i2c_device *i2c_bmx160();

struct motion_reading {
    int16_t x, y, z;
};

#define MOTION_READ_MAX (1024 / 6)

void motion_start();
void motion_suspend();
size_t motion_read(struct motion_reading *buf, size_t buf_size);

#endif
