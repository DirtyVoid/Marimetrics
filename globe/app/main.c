#include "globe.h"

#define ADC_BUFFER_SIZE 383

static void init_clk_timer(NRF_TIMER_Type *clk_timer) {
    /* The TIMER is set in TIMER mode (as opposed to counter mode), so it
     * periodically increments. */
    nrf_timer_mode_set(clk_timer, NRF_TIMER_MODE_TIMER);
    nrf_timer_bit_width_set(clk_timer, NRF_TIMER_BIT_WIDTH_32);
    /* The timer is configured to run at 2MHz. */
    nrf_timer_frequency_set(clk_timer, NRF_TIMER_FREQ_2MHz);
    /* Capture/Compare (CC) register 0 is configured to clear the timer at a
     * 200kHz frequency (2Mhz / 10 = 200kHz) */
    clk_timer->CC[0] = 10;
    /* The short resets the counter to zero after each period */
    nrf_timer_shorts_enable(clk_timer, NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK);
    /* CC[1] is triggered half way through a period */
    clk_timer->CC[1] = 5;
}

static void init_counter(NRF_TIMER_Type *counter, int num_clk_periods_st_high) {
    /* The counter is triggered from the CLK TIMER. */
    nrf_timer_mode_set(counter, NRF_TIMER_MODE_COUNTER);
    nrf_timer_bit_width_set(counter, NRF_TIMER_BIT_WIDTH_32);
    /* CC[0] is triggered after a given number of CLK periods */
    counter->CC[0] = num_clk_periods_st_high;
}

static void init_clk_gpiote(uint32_t clk_pin, int clk_gpiote_channel) {
    /* The CLK GPIOTE toggles the CLK line. */
    nrf_gpiote_task_configure(clk_gpiote_channel, clk_pin,
                              GPIOTE_CONFIG_POLARITY_Toggle,
                              NRF_GPIOTE_INITIAL_VALUE_LOW);
    nrf_gpiote_task_enable(clk_gpiote_channel);
}

static void init_st_gpiote(int st_pin, int st_gpiote_channel) {
    /* ST GPIOTE brings the ST pin from high to low */
    nrf_gpiote_task_configure(st_gpiote_channel, st_pin, NRF_GPIOTE_POLARITY_HITOLO,
                              NRF_GPIOTE_INITIAL_VALUE_HIGH);
    nrf_gpiote_task_enable(st_gpiote_channel);
}

static void init_adc(nrf_saadc_input_t video_pin) {

    const nrf_saadc_channel_config_t channel_config = {
        .resistor_p =
            NRF_SAADC_RESISTOR_DISABLED, /* Resistor value on positive input. */
        .resistor_n =
            NRF_SAADC_RESISTOR_DISABLED, /* Resistor value on negative input. */
        .gain = NRF_SAADC_GAIN1_6,       /* Gain control value. */
        .reference = NRF_SAADC_REFERENCE_INTERNAL, /* Reference control value. */
        .acq_time = NRF_SAADC_ACQTIME_3US,         /* Acquisition time. */
        .mode =
            NRF_SAADC_MODE_SINGLE_ENDED, /* SAADC mode. Single-ended or differential. */
        .burst = NRF_SAADC_BURST_DISABLED, /* Burst mode configuration. */
        .pin_p = video_pin,                /* Input positive pin selection. */
        .pin_n = NRF_SAADC_INPUT_DISABLED, /* Input negative pin selection. */
    };

    nrf_saadc_channel_init(0, &channel_config);
    nrf_saadc_int_enable(NRF_SAADC_INT_END);
}

static void start_adc(int16_t *buffer, int buffer_size) {
    nrf_saadc_enable();
    nrf_saadc_buffer_init(buffer, buffer_size);
    nrf_saadc_event_clear(NRF_SAADC_EVENT_END);
    nrf_saadc_task_trigger(NRF_SAADC_TASK_START);
}

static void stop_adc() { nrf_saadc_disable(); }

static void init_end_clk_ppi_channel(int end_clk_ppi_channel, NRF_TIMER_Type *clk_timer,
                                     NRF_TIMER_Type *counter, int clk_gpiote_channel) {
    /* Triggered at end of CLK period */
    NRF_PPI->CH[end_clk_ppi_channel].EEP = (uint32_t)&clk_timer->EVENTS_COMPARE[0];
    /* Trigger CLK toggle at end of CLK period */
    NRF_PPI->CH[end_clk_ppi_channel].TEP =
        (uint32_t)&NRF_GPIOTE->TASKS_OUT[clk_gpiote_channel];
    /* Trigger counter increment at end of CLK period */
    NRF_PPI->FORK[end_clk_ppi_channel].TEP =
        (uint32_t)nrf_timer_task_address_get(counter, NRF_TIMER_TASK_COUNT);

    nrf_ppi_channel_enable(end_clk_ppi_channel);
}

static void init_mid_clk_ppi_channel(int mid_clk_ppi_channel, NRF_TIMER_Type *clk_timer,
                                     int clk_gpiote_channel) {
    /* Triggered at middle of CLK period */
    NRF_PPI->CH[mid_clk_ppi_channel].EEP = (uint32_t)&clk_timer->EVENTS_COMPARE[1];

    /* Trigger CLK toggle at middle of CLK period */
    NRF_PPI->CH[mid_clk_ppi_channel].TEP =
        (uint32_t)&NRF_GPIOTE->TASKS_OUT[clk_gpiote_channel];
    /* Trigger ADC sample at middle of CLK period */
    NRF_PPI->FORK[mid_clk_ppi_channel].TEP =
        nrf_saadc_task_address_get(NRF_SAADC_TASK_SAMPLE);

    nrf_ppi_channel_enable(mid_clk_ppi_channel);
}

static void init_counter_ppi_channel(int counter_ppi_channel, NRF_TIMER_Type *counter,
                                     int st_gpiote_channel) {
    /* Triggered at ST counter trigger value */
    NRF_PPI->CH[counter_ppi_channel].EEP = (uint32_t)&counter->EVENTS_COMPARE[0];
    /* Triggle set ST low */
    NRF_PPI->CH[counter_ppi_channel].TEP =
        (uint32_t)&NRF_GPIOTE->TASKS_OUT[st_gpiote_channel];

    nrf_ppi_channel_enable(counter_ppi_channel);
}

static void start_nrf_timer(NRF_TIMER_Type *timer) {
    timer->TASKS_CLEAR = 1;
    timer->TASKS_START = 1;
}

static void stop_nrf_timer(NRF_TIMER_Type *timer) { timer->TASKS_STOP = 1; }

void read_spec(int num_clk_periods_st_high, int16_t *buffer, int buffer_size) {
    int clk_pin = header_gpio_3_pin();
    nrf_saadc_input_t video_pin = NRF_SAADC_INPUT_AIN1;
    int st_pin = header_gpio_4_pin();

    NRF_TIMER_Type *clk_timer = NRF_TIMER3;
    NRF_TIMER_Type *counter = NRF_TIMER4;

    int clk_gpiote_channel = 0;
    int st_gpiote_channel = 1;

    int end_clk_ppi_channel = 0;
    int mid_clk_ppi_channel = 1;
    int counter_ppi_channel = 2;

    init_clk_timer(clk_timer);
    init_counter(counter, num_clk_periods_st_high);
    init_clk_gpiote(clk_pin, clk_gpiote_channel);
    init_st_gpiote(st_pin, st_gpiote_channel);
    init_adc(video_pin);
    init_end_clk_ppi_channel(end_clk_ppi_channel, clk_timer, counter,
                             clk_gpiote_channel);
    init_mid_clk_ppi_channel(mid_clk_ppi_channel, clk_timer, clk_gpiote_channel);
    init_counter_ppi_channel(counter_ppi_channel, counter, st_gpiote_channel);

    start_adc(buffer, buffer_size);
    start_nrf_timer(counter);
    start_nrf_timer(clk_timer);

    while (!nrf_saadc_event_check(NRF_SAADC_EVENT_END))
        ;
    stop_nrf_timer(clk_timer);
    stop_nrf_timer(counter);
    stop_adc();
    nrf_gpiote_task_disable(clk_gpiote_channel);
    nrf_ppi_channel_disable(end_clk_ppi_channel);
    nrf_ppi_channel_disable(mid_clk_ppi_channel);
    nrf_gpiote_task_disable(st_gpiote_channel);
    nrf_ppi_channel_disable(counter_ppi_channel);
}
/* START PRINT FUNCTIONS
 * **************************************************************************/
void populate_log_column(FILE *f) {

    struct log_column_def log_col[ADC_BUFFER_SIZE-94];
    char field_name_str[ADC_BUFFER_SIZE-94][strlen("pixel_xxx") + 1];

    for (int i = 0; i < ADC_BUFFER_SIZE-94; i++) {
        log_col[i].enabled = true;
        log_col[i].data_type = "f";
        log_col[i].units = "counts";
        snprintf(field_name_str[i], sizeof(field_name_str[i]), "pixel_%d", i);
        log_col[i].field_name = (const char *)field_name_str[i];
    }
    print_header_line(f, log_col, array_size(log_col), "FIELD_NAMES",
                      offsetof(struct log_column_def, field_name));
    print_header_line(f, log_col, array_size(log_col), "FIELD_DATA_TYPES",
                      offsetof(struct log_column_def, data_type));
    print_header_line(f, log_col, array_size(log_col), "FIELD_UNITS",
                      offsetof(struct log_column_def, units));
}

void create_log_headers(FILE *active_log) {

    fprintf(active_log, "#DATA_FILE_REVISION=C\n");
    fprintf(active_log, "#DELIMITER=,\n");
    fprintf(active_log, "#FIRMWARE_REVISION=%s\n", revision);
    fprintf(active_log, "#FIRMWARE_BUILD_TYPE=%s\n", build_type);
    fprintf(active_log, "#FIRMWARE_APPLICATION=%s\n", application);
    fprintf(active_log, "#DEVICE_ID=%s\n", device_id);
    fprintf(active_log, "#LABEL=TEST\n");
    fprintf(active_log, "#SAMPLE_PERIOD_SECONDS=1_SAADC_READ_CYCLE\n");
    fprintf(active_log, "#MAXIMUM_STACK_USAGE=%d\n", (int)stack_maximum_usage());
    fprintf(active_log, "#STACK_CAPACITY=%d\n", (int)stack_capacity());
    fprintf(active_log, "#RESET_CAUSE=%s\n", reset_cause());
    populate_log_column(active_log);
    fprintf(active_log, "#AVERAGING_IN_SCANS=1\n");
    fprintf(active_log, "#INTEGRATION_TIME_IN_MICROSECONDS=27\n");
    fprintf(active_log, "#IMAGE_SENSOR_SERIAL_NUMBER=22C03696\n");
    fprintf(active_log, "#IMAGE_SENSOR_WAVELENGTH_CALIBRATION_NUMBER=683012\n");
    fprintf(active_log,
            "#IMAGE_SENSOR_WAVELENGTH_CALIBRATION_COEFFICIENT_A0=311.160481905306\n");
    fprintf(active_log,
            "#IMAGE_SENSOR_WAVELENGTH_CALIBRATION_COEFFICIENT_B1=2.70309514936651\n");
    fprintf(
        active_log,
        "#IMAGE_SENSOR_WAVELENGTH_CALIBRATION_COEFFICIENT_B2=-0.00101535273152902\n");
    fprintf(
        active_log,
        "#IMAGE_SENSOR_WAVELENGTH_CALIBRATION_COEFFICIENT_B3=-9.53443884696264E-06\n");
    fprintf(
        active_log,
        "#IMAGE_SENSOR_WAVELENGTH_CALIBRATION_COEFFICIENT_B4=1.74621658666512E-08\n");
    fprintf(
        active_log,
        "#IMAGE_SENSOR_WAVELENGTH_CALIBRATION_COEFFICIENT_B5=-6.89368048459595E-12\n");
    fprintf(active_log,
            "#IMAGE_SENSOR_WAVELENGTH_CALIBRATION_TEMPERATURE_IN_DEG_C=24\n");
    fprintf(active_log, "#IMAGE_SENSOR_WAVELENGTH_RESOLUTION_IN_NANOMETER=10.1\n");
    fprintf(active_log, "#IMAGE_SENSOR_WAVELENGTH_CALIBRATION_DATE=2022/04/07\n");
    fprintf(active_log, "#IMAGE_SENSOR_DARK_CURRENT_CALIBRATION_DATE=\n");
    fprintf(active_log, "#IMAGE_SENSOR_DARK_CURRENT_CALIBRATION_DATA=\n");
}

void create_data_line(FILE *log_pointer, int16_t buff[]) {
    for (int i = 93; i < ADC_BUFFER_SIZE - 2; i++) {
        fprintf(log_pointer, "%d,", (int)buff[i]);
    }
    fprintf(log_pointer, "%d\n", (int)buff[ADC_BUFFER_SIZE - 2]);
}

/* END PRINT FUNCTIONS
 * **************************************************************************/

int main(void) {

    board_init();

    extern bool fdoems_plugged_in;
    fdoems_plugged_in = false;

    peripheral_require(PERIPH_MASK_COMM | PERIPH_MASK_THERMISTOR);

    FILE *log_file = fopen("log_laser_650nm.csv", "w+");
    assert(log_file != NULL); 


    static int16_t buffer[ADC_BUFFER_SIZE] = {};

    int num_clk_periods_st_high = 5;
    create_log_headers(log_file);
    for (int n = 0; n < 10; n++) {
        read_spec(num_clk_periods_st_high, buffer, ADC_BUFFER_SIZE);
        create_data_line(log_file, buffer);
    }

    int res = fclose(log_file);
    assert(res == 0);

    assert(0 && "finished");

    return 0;
}
