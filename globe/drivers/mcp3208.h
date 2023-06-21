#ifndef MCP3208_H
#define MCP3208_H

#include "globe.h"

const struct spi_device *spi_mcp3208();

uint16_t mcp3208_single_ended_conversion(int channel);
uint16_t mcp3208_differential_conversion(int channel);

#endif
