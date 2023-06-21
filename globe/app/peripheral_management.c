#include "globe.h"

static void peripheral_configure(periph_mask_t require_mask,
                                 periph_mask_t unrequire_mask) {

    static periph_mask_t initialized_mask;
    static periph_mask_t required_mask;
    DEBUG(require_mask, unrequire_mask, initialized_mask, required_mask);

    periph_mask_t new_required_mask = (required_mask | require_mask) & ~unrequire_mask;
    periph_mask_t powered_mask = power_management_configure(new_required_mask);
    DEBUG(powered_mask);
    assert((powered_mask & new_required_mask) == new_required_mask);
    periph_mask_t uninit = powered_mask & ~initialized_mask; /* uninitialized */
    periph_mask_t newly_required_mask = new_required_mask & ~required_mask;
    required_mask = new_required_mask;
    /* Unpowered peripherals are no longer initialized */
    initialized_mask &= powered_mask;

    epoch_t t0 = elapsed_ms();

    /* SD card must be initialized as soon as it is powered because it starts
     * in SD mode and any communication over the SPI lines while in this mode
     * can compromise the state of the card. */
    if (uninit & PERIPH_MASK_SD_CARD) {
        /* After supply voltage reaches 2.2V, wait for 1 millisecond at
         * least. */
        wait_until(t0 + 500);
        sdcard_init();
        initialized_mask |= PERIPH_MASK_SD_CARD;
    }

    /* If the BMX160 is required it can be started, otherwise it should be in
     * the suspend state. Note that the IC is initialised in suspend state but
     * it may not be in this state if the uC experienced a reset while the
     * BMX160 was running. */
    periph_mask_t bmx160_mask = (PERIPH_MASK_BMX160_SPI | PERIPH_MASK_BMX160_I2C);
    if (newly_required_mask & bmx160_mask) {
        wait_until(t0 + 5); /* Delay determined empirically */
        motion_start();
        initialized_mask |= bmx160_mask;
    } else if (uninit & bmx160_mask) {
        wait_until(t0 + 5); /* Delay determined empirically */
        motion_suspend();
        initialized_mask |= bmx160_mask;
    }

    /* The FDOEM devices should only be communicated with if they are required
     * because they may not be populated. */
    if (newly_required_mask & PERIPH_MASK_FDOEMO2) {
        /* This delay is required for the FD-OEM to startup. It was determined
         * empirically. */
        wait_until(t0 + 750);
        fdoem_init(fdoem_o2);
        initialized_mask |= PERIPH_MASK_FDOEMO2;
    }

    if (newly_required_mask & PERIPH_MASK_FDOEMPH) {
        wait_until(t0 + 750);
        fdoem_init(fdoem_ph);
        initialized_mask |= PERIPH_MASK_FDOEMPH;
    }

    if (newly_required_mask & PERIPH_MASK_MAX31856) {
        wait_until(t0 + 5); /* Delay determined empirically */
        max31856_init();
        initialized_mask |= PERIPH_MASK_MAX31856;
    }

    if (newly_required_mask & PERIPH_MASK_THERMISTOR) {
        initialized_mask |= PERIPH_MASK_THERMISTOR;
    }

    if (newly_required_mask & PERIPH_MASK_89BSD) {
        wait_until(t0 + 750);
        _89bsd_init();
        initialized_mask |= PERIPH_MASK_89BSD;
    }
}

void peripheral_require(periph_mask_t require_mask) {
    peripheral_configure(require_mask, 0);
}

void peripheral_unrequire(periph_mask_t unrequire_mask) {
    peripheral_configure(0, unrequire_mask);
}
