#ifndef TASKS_H
#define TASKS_H

#include <stddef.h>
#include "task_api.h"

typedef struct {
    int number;
    const char *title;
    task_callback_fn entry; /* NULL when the task module is unavailable. */
} project_task_t;

const project_task_t *tasks_get(size_t *count);
int task1_process_demo(void);

#endif
