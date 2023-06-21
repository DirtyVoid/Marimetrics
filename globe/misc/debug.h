#ifndef DEBUG_H
#define DEBUG_H

#include "globe.h"

#define DEBUG_GENERIC_VALUE_EXPAND(M)                                                  \
    M(u8)                                                                              \
    M(u16)                                                                             \
    M(u32) M(u64) M(u) M(i8) M(i16) M(i32) M(i64) M(i) M(f32) M(f64) M(boolean) M(c_str)

struct debug_metadata {
    const char *src_file;
    int line;
    const char *function;
    int n_expr;
    struct debug_metadata_expr {
        enum debug_metadata_expr_type_tag {
#define DEBUG_GENERIC_VALUE_TAG_GEN(X) debug_generic_value_##X##_tag,
            DEBUG_GENERIC_VALUE_EXPAND(DEBUG_GENERIC_VALUE_TAG_GEN)
        } type_tag;
        const char *str;
    } expr_arr[];
};

typedef uint8_t debug_generic_value_u8_type;
typedef uint16_t debug_generic_value_u16_type;
typedef uint32_t debug_generic_value_u32_type;
typedef uint64_t debug_generic_value_u64_type;
typedef unsigned debug_generic_value_u_type;

typedef int8_t debug_generic_value_i8_type;
typedef int16_t debug_generic_value_i16_type;
typedef int32_t debug_generic_value_i32_type;
typedef int64_t debug_generic_value_i64_type;
typedef int debug_generic_value_i_type;

typedef float debug_generic_value_f32_type;
typedef double debug_generic_value_f64_type;

typedef bool debug_generic_value_boolean_type;

typedef const char *debug_generic_value_c_str_type;

union debug_generic_value {
#define DEBUG_GENERIC_VALUE_MEM_GEN(X) debug_generic_value_##X##_type X;
    DEBUG_GENERIC_VALUE_EXPAND(DEBUG_GENERIC_VALUE_MEM_GEN);
};

#define DEBUG_GENERIC_FUNC_GEN(X)                                                      \
    static inline union debug_generic_value debug_generic_value_##X(                   \
        debug_generic_value_##X##_type x) {                                            \
        return ((union debug_generic_value){.X = x});                                  \
    }
DEBUG_GENERIC_VALUE_EXPAND(DEBUG_GENERIC_FUNC_GEN);

struct debug {
    struct debug *prev;
    const struct debug_metadata *metadata;
    union debug_generic_value vals[];
};

extern struct debug *debug_head;

static inline void cleanup_debug(void *debug_ptr) {
    debug_head = ((struct debug *)debug_ptr)->prev;
}

#define DEBUG_2_1_2_1(X) #X
#define DEBUG_2_1_2(X) DEBUG_2_1_2_1(X)
#define DEBUG_2_1_1(X) debug_generic_value_##X##_type : debug_generic_value_##X##_tag,
#define DEBUG_2_1(r, data, XX)                                                         \
    {                                                                   \
        .type_tag = _Generic(                                           \
            (XX),                                                       \
            DEBUG_GENERIC_VALUE_EXPAND(DEBUG_2_1_1)                     \
            char *: debug_generic_value_c_str_tag),                     \
            .str = DEBUG_2_1_2(XX)                                      \
            },

#define DEBUG_2_2_1(X) debug_generic_value_##X##_type : debug_generic_value_##X,
#define DEBUG_2_2(r, data, X)                                                          \
    _Generic(                                                           \
        (X),                                                            \
        DEBUG_GENERIC_VALUE_EXPAND(DEBUG_2_2_1)                         \
        char *: debug_generic_value_c_str)(X),

#define DEBUG_2(SEQ)                                                                   \
    static const struct {                                                              \
        struct debug_metadata header;                                                  \
        struct debug_metadata_expr exprs[BOOST_PP_SEQ_SIZE(SEQ)];                      \
    } BOOST_PP_CAT(_debug_metadata_,                                                   \
                   __LINE__) = {.header =                                              \
                                    {                                                  \
                                        .src_file = __FILE__,                          \
                                        .line = __LINE__,                              \
                                        .function = __func__,                          \
                                        .n_expr = BOOST_PP_SEQ_SIZE(SEQ),              \
                                    },                                                 \
                                .exprs = {BOOST_PP_SEQ_FOR_EACH(DEBUG_2_1, , SEQ)}     \
                                                                                       \
    };                                                                                 \
    struct {                                                                           \
        struct debug header;                                                           \
        union debug_generic_value vals[BOOST_PP_SEQ_SIZE(SEQ)];                        \
    } BOOST_PP_CAT(_debug_, __LINE__) __attribute__((cleanup(cleanup_debug))) = {      \
        .header = {.prev = debug_head,                                                 \
                   .metadata = &BOOST_PP_CAT(_debug_metadata_, __LINE__).header},      \
        .vals = {BOOST_PP_SEQ_FOR_EACH(DEBUG_2_2, , SEQ)},                             \
    };                                                                                 \
                                                                                       \
    debug_head = &BOOST_PP_CAT(_debug_, __LINE__).header;

#define DEBUG(...) DEBUG_2(BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))

void debug_print_str(char *buf, size_t buf_size);

#endif
