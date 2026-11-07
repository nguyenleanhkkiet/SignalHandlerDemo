#include "process_manager.h"
#include "logger.h"
#include "ui_theme.h"

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

    /* Unidirectional synchronization pipe to ensure parent prints creation banner first */
    int start_pipe[2];
    if (pipe(start_pipe) < 0) {
        int saved_errno = errno;
        (void)logger_error("pipe cho '%s' that bai: %s", label, strerror(saved_errno));
        errno = saved_errno;
        perror("pipe");
        return -1;
    }

    /* Flush stdio buffers before fork() to prevent duplicated buffer outputs */
    fflush(stdout);
    fflush(stderr);

    pid_t child = fork();
    if (child < 0) {
        int saved_errno = errno;
        close(start_pipe[0]);
        close(start_pipe[1]);
        (void)logger_error("fork cho '%s' that bai: %s", label, strerror(saved_errno));
        errno = saved_errno;
        perror("fork");
        return -1;
    }
    if (child == 0) {
        close(start_pipe[1]);
        char token = 0;
        /* Wait until parent has printed and flushed its process creation banner */
        ssize_t n_token;
        do {
            n_token = read(start_pipe[0], &token, 1);
        } while (n_token < 0 && errno == EINTR);
        close(start_pipe[0]);

        (void)logger_info("Child bat dau tac vu '%s'", label);
        int task_result = task();
        if (task_result < 0 || task_result > 125) task_result = 125;
        (void)logger_info("Child ket thuc tac vu '%s' voi ma %d", label, task_result);
        fflush(NULL);
        _exit(task_result);
    }

    /* Parent process */
    close(start_pipe[0]);
    (void)logger_info("Parent tao child pid=%ld cho tac vu '%s'", (long)child, label);
    printf("%s[PROCESS]%s Parent (PID: %ld) -> Da tao Child (PID: %ld); dang cho ket qua...\n",
           ui_color_blue(), ui_color_reset(), (long)getpid(), (long)child);
    fflush(stdout);

    /* Release child to begin execution */
    ssize_t n_written;
    do {
        n_written = write(start_pipe[1], "G", 1);
    } while (n_written < 0 && errno == EINTR);
    close(start_pipe[1]);

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
        return -1;
    }
    if (WIFEXITED(status)) {
        int code = WEXITSTATUS(status);
        result->kind = PROCESS_RESULT_EXITED;
        result->exit_code = code;
        result->terminating_signal = 0;
        if (code == 0) {
            printf("%s[SUCCESS]%s Parent (PID: %ld) <- Child (PID: %ld) hoan tat, exit=%d.\n",
                   ui_color_green(), ui_color_reset(), (long)getpid(), (long)child, code);
        } else {
            printf("%s[ERROR]%s Parent (PID: %ld) <- Child (PID: %ld) that bai, exit=%d.\n",
                   ui_color_red(), ui_color_reset(), (long)getpid(), (long)child, code);
        }
        fflush(stdout);
        (void)logger_info("Parent thu duoc exit=%d tu child pid=%ld", code, (long)child);
        return 0;
    }
    if (WIFSIGNALED(status)) {
        result->kind = PROCESS_RESULT_SIGNALED;
        result->exit_code = 0;
        result->terminating_signal = WTERMSIG(status);
        printf("%s[SIGNAL]%s Parent (PID: %ld) <- Child (PID: %ld) bi ket thuc boi signal %d.\n",
               ui_color_cyan(), ui_color_reset(), (long)getpid(), (long)child, WTERMSIG(status));
        fflush(stdout);
        (void)logger_error("Child pid=%ld bi ket thuc boi signal %d", (long)child, WTERMSIG(status));
        return 0;
    }
    errno = ECHILD;
    return -1;
}
