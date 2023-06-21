#ifndef LOGGING_H
#define LOGGING_H

#include "globe.h"

struct log_column_def {
    bool enabled;
    const char *field_name;
    const char *data_type;
    const char *units;
};

void log_dump();
FILE *log_start(const struct app_config *conf, const struct log_column_def *cols,
                size_t n_cols, float last_calibration_reference_ph);
void flush_log(FILE *active_log);
void print_header_line(FILE *active_log, const struct log_column_def *defs,
                       size_t n_cols, const char *header_name, size_t field_offset);
#endif
