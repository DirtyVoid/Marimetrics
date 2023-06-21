#include "globe.h"

static char buf[3 + 48 / 4 + 1];
const char *const device_id = buf;

static char nibble_to_char(uint8_t val) {
    val &= 0x0f;
    return val < 0x0a ? '0' + val : 'a' + (val - 0x0a);
}

void device_id_init() {

    /* The device ID is the BLE address (prefixed with 02-). */

    uint64_t id =
        NRF_FICR->DEVICEADDR[0] + (((uint64_t)(NRF_FICR->DEVICEADDR[1])) << 32);

    /* These two bits are set in the BLE address.
     *
     * https://devzone.nordicsemi.com/f/nordic-q-a/2112/how-to-get-6-byte-mac-address-at-nrf51822#6677
     */
    id |= ((uint64_t)0x3) << 46;

    char *ptr = &buf[0];

    /* ID prefix */
    *ptr++ = '0';
    *ptr++ = '2';
    *ptr++ = '-';

    /* set ID */
    for (int i = 0; i < 48 / 4; ++i) {
        uint8_t nibble = id >> (48 - 4 * (i + 1));
        *ptr++ = nibble_to_char(nibble);
    }

    /* null-terminate string */
    *ptr++ = '\0';

    assert(ptr - buf == sizeof(buf));
}
