#ifndef SDCARD_H
#define SDCARD_H

#include "globe.h"

const struct spi_device *spi_sdcard();

#define SDCARD_BLOCK_SIZE 512

/* Initializes the SD card in SPI mode.
 *
 * Note that an SD card is not initialized in SPI mode when it is powered on.
 * Any traffic over the SPI bus prior to initialization of the SD card may
 * interfere with the SD card. The SD card should therefore be initialized
 * immediately after it is powered on prior to any other communication over a
 * multi-slave SPI bus.
 */
void sdcard_init();

void sdcard_start_multiblock_read(uint32_t addr);
void sdcard_stop_multiblock_read();
void sdcard_read_block(void *data_buffer);

void sdcard_start_multiblock_write(uint32_t addr);
void sdcard_stop_multiblock_write();
void sdcard_write_block(const void *data_buffer);

#endif
