#include "globe.h"

#define TIMER_BANK_CAPACITY 8

static int active_timer_mask;

/* Note: the timer_id is always greater than zero so a timer_id of zero can be
 * used to identify a inactive timer. This means that a default-initialized
 * timer will be inactive. */

static int allocate_app_timer() {
    CRITICAL_SCOPE();
    for (unsigned i = 0; i < TIMER_BANK_CAPACITY; i++) {
        if ((active_timer_mask & (1 << i)) == 0) {
            active_timer_mask |= 1 << i;
            return i + 1;
        }
    }
    assert(0 && "No app timers available");
    return 0;
}

static void free_app_timer(int timer_id) {
    CRITICAL_SCOPE();
    assert(timer_id > 0);
    int i = timer_id - 1;
    assert(active_timer_mask & (1 << i));
    active_timer_mask &= ~(1 << i);
}

static app_timer_id_t get_app_timer(int timer_id) {

    APP_TIMER_DEF(bank_timer_0);
    APP_TIMER_DEF(bank_timer_1);
    APP_TIMER_DEF(bank_timer_2);
    APP_TIMER_DEF(bank_timer_3);
    APP_TIMER_DEF(bank_timer_4);
    APP_TIMER_DEF(bank_timer_5);
    APP_TIMER_DEF(bank_timer_6);
    APP_TIMER_DEF(bank_timer_7);

    static const app_timer_id_t timer_bank[] = {
        bank_timer_0, bank_timer_1, bank_timer_2, bank_timer_3,
        bank_timer_4, bank_timer_5, bank_timer_6, bank_timer_7,
    };

    assert(timer_id > 0);

    return timer_bank[timer_id - 1];
}

static void start_timer(struct timer *timer, epoch_t ms) {
    ms = max(0, ms); /* negative duration resolves immediately */
    uint32_t ticks = APP_TIMER_TICKS(ms);
    ticks = max(5U, ticks); /* Ticks less than 5 are not accepted */
    assert(ticks <= APP_TIMER_MAX_CNT_VAL);
    app_timer_id_t app_timer = get_app_timer(timer->timer_id);
    ret_code_t rc = app_timer_start(app_timer, ticks, timer);
    assert(rc == NRF_SUCCESS);
}

static void timer_task(void *context) {
    struct timer *timer = context;
    timer->callback(timer);
}

static void timer_handler(void *context) {
    struct timer *timer = context;
    if (timer->callback)
        schedule_task(timer_task, timer);

    bool is_periodic = timer->period_ms >= 0;
    if (is_periodic) {
        /* Care is taken in setting up the next alarm to avoid drift in alarms
         * due to accumulated rounding errors when converting between ticks and
         * ms. */
        timer->starting_ms += timer->period_ms;
        epoch_t next_alarm_ms = timer->starting_ms + timer->period_ms;
        epoch_t ms_to_next_alarm = next_alarm_ms - elapsed_ms();
        start_timer(timer, ms_to_next_alarm);
    } else {
        free_app_timer(timer->timer_id);
        timer->timer_id = 0;
    }
}

void start_periodic_timer(struct timer *timer, epoch_t period_ms,
                          void (*callback)(struct timer *timer)) {
    *timer = (struct timer){
        .callback = callback,
        .starting_ms = elapsed_ms(),
        .period_ms = period_ms,
        .timer_id = allocate_app_timer(),
    };
    start_timer(timer, period_ms);
}

void start_one_shot_timer(struct timer *timer, epoch_t duration_ms,
                          void (*callback)(struct timer *timer)) {
    *timer = (struct timer){
        .callback = callback,
        .starting_ms = elapsed_ms(),
        .period_ms = -1,
        .timer_id = allocate_app_timer(),
    };
    start_timer(timer, duration_ms);
}

void stop_timer(struct timer *timer) {
    CRITICAL_SCOPE();
    if (!timer_active(timer)) {
        return;
    }
    ret_code_t rc = app_timer_stop(get_app_timer(timer->timer_id));
    assert(rc == NRF_SUCCESS);
    free_app_timer(timer->timer_id);
    timer->timer_id = 0;
}

bool timer_active(const struct timer *timer) {
    CRITICAL_SCOPE();
    return timer->timer_id > 0;
}

#define TIMER_TICKS_PER_SEC (APP_TIMER_TICKS(1000))
#define APP_TIMER_CAPACITY_MS                                                          \
    ((1000 * ((epoch_t)(APP_TIMER_MAX_CNT_VAL) + 1)) / TIMER_TICKS_PER_SEC)
#define ACCUMULATED_ELAPSED_MS_UPDATE_PERIOD_MS (APP_TIMER_CAPACITY_MS - 30 * 1000)

static epoch_t accumulated_ticks;
static epoch_t last_serviced_tick;
APP_TIMER_DEF(elapsed_ticks_timer);
static void accumulated_elapsed_ms_update(void *context) {
    (void)context;
    epoch_t current_tick = app_timer_cnt_get();
    accumulated_ticks += app_timer_cnt_diff_compute(current_tick, last_serviced_tick);
    last_serviced_tick = current_tick;
}

epoch_t elapsed_ms() {
    epoch_t elapsed_ticks =
        app_timer_cnt_diff_compute(app_timer_cnt_get(), last_serviced_tick) +
        accumulated_ticks;
    epoch_t ms_per_second = 1000;
    return (ms_per_second * elapsed_ticks) / TIMER_TICKS_PER_SEC;
}

static int64_t dt;
static bool epoch_set_ = false;

epoch_t get_epoch_ms() {
    assert(epoch_set_);
    return elapsed_ms() + dt;
}

void set_epoch_ms(epoch_t epoch_ms) {
    dt = epoch_ms - elapsed_ms();
    epoch_set_ = true;
}

bool is_epoch_set() { return epoch_set_; }

void timer_module_init() {

    ret_code_t err_code = app_timer_init();
    assert(err_code == 0);

    for (int i = 0; i < TIMER_BANK_CAPACITY; i++) {
        int timer_id = i + 1;
        app_timer_id_t app_timer = get_app_timer(timer_id);
        ret_code_t rc =
            app_timer_create(&app_timer, APP_TIMER_MODE_SINGLE_SHOT, timer_handler);
        assert(rc == NRF_SUCCESS);
    }

    last_serviced_tick = app_timer_cnt_get();
    accumulated_ticks = 0;

    ret_code_t rc = app_timer_create(&elapsed_ticks_timer, APP_TIMER_MODE_REPEATED,
                                     accumulated_elapsed_ms_update);
    assert(rc == NRF_SUCCESS);
    rc = app_timer_start(elapsed_ticks_timer,
                         ((uint32_t)(APP_TIMER_MAX_CNT_VAL) + 1) / 2, NULL);
    assert(rc == NRF_SUCCESS);
}
