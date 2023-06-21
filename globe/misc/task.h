#ifndef TASK_H
#define TASK_H

#include "globe.h"

void schedule_task(void (*callback)(void *context), void *context);

/* Discard all pending tasks with a given context. This should be done if the
 * memory pointed to by the context goes out of scope. Context must not be
 * NULL. */
void cancel_pending_tasks(void *context);

void handle_pending_tasks();

bool tasks_pending();

#endif
