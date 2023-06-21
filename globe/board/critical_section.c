#include "globe.h"

extern inline void memory_barrier();

void wait_for_event() {
    nrf_pwr_mgmt_run();
    memory_barrier();
}

void critical_section_enter(uint8_t *nest_context) {
    app_util_critical_region_enter(SOFTDEVICE_PRESENT ? nest_context : NULL);
    memory_barrier();
}

void critical_section_cleanup(uint8_t *nest_context) {
    app_util_critical_region_exit(*nest_context);
    memory_barrier();
}
