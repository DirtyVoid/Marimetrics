#ifndef SKY66112_H
#define SKY66112_H

struct sky66112 {
    int cps_gpio_idx;
    int crx_gpio_idx;
    int ctx_gpio_idx;
};

void sky66112_init(const struct sky66112 *dev);

#endif
