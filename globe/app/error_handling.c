#include "globe.h"

void handle_fault() {
    const char *cause = reset_cause();

    if (cause[0] == '\0')
        /* No fault to report */
        return;

    FILE *fil = fopen("/error_log.txt", "a");
    assert(fil);
    fputs(cause, fil);
    fclose(fil);
}
