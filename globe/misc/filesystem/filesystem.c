#include "globe.h"

void fs_read(uint32_t sector, void *buffer, size_t size) {
    DEBUG(sector, size);

    assert(size % SDCARD_BLOCK_SIZE == 0);

    peripheral_require(PERIPH_MASK_SD_CARD | PERIPH_MASK_COMM);

    sdcard_start_multiblock_read(sector);
    for (size_t offset = 0; offset < size; offset += SDCARD_BLOCK_SIZE) {
        sdcard_read_block(((char *)buffer) + offset);
    }
    sdcard_stop_multiblock_read();
}

void fs_write(uint32_t sector, const void *buffer, size_t size) {
    DEBUG(sector, size);

    assert(size % SDCARD_BLOCK_SIZE == 0);

    peripheral_require(PERIPH_MASK_SD_CARD | PERIPH_MASK_COMM);

    sdcard_start_multiblock_write(sector);
    for (size_t offset = 0; offset < size; offset += SDCARD_BLOCK_SIZE) {
        sdcard_write_block(((const char *)buffer) + offset);
    }
    sdcard_stop_multiblock_write();
}
