#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H

#include "task_api.h"

typedef enum {
    PROCESS_RESULT_EXITED,
    PROCESS_RESULT_SIGNALED
} process_result_kind_t;

typedef struct {
    process_result_kind_t kind;
    int exit_code;
    int terminating_signal;
} process_result_t;

/* Returns 0 when the child was reaped; -1 on a system error (errno is set). */
int process_run_child(const char *label, task_callback_fn task,
                      process_result_t *result);

#endif
