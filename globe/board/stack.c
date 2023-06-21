#include "globe.h"

#define STACK_USAGE_DUMMY_CHAR 0xab

/* The start and end of the stack are defined in the linker script
 * (nrf_common.ld). Note that the stack grows down (from stack_top to
 * stack_bottom). */

static unsigned char *stack_bottom() { return (unsigned char *)&__StackLimit; }
static unsigned char *stack_top() { return (unsigned char *)&__StackTop; }

void stack_usage_init() {
    /* Initialize the unused part of the stack to known value.
     *
     * Note that memset is avoided here because a function call will push
     * values onto the stack which will be overwritten by this routine, leading
     * to data corruption.
     */
    register unsigned char *end = (unsigned char *)(__get_MSP());
    for (register unsigned char *it = stack_bottom(); it != end; ++it) {
        *it = STACK_USAGE_DUMMY_CHAR;
    }
}

size_t stack_capacity() { return stack_top() - stack_bottom(); }

size_t stack_maximum_usage() {
    unsigned char *ptr = stack_bottom();
    /* Perform a linear scan of the stack to find the last unused byte. */
    while (ptr != stack_top() && *ptr == STACK_USAGE_DUMMY_CHAR)
        ++ptr;
    return stack_top() - ptr;
}
