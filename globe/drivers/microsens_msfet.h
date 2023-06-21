#ifndef MICROSENS_MSFET_H
#define MICROSENS_MSFET_H

#include "globe.h"

enum msfet_channel {
    MSFET_PH_1,  /* units: pH */
    MSFET_K_1,   /* units: Molarity (M) */
    MSFET_NA_1,  /* units: Molarity (M) */
    MSFET_PH_2,  /* units: pH */
    MSFET_NO3_1, /* units: Molarity (M) */
    MSFET_N_CHANS,
};

struct msfet_params {
    struct msfet_channel_params {
        float scale;
        float offset;
        float t_dep;
    } channel_params[MSFET_N_CHANS];
};

struct msfet_reading {
    uint16_t adc_count;
    float v_sg_mv;
    float concentration;
};

struct msfet_reading msfet_read(const struct msfet_params *params,
                                enum msfet_channel chan, float temp_deg_c);

#endif
