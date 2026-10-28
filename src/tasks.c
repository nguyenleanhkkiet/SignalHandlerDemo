#include "tasks.h"
#include "logger.h"

#include <stdio.h>
#include <unistd.h>

int task1_process_demo(void)
{
    printf("[Child %ld] Task 1: minh hoa tien trinh con duoc tao bang fork().\n", (long)getpid());
    printf("[Child %ld] PID cua tien trinh cha: %ld.\n", (long)getpid(), (long)getppid());
    logger_info("Task 1 demo dang chay trong child pid=%ld, parent pid=%ld", (long)getpid(), (long)getppid());
    return 0;
}

const project_task_t *tasks_get(size_t *count)
{
    static const project_task_t tasks[] = {
        {1, "Task 1 - Nen tang fork/parent-child va logger", task1_process_demo},
        {2, "Task 2 - Cho tich hop", NULL},
        {3, "Task 3 - Cho tich hop", NULL},
        {4, "Task 4 - Cho tich hop", NULL}
    };
    *count = sizeof(tasks) / sizeof(tasks[0]);
    return tasks;
}
