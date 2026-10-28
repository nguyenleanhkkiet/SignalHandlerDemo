#include "process_manager.h"
#include "logger.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int process_run_child(const char *label, task_callback_fn task,
                      process_result_t *result)
{
    if (label == NULL || task == NULL || result == NULL) {
        errno = EINVAL;
        return -1;
    }

    pid_t child = fork();
    if (child < 0) {
        int saved_errno = errno;
        (void)logger_error("fork cho '%s' that bai: %s", label, strerror(saved_errno));
        errno = saved_errno;
        perror("fork");
        errno = saved_errno;
        return -1;
    }
    if (child == 0) {
        (void)logger_info("Child bat dau tac vu '%s'", label);
        int task_result = task();
        if (task_result < 0 || task_result > 125) task_result = 125;
        (void)logger_info("Child ket thuc tac vu '%s' voi ma %d", label, task_result);
        fflush(NULL);
        _exit(task_result);
    }

    (void)logger_info("Parent tao child pid=%ld cho tac vu '%s'", (long)child, label);
    printf("[Parent %ld] Da tao child %ld; dang cho ket qua...\n", (long)getpid(), (long)child);
    int status;
    pid_t waited;
    do {
        waited = waitpid(child, &status, 0);
    } while (waited < 0 && errno == EINTR);
    if (waited < 0) {
        int saved_errno = errno;
        (void)logger_error("waitpid cho pid=%ld that bai: %s", (long)child,
                           strerror(saved_errno));
        errno = saved_errno;
        perror("waitpid");
        errno = saved_errno;
        return -1;
    }
    if (WIFEXITED(status)) {
        int code = WEXITSTATUS(status);
        result->kind = PROCESS_RESULT_EXITED;
        result->exit_code = code;
        result->terminating_signal = 0;
        printf("[Parent %ld] Child %ld ket thuc, exit=%d.\n", (long)getpid(), (long)child, code);
        (void)logger_info("Parent thu duoc exit=%d tu child pid=%ld", code, (long)child);
        return 0;
    }
    if (WIFSIGNALED(status)) {
        result->kind = PROCESS_RESULT_SIGNALED;
        result->exit_code = 0;
        result->terminating_signal = WTERMSIG(status);
        printf("[Parent %ld] Child %ld bi ket thuc boi signal %d.\n", (long)getpid(), (long)child, WTERMSIG(status));
        (void)logger_error("Child pid=%ld bi ket thuc boi signal %d", (long)child, WTERMSIG(status));
        return 0;
    }
    errno = ECHILD;
    return -1;
}
