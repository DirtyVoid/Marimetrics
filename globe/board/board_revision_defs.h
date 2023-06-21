#ifndef BOARD_REVISION_DEFS_H
#define BOARD_REVISION_DEFS_H

enum board_config {
    BOARD_CONFIG_UNDEFINED = 0,
    BOARD_CONFIG_BARE = 1,
    BOARD_CONFIG_R4_2 = 2,
    BOARD_CONFIG_R4_3 = 3,
    BOARD_CONFIG_DEV = 4,
};

static inline enum board_config board_revision() {
    return ((enum board_config) * (const unsigned *)(0x10001080));
}

#endif
