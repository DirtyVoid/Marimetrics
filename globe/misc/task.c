#include "globe.h"

#define TASK_QUEUE_CAPACITY 16

struct task_queue {
    int begin_idx;
    int size;
    struct task {
        void (*callback)(void *context);
        void *context;
    } tasks[TASK_QUEUE_CAPACITY];
} task_queue;

static bool task_queue_empty(const struct task_queue *q) { return q->size == 0; }

static bool task_queue_full(const struct task_queue *q) {
    return q->size == TASK_QUEUE_CAPACITY;
}

static int task_queue_idx_inc(int i0, int i1) {
    return (i0 + i1) % TASK_QUEUE_CAPACITY;
}

static struct task task_queue_pop(struct task_queue *q) {
    assert(!task_queue_empty(q));
    struct task t = q->tasks[q->begin_idx];
    q->begin_idx = task_queue_idx_inc(q->begin_idx, 1);
    q->size--;
    return t;
}

static void task_queue_push(struct task_queue *q, struct task t) {
    assert(!task_queue_full(q));
    q->tasks[task_queue_idx_inc(q->begin_idx, q->size)] = t;
    q->size++;
}

void schedule_task(void (*callback)(void *context), void *context) {
    struct task t = {
        .callback = callback,
        .context = context,
    };
    CRITICAL_SCOPE();
    task_queue_push(&task_queue, t);
}

void cancel_pending_tasks(void *context) {
    assert(context != NULL);
    CRITICAL_SCOPE();
    for (int i = 0; i < task_queue.size; i++) {
        int idx = task_queue_idx_inc(task_queue.begin_idx, i);
        struct task *t = &task_queue.tasks[idx];
        if (t->context == context) {
            t->callback = NULL;
            t->context = NULL;
        }
    }
}

static bool try_handle_pending_task() {
    struct task t;
    {
        CRITICAL_SCOPE();
        if (task_queue_empty(&task_queue))
            return false;
        t = task_queue_pop(&task_queue);
    }
    if (t.callback) /* May be NULL if the callback is cancelled. */
        t.callback(t.context);
    return true;
}

void handle_pending_tasks() {
    while (try_handle_pending_task())
        ;
}

bool tasks_pending() {
    CRITICAL_SCOPE();
    return !task_queue_empty(&task_queue);
}
