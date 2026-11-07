#include "menu.h"
#include "logger.h"
#include "process_manager.h"
#include "tasks.h"
#include "ui_theme.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int read_choice(long *choice)
{
    char input[64];
    if (fgets(input, sizeof(input), stdin) == NULL) return 0;

    size_t length = strlen(input);
    if (length > 0 && input[length - 1] != '\n' && !feof(stdin)) {
        int ch = getchar();
        if (ch != EOF) {
            while (ch != '\n' && ch != EOF) ch = getchar();
            return -1;
        }
    }

    char *start = input;
    while (isspace((unsigned char)*start)) ++start;
    char *end = start + strlen(start);
    while (end > start && isspace((unsigned char)end[-1])) --end;
    *end = '\0';
    if (*start == '\0') return -1;
    for (char *p = start; *p != '\0'; ++p)
        if (!isdigit((unsigned char)*p)) return -1;

    errno = 0;
    char *number_end = NULL;
    long parsed = strtol(start, &number_end, 10);
    if (errno == ERANGE || number_end == start || *number_end != '\0')
        return -1;
    *choice = parsed;
    return 1;
}

int menu_run(void)
{
    size_t count;
    const project_task_t *tasks = tasks_get(&count);
    long choice;

    for (;;) {
        ui_print_header(getpid());
        ui_print_task_dashboard();
        ui_print_footer_prompt();

        int input_status = read_choice(&choice);
        if (input_status == 0) return 0;
        if (!isatty(STDIN_FILENO)) {
            putchar('\n');
            fflush(stdout);
        }
        if (input_status < 0) {
            printf("\n%s[WARNING]%s Vui long nhap mot so hop le (tu 0 den 4).\n",
                   ui_color_yellow(), ui_color_reset());
            continue;
        }
        if (choice == 0) return 0;
        const project_task_t *selected = NULL;
        for (size_t i = 0; i < count; ++i) {
            if (tasks[i].number == choice) selected = &tasks[i];
        }
        if (selected == NULL) {
            printf("\n%s[WARNING]%s Lua chon khong hop le. Vui long chon tu 0 den 4.\n",
                   ui_color_yellow(), ui_color_reset());
            continue;
        }
        if (selected->entry == NULL) {
            printf("\n%s[WARNING]%s Nhiem vu chua duoc cai dat.\n",
                   ui_color_yellow(), ui_color_reset());
            (void)logger_info("Nguoi dung chon muc Task %d chua trien khai", selected->number);
            continue;
        }
        (void)logger_info("Nguoi dung chon Task %d", selected->number);
        process_result_t result;
        if (process_run_child(selected->title, selected->entry, &result) != 0) {
            printf("\n%s[ERROR]%s Tac vu ket thuc voi loi khoi tao.\n",
                   ui_color_red(), ui_color_reset());
        } else if (result.kind == PROCESS_RESULT_EXITED && result.exit_code != 0) {
            printf("\n%s[ERROR]%s Tac vu that bai voi ma thoat %d. Menu tiep tuc.\n",
                   ui_color_red(), ui_color_reset(), result.exit_code);
        } else if (result.kind == PROCESS_RESULT_SIGNALED) {
            printf("\n%s[ERROR]%s Tac vu bi signal %d ket thuc. Menu tiep tuc.\n",
                   ui_color_red(), ui_color_reset(), result.terminating_signal);
        }
    }
}
