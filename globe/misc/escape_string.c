#include "globe.h"

void escape_string(const char **str, char **out_buf, size_t out_capacity) {
    /* Can not add '\0' to out string so break immediately */
    if (out_capacity == 0)
        return;

    const char *in = *str;
    char *out = *out_buf;

    char *end = out + out_capacity - 1; /* leave 1 for \0 */

    static const struct {
        char c;
        char r;
    } escaped_chars[] = {
        {'"', '"'},  {'\\', '\\'}, {'\b', 'b'}, {'\f', 'f'},
        {'\n', 'n'}, {'\r', 'r'},  {'\t', 't'},
    };

    for (; *in; in++) {
        size_t rem = end - out;
        for (size_t i = 0; i < array_size(escaped_chars); i++) {
            if (*in != escaped_chars[i].c)
                continue;
            if (rem < 2)
                goto exit;
            *out++ = '\\';
            *out++ = escaped_chars[i].r;
            goto next;
        }

        if (isprint((int)*in)) {
            if (rem < 1)
                goto exit;
            *out++ = *in;
        } else {
            if (rem < 4)
                goto exit;
            out += sprintf(out, "\\x%02x", (unsigned char)*in);
        }

    next:;
    }

exit:
    *out = '\0';
    *str = in;
    *out_buf = out;
}
