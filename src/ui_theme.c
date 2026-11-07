#include "ui_theme.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int ui_is_color_enabled(void)
{
    /* 1. Only enable colors if stdout is an interactive terminal */
    if (!isatty(STDOUT_FILENO)) {
        return 0;
    }

    /* 2. Check standard NO_COLOR environment variable */
    const char *no_color = getenv("NO_COLOR");
    if (no_color != NULL && no_color[0] != '\0') {
        return 0;
    }

    /* 3. Check for dumb terminals */
    const char *term = getenv("TERM");
    if (term != NULL && strcmp(term, "dumb") == 0) {
        return 0;
    }

    return 1;
}

const char *ui_color_reset(void)
{
    return ui_is_color_enabled() ? "\033[0m" : "";
}

const char *ui_color_bold(void)
{
    return ui_is_color_enabled() ? "\033[1m" : "";
}

const char *ui_color_dim(void)
{
    return ui_is_color_enabled() ? "\033[2m" : "";
}

const char *ui_color_red(void)
{
    return ui_is_color_enabled() ? "\033[1;31m" : "";
}

const char *ui_color_green(void)
{
    return ui_is_color_enabled() ? "\033[1;32m" : "";
}

const char *ui_color_yellow(void)
{
    return ui_is_color_enabled() ? "\033[1;33m" : "";
}

const char *ui_color_blue(void)
{
    return ui_is_color_enabled() ? "\033[1;34m" : "";
}

const char *ui_color_magenta(void)
{
    return ui_is_color_enabled() ? "\033[1;35m" : "";
}

const char *ui_color_cyan(void)
{
    return ui_is_color_enabled() ? "\033[1;36m" : "";
}

const char *ui_color_white(void)
{
    return ui_is_color_enabled() ? "\033[1;37m" : "";
}

void ui_print_header(pid_t menu_pid)
{
    const char *c_cyan = ui_color_cyan();
    const char *c_yellow = ui_color_yellow();
    const char *c_green = ui_color_green();
    const char *c_blue = ui_color_blue();
    const char *c_bold = ui_color_bold();
    const char *c_reset = ui_color_reset();

    puts("");
    puts("==========================================================================");
    printf("                  %s%sSIGNAL HANDLER DEMO (C POSIX)%s                     \n",
           c_bold, c_cyan, c_reset);
    printf("    %sImplement and Demo for Signals Handler: signal & sigaction in C%s   \n",
           c_yellow, c_reset);
    puts("--------------------------------------------------------------------------");
    printf("  Platform: %sLinux / POSIX C%s                  Status: %s4/4 TASKS IMPLEMENTED%s\n",
           c_cyan, c_reset, c_green, c_reset);
    printf("  Menu Process PID: %s%-10ld%s               Author: %sSystems Programming%s  \n",
           c_blue, (long)menu_pid, c_reset, c_bold, c_reset);
    puts("==========================================================================");
    fflush(stdout);
}

void ui_print_task_dashboard(void)
{
    const char *c_bold = ui_color_bold();
    const char *c_green = ui_color_green();
    const char *c_dim = ui_color_dim();
    const char *c_reset = ui_color_reset();

    /* Card 1 */
    puts("+------------------------------------------------------------------------+");
    printf("| %s[1] TASK 1: ARCHITECTURE & PROCESS MANAGEMENT%s              %s[COMPLETED]%s |\n",
           c_bold, c_reset, c_green, c_reset);
    printf("| %s    * fork() & waitpid() parent-child process lifecycle management     %s|\n",
           c_dim, c_reset);
    printf("| %s    * Process group terminal safety & concurrent logger (app.log)      %s|\n",
           c_dim, c_reset);
    puts("+------------------------------------------------------------------------+");

    /* Card 2 */
    puts("+------------------------------------------------------------------------+");
    printf("| %s[2] TASK 2: BASIC SIGNAL HANDLING%s                          %s[COMPLETED]%s |\n",
           c_bold, c_reset, c_green, c_reset);
    printf("| %s    * signal() custom handlers: SIGINT (Ctrl+C), SIGTSTP (Ctrl+Z)      %s|\n",
           c_dim, c_reset);
    printf("| %s    * IPC kill() with SIGUSR1, SIGUSR2 and bidirectional pipe sync     %s|\n",
           c_dim, c_reset);
    printf("| %s    * Signal ignoring (SIG_IGN) and default restoration (SIG_DFL)      %s|\n",
           c_dim, c_reset);
    puts("+------------------------------------------------------------------------+");

    /* Card 3 */
    puts("+------------------------------------------------------------------------+");
    printf("| %s[3] TASK 3: ADVANCED SIGNAL HANDLING%s                       %s[COMPLETED]%s |\n",
           c_bold, c_reset, c_green, c_reset);
    printf("| %s    * sigaction() struct with sa_mask and sa_flags                     %s|\n",
           c_dim, c_reset);
    printf("| %s    * SA_SIGINFO 3-argument handler extracting siginfo_t context       %s|\n",
           c_dim, c_reset);
    printf("| %s    * Syscall restart comparison: EINTR interruption vs SA_RESTART     %s|\n",
           c_dim, c_reset);
    puts("+------------------------------------------------------------------------+");

    /* Card 4 */
    puts("+------------------------------------------------------------------------+");
    printf("| %s[4] TASK 4: BLOCKED & UNBLOCKED SIGNALS%s                    %s[COMPLETED]%s |\n",
           c_bold, c_reset, c_green, c_reset);
    printf("| %s    * sigset_t signal sets: sigemptyset(), sigaddset(), sigismember()  %s|\n",
           c_dim, c_reset);
    printf("| %s    * sigprocmask(SIG_BLOCK/UNBLOCK) and pending inspection            %s|\n",
           c_dim, c_reset);
    printf("| %s    * Signal coalescing demonstration & safe state/disposition restore %s|\n",
           c_dim, c_reset);
    puts("+------------------------------------------------------------------------+");

    /* Card 0 (Exit) */
    puts("+------------------------------------------------------------------------+");
    printf("| %s[0] EXIT / THOAT CHUONG TRINH%s                                          |\n",
           c_bold, c_reset);
    printf("| %s    * Clean process termination & resource cleanup                     %s|\n",
           c_dim, c_reset);
    puts("+------------------------------------------------------------------------+");
    fflush(stdout);
}

void ui_print_footer_prompt(void)
{
    const char *c_yellow = ui_color_yellow();
    const char *c_reset = ui_color_reset();
    puts("==========================================================================");
    printf("  %sNhap lua chon [1-4] de chay nhiem vu, hoac [0] de thoat:%s ", c_yellow, c_reset);
    fflush(stdout);
}
