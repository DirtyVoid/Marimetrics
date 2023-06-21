#include "globe.h"

/* A global variable whose definition is marked with this macro will not be
 * reinitialized across system resets unless the system is fully powered off
 * between resets.
 */
#define PERSIST_ACROSS_RESET __attribute__((section(".persistent_data")))

extern char __persistent_data_start__, __persistent_data_end__;

#define PERSISTENT_DATA_TEST_EXPECTED_VALUE 0xDEADBEEFU

static uint32_t PERSIST_ACROSS_RESET persistent_data_test_value;
static epoch_t PERSIST_ACROSS_RESET epoch_ms;

void system_reset_init() {
    /* From Nordic Infocenter:
     *
     * "The RAM is never reset, but depending on reset source, RAM content may
     * be corrupted."
     *
     * "Unless cleared, the RESETREAS register will be cumulative. A field is
     * cleared by writing '1' to it. If none of the reset sources are flagged,
     * this indicates that the chip was reset from the on-chip reset generator,
     * which will indicate a power-on-reset or a brownout reset."
     *
     * To test if we need to clear the memory, we have a test value that is set
     * to a known value. If it does not match the expected value on startup, we
     * clear the memory.
     *
     */

    if (persistent_data_test_value == PERSISTENT_DATA_TEST_EXPECTED_VALUE) {
        /* Reset occured with RAM retention. No need to initialize persistent
         * memory. */

        /* Initialize time */
        if (epoch_ms) {
            set_epoch_ms(epoch_ms);
        }
        return;
    }

    memset(&__persistent_data_start__, 0,
           &__persistent_data_end__ - &__persistent_data_start__);

    persistent_data_test_value = PERSISTENT_DATA_TEST_EXPECTED_VALUE;
}

static char PERSIST_ACROSS_RESET reset_cause_buf[120];
static char PERSIST_ACROSS_RESET reset_debug_buf[1024];

const char *reset_cause() { return reset_cause_buf; }
const char *reset_debug_info() { return reset_debug_buf; }

static void __attribute__((noreturn)) system_reset() {

    if (is_epoch_set()) {
        epoch_ms = get_epoch_ms();
    }

    debug_print_str(reset_debug_buf, array_size(reset_debug_buf));

    sd_nvic_SystemReset();
    while (1)
        ;
}

void __assert_func(const char *file, int line, const char *function,
                   const char *failedexpr) {
    snprintf(reset_cause_buf, array_size(reset_cause_buf),
             "%s:%d: %s: Assertion `%s` failed.", file, line, function, failedexpr);
    reset_cause_buf[array_size(reset_cause_buf) - 1] = '\0';
    system_reset();
}

void HardFault_process(HardFault_stack_t *p_stack) {
    (void)p_stack;

    snprintf(reset_cause_buf, array_size(reset_cause_buf), "Hard fault (CFSR=0x%08x).",
             (unsigned)(SCB->CFSR));
    reset_cause_buf[array_size(reset_cause_buf) - 1] = '\0';
    system_reset();
}

void app_error_fault_handler(uint32_t id, uint32_t pc, uint32_t info) {
    (void)pc;

    snprintf(reset_cause_buf, array_size(reset_cause_buf),
             "nRF error (id=0x%08x,info=0x%08x).", (unsigned)id, (unsigned)info);
    reset_cause_buf[array_size(reset_cause_buf) - 1] = '\0';
    system_reset();
}

void assert_nrf_callback(uint16_t line_num, const uint8_t *p_file_name) {
    snprintf(reset_cause_buf, array_size(reset_cause_buf),
             "%s:%d: NRF assertion failed.", (const char *)p_file_name, (int)line_num);
    reset_cause_buf[array_size(reset_cause_buf) - 1] = '\0';
    system_reset();
}

void reset_into_dfu_mode() {
    sd_power_gpregret_clr(0, 0xffffffff);
    sd_power_gpregret_set(0, BOOTLOADER_DFU_START);
    nrf_pwr_mgmt_shutdown(NRF_PWR_MGMT_SHUTDOWN_GOTO_DFU);
}
