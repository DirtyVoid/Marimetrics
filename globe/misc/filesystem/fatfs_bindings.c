#include "globe.h"

DSTATUS disk_status(BYTE pdrv) {
    assert(pdrv == 0);
    return 0;
}

DSTATUS disk_initialize(BYTE pdrv) {
    assert(pdrv == 0);
    return 0;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count) {
    assert(pdrv == 0);
    assert(FF_MIN_SS == 512);
    assert(FF_MAX_SS == 512);
    fs_read(sector, buff, 512 * count);
    return RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count) {
    assert(pdrv == 0);
    assert(FF_MIN_SS == 512);
    assert(FF_MAX_SS == 512);
    fs_write(sector, buff, 512 * count);
    return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff) {
    (void)buff;
    assert(pdrv == 0);
    switch (cmd) {
    case CTRL_SYNC:
        break;
    case GET_BLOCK_SIZE:   /* fallthrough */
    case GET_SECTOR_SIZE:  /* fallthrough */
    case GET_SECTOR_COUNT: /* fallthrough */
    case CTRL_TRIM:
        assert(0 && "not implemented");
        break;
    default:
        assert(0 && "Invalid ioctl command");
    }

    return RES_OK;
}

DWORD get_fattime(void) {

    /* the tm structure is loaded with some defaults in case the time has
     * not yet been programmed. */
    struct tm tm = {
        .tm_year = 2000,
        .tm_mon = 0,
        .tm_mday = 1,
        .tm_hour = 0,
        .tm_min = 0,
        .tm_sec = 0,
    };

    if (is_epoch_set()) {
        /* convert from ms to s */
        const time_t timep = get_epoch_ms() / 1000;

        /* convert from epoch to time structure */
        localtime_r(&timep, &tm);
    }

    int Y = (tm.tm_year + 1900) - 1980; /* years since 1980 */
    int M = tm.tm_mon + 1;              /* 1 .. 12 */
    int D = tm.tm_mday;                 /* 1 .. 31 */
    int h = tm.tm_hour;                 /* 0 .. 23 */
    int m = tm.tm_min;                  /* 0 .. 59 */
    int s = tm.tm_sec;                  /* 0 .. 59 */

    return (Y << 25) | (M << 21) | (D << 16) | (h << 11) | (m << 5) | (s << 0);
}
