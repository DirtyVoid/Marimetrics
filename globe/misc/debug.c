#include "globe.h"

struct debug *debug_head;

static void buf_bump(char **buf, char *buf_end, int n) {
    assert(n >= 0);
    *buf += min((int)(buf_end - *buf), n);
}

void debug_print_str(char *buf, size_t buf_size) {
    char *end = buf + buf_size;

#define DEBUG_PRINT(...) buf_bump(&buf, end, snprintf(buf, end - buf, __VA_ARGS__))

    for (const struct debug *cur = debug_head; cur; cur = cur->prev) {
        const struct debug_metadata *m = cur->metadata;
        DEBUG_PRINT("%s:%d: %s:", m->src_file, m->line, m->function);

        for (int i = 0; i < m->n_expr; i++) {
            DEBUG_PRINT(" %s=", m->expr_arr[i].str);

            switch (m->expr_arr[i].type_tag) {
#define DEBUG_PRINT_CASE(X, FMT, CAST_TYPE)                                            \
    case debug_generic_value_##X##_tag:                                                \
        DEBUG_PRINT(FMT, (CAST_TYPE)(cur->vals[i].X));                                 \
        break;
                /* The printf implementation does not support printing long
                 * long/64-bit values so they are truncated. */
                DEBUG_PRINT_CASE(u8, "%u", unsigned);
                DEBUG_PRINT_CASE(u16, "%u", unsigned);
                DEBUG_PRINT_CASE(u32, "%u", unsigned);
                DEBUG_PRINT_CASE(u64, "%u", unsigned);
                DEBUG_PRINT_CASE(u, "%u", unsigned);
                DEBUG_PRINT_CASE(i8, "%d", int);
                DEBUG_PRINT_CASE(i16, "%d", int);
                DEBUG_PRINT_CASE(i32, "%d", int);
                DEBUG_PRINT_CASE(i64, "%d", int);
                DEBUG_PRINT_CASE(i, "%d", int);
                DEBUG_PRINT_CASE(f32, "%f", float);
                DEBUG_PRINT_CASE(f64, "%f", double);
            case debug_generic_value_boolean_tag:
                DEBUG_PRINT(cur->vals[i].boolean ? "true" : "false");
                break;
            case debug_generic_value_c_str_tag:
                DEBUG_PRINT("\"");

                const char *str = cur->vals[i].c_str;
                escape_string(&str, &buf, end - buf);
                DEBUG_PRINT("\"");
                break;
            }
        }
        DEBUG_PRINT("\n");
    }
}
