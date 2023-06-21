#include "globe.h"

#define MAX_OPEN_FILES FF_FS_LOCK
#define FD_OFFSET 3

static FATFS fs;

static struct {
    bool is_open;
    FIL file;
} open_files[MAX_OPEN_FILES];

static void check_fresult(FRESULT res) { assert(res == FR_OK); }

static void ensure_mounted() {
    static bool is_mounted = false;
    if (is_mounted)
        return;
    check_fresult(f_mount(&fs, "", 0));
    is_mounted = true;
}

static int find_unallocated_file() {
    for (int i = 0; i < MAX_OPEN_FILES; ++i) {
        if (!open_files[i].is_open) {
            return i;
        }
    }
    assert(0); /* out of file slots. Increase MAX_OPEN_FILES. */
}

static bool get_and_clear(int *flags, int mask) {
    bool result = !!(*flags & mask);
    *flags &= ~mask;
    return result;
}

static BYTE get_and_clear_access_mode(int *flags) {
    int access_mode = O_ACCMODE & *flags;
    *flags &= ~O_ACCMODE;
    switch (access_mode) {
    case O_RDONLY:
        return FA_READ;
    case O_WRONLY:
        return FA_WRITE;
    case O_RDWR:
        return FA_READ | FA_WRITE;
    default:
        assert(0 && "invalid access mode");
    }
}

static BYTE newlib_to_fatfs_mode(int flags, int mode) {
    BYTE m = get_and_clear_access_mode(&flags);

    if (get_and_clear(&flags, O_APPEND))
        m |= FA_OPEN_APPEND;
    if (get_and_clear(&flags, O_CREAT))
        m |= FA_OPEN_ALWAYS;
    const bool excl = get_and_clear(&flags, O_EXCL);
    if (get_and_clear(&flags, O_TRUNC))
        m |= excl ? FA_CREATE_NEW : FA_CREATE_ALWAYS;

    assert(flags == 0);
    assert(mode == 0666);

    return m;
}

int _open(const char *name, int flags, int mode) {
    DEBUG(name, flags, mode);

    ensure_mounted();

    int i = find_unallocated_file();
    BYTE m = newlib_to_fatfs_mode(flags, mode);
    FRESULT res = f_open(&open_files[i].file, name, m);

    if (res == FR_NO_FILE || res == FR_NO_PATH) {
        /* file not found */
        errno = ENOENT;
        return -1;
    }
    if (res == FR_EXIST) {
        /* File exists */
        errno = EEXIST;
        return -1;
    }
    if (res == FR_LOCKED) {
        /* File already open */
        errno = EEXIST; /* TODO: maybe not correct errno code */
        return -1;
    }

    check_fresult(res);
    open_files[i].is_open = true;
    return i + FD_OFFSET;
}

static FIL *get_open_file(int fd) {
    int i = fd - FD_OFFSET;
    assert(i >= 0);
    assert(i < MAX_OPEN_FILES);
    assert(open_files[i].is_open);
    return &open_files[i].file;
}

int _close(int fd) {
    DEBUG(fd);

    FIL *fil = get_open_file(fd);

    check_fresult(f_close(fil));
    open_files[fd - FD_OFFSET].is_open = false;
    return 0;
}

int _unlink(char *name) {
    DEBUG(name);

    (void)name;
    ensure_mounted();
    return -1; /* Always fails */
}

int _read(int fd, char *buf, int len) {
    DEBUG(fd, len);

    FIL *fil = get_open_file(fd);

    UINT nread;
    check_fresult(f_read(fil, buf, len, &nread));

    return nread;
}

int _write(int fd, char *buf, int len) {
    DEBUG(fd, len);

    FIL *fil = get_open_file(fd);

    UINT nwritten;
    check_fresult(f_write(fil, buf, len, &nwritten));

    /* nwritten may not equan len if the volume became full during the write
     * operation or another error occurred. */
    assert(nwritten == (UINT)len);

    return nwritten;
}

int syncfs(int fd) {
    DEBUG(fd);

    FIL *fil = get_open_file(fd);
    check_fresult(f_sync(fil));

    return 0;
}
