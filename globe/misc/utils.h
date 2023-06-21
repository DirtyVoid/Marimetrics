#ifndef UTILS_H
#define UTILS_H

#include "globe.h"

#define min(a, b)                                                                      \
    ({                                                                                 \
        __typeof__(a) _a = (a);                                                        \
        __typeof__(b) _b = (b);                                                        \
        _a < _b ? _a : _b;                                                             \
    })

#define max(a, b)                                                                      \
    ({                                                                                 \
        __typeof__(a) _a = (a);                                                        \
        __typeof__(b) _b = (b);                                                        \
        _a > _b ? _a : _b;                                                             \
    })

#define array_size(array) (sizeof(array) / sizeof(array[0]))

void escape_string(const char **str, char **out_buf, size_t out_capacity);

#define container_of(ptr, type, member)                                                \
    ({                                                                                 \
        const typeof(((type *)0)->member) *__mptr = (ptr);                             \
        (type *)((char *)__mptr - offsetof(type, member));                             \
    })

#endif
