#ifndef PERIPHERAL_MANAGEMENT_H
#define PERIPHERAL_MANAGEMENT_H

#include "globe.h"

/* These functions control power and initialization of peripherals. Using
 * peripheral_require, peripherals in the require_mask will be powered on and
 * initialized if they have not been so already. After the peripherals are
 * finished being used, peripheral_unrequire may be used to mark the
 * peripherals as unrequired so they can be powered down.
 *
 * When the required peripherals are powered, additional peripherals may also
 * be powered at the same time if they share a common power rail with the
 * required peripherals. These peripherals are also initialized after they are
 * provided power. Likewise, an unrequired peripheral may not be powered down
 * if it shares a power rail with a required peripheral.
 *
 * If a peripheral is not populated, then it should not be initialized.
 * peripheral_indicate_populated must be called to mask each populated
 * peripheral to indicate it is populated. This call will immediately
 * initialize the peripherals if they are powered at the time of the call.
 */

void peripheral_require(periph_mask_t require_mask);
void peripheral_unrequire(periph_mask_t unrequire_mask);

#endif
