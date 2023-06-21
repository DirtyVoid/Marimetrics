#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include "globe.h"

void fs_read(uint32_t sector, void *buffer, size_t size);
void fs_write(uint32_t sector, const void *buffer, size_t size);

#endif
