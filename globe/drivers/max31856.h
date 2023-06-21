#ifndef MAX31856_H_
#define MAX31856_H_

#include "globe.h"

const struct spi_device *spi_max31856();

void max31856_init();
float max31856_read();

#endif
