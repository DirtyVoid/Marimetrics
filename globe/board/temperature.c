#include "globe.h"

int board_temperature() {
    int32_t temp;
    uint32_t err = sd_temp_get(&temp);
    assert(!err);
    return temp;
}
