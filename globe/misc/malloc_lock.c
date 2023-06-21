#include "globe.h"

static int depth = 0;
static uint8_t context = 0;

void __malloc_lock(struct _reent *reent) {
    (void)reent;

    bool should_lock;

    {
        CRITICAL_SCOPE();
        should_lock = (depth == 0);
        depth++;
    }

    if (should_lock) {
        critical_section_enter(&context);
    }
}

void __malloc_unlock(struct _reent *reent) {
    (void)reent;

    bool should_unlock;

    {
        CRITICAL_SCOPE();
        should_unlock = (depth == 1);
        depth--;
    }

    if (should_unlock) {
        critical_section_cleanup(&context);
    }
}
