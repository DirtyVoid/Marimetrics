#include "globe.h"

static void fdoem_read_test() {
    peripheral_require(PERIPH_MASK_COMM | PERIPH_MASK_FDOEMO2);

    fdoem_read(fdoem_o2, 20.0f, 28.0f);

    peripheral_unrequire(PERIPH_MASK_COMM | PERIPH_MASK_FDOEMO2);
}

static void max31856_read_test() {
    peripheral_require(PERIPH_MASK_COMM | PERIPH_MASK_MAX31856);

    max31856_read();

    peripheral_unrequire(PERIPH_MASK_COMM | PERIPH_MASK_MAX31856);
}

static void simple_storage_test() {
    FILE *w_file = fopen("test_file.txt", "w");
    assert(w_file);

    const char *test_string = "hello world\n";
    size_t test_string_len = strlen(test_string);

    for (int i = 0; i < 5; i++) {
        fputs(test_string, w_file);
    }

    fclose(w_file);

    FILE *r_file = fopen("test_file.txt", "r");

    for (int i = 0; i < 5; i++) {
        char buf[36];
        assert(array_size(buf) > test_string_len);
        char *out = fgets(buf, test_string_len + 1, r_file);
        assert(out == buf);
        assert(strcmp(buf, test_string) == 0);
    }

    fclose(r_file);
}

static void large_write_test() {
    FILE *w_file = fopen("test_file.txt", "w");
    assert(w_file);

    char buffer[512];
    memset(buffer, 0xab, array_size(buffer));

    for (int j = 0; j < 2 * 1024; j++) { /* Write 1mB = 2 * 1024 * 512 bytes */
        size_t n = fwrite(buffer, 1, array_size(buffer), w_file);
        assert(n == array_size(buffer));
    }

    fclose(w_file);
}

static void power_management_test() {
    for (int i = 0; i < 10; i++) {
        periph_mask_t sd = PERIPH_MASK_SD_CARD, sens = PERIPH_MASK_FDOEMO2;

        power_management_configure(sd);
        power_management_configure(sd | sens);
        power_management_configure(0);

        power_management_configure(sens);
        power_management_configure(sd | sens);
        power_management_configure(0);

        power_management_configure(sd | sens);
        power_management_configure(sens);
        power_management_configure(0);

        power_management_configure(sd | sens);
        power_management_configure(sd);
        power_management_configure(0);
    }
}

static void storage_test_with_power_cycling() {
    FILE *w_file = fopen("test_file.txt", "w");
    peripheral_unrequire(PERIPH_MASK_COMM | PERIPH_MASK_SD_CARD);
    assert(w_file);

    const char *test_string = "hello world\n";
    size_t test_string_len = strlen(test_string);

    for (int i = 0; i < 5; i++) {
        fputs(test_string, w_file);
        peripheral_unrequire(PERIPH_MASK_COMM | PERIPH_MASK_SD_CARD);
    }

    fclose(w_file);
    peripheral_unrequire(PERIPH_MASK_COMM | PERIPH_MASK_SD_CARD);

    FILE *r_file = fopen("test_file.txt", "r");

    for (int i = 0; i < 5; i++) {
        char buf[36];
        assert(array_size(buf) > test_string_len);
        char *out = fgets(buf, test_string_len + 1, r_file);
        peripheral_unrequire(PERIPH_MASK_COMM | PERIPH_MASK_SD_CARD);
        assert(out == buf);
        assert(strcmp(buf, test_string) == 0);
    }

    fclose(r_file);
}

static void power_test(periph_mask_t enable_mask) {
    /* This test should be stepped through with a scope to ensure that
     *
     * 1. There is no voltage dip on power rail
     *
     * 2. The input to the MOSFET follows a PWM ramp
     *
     * 3. MOSFET output stabilizes quickly
     */

    power_management_configure(0); /* Should be unpowered */

    /* Check for ramp. There should not be a dip on VCC */
    power_management_configure(enable_mask);

    /* Nothing should happen */
    power_management_configure(enable_mask);

    /* Turn off */
    power_management_configure(0);
    /* Nothing should happen */
    power_management_configure(0);
}

void run_unit_tests() {
    board_test();

    power_test(PERIPH_MASK_SD_CARD);
    power_test(PERIPH_MASK_FDOEMO2);
    fdoem_read_test();
    simple_storage_test();
    storage_test_with_power_cycling();
    max31856_read_test();
}

void run_stress_tests() {

    /* TODO: initialize BLE */

    for (int i = 0; i < 50; i++) {
        peripheral_unrequire(PERIPH_MASK_COMM | PERIPH_MASK_FDOEMO2 |
                             PERIPH_MASK_MAX31856 | PERIPH_MASK_SD_CARD);

        fdoem_read_test();
        large_write_test();
        max31856_read_test();
        power_management_test();
    }

    assert(0 && "test complete");
}
