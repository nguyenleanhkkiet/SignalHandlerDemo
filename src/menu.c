#include "menu.h"
#include "logger.h"
#include "process_manager.h"
#include "tasks.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
        puts("\n=== SIGNAL HANDLER: signal va sigaction ===");
        for (size_t i = 0; i < count; ++i) {
            printf("%d. %s%s\n", tasks[i].number, tasks[i].title,
                   tasks[i].entry != NULL ? "" : " [chua trien khai]");
        }
        puts("0. Thoat");
        printf("Lua chon: ");
        fflush(stdout);
        int input_status = read_choice(&choice);
        if (input_status == 0) return 0;
        if (input_status < 0) {
            puts("Vui long nhap mot so.");
            continue;
        }
        if (choice == 0) return 0;
        const project_task_t *selected = NULL;
        for (size_t i = 0; i < count; ++i) {
            if (tasks[i].number == choice) selected = &tasks[i];
        }
        if (selected == NULL) {
            puts("Lua chon khong hop le.");
            continue;
        }
        if (selected->entry == NULL) {
            puts("Muc nay la giao dien tich hop; noi dung Task 2-4 chua duoc cai dat.");
            (void)logger_info("Nguoi dung chon muc Task %d chua trien khai", selected->number);
            continue;
        }
        (void)logger_info("Nguoi dung chon Task %d", selected->number);
        process_result_t result;
        if (process_run_child(selected->title, selected->entry, &result) != 0) {
            puts("Tac vu ket thuc voi loi.");
        } else if (result.kind == PROCESS_RESULT_EXITED && result.exit_code != 0) {
            printf("Tac vu that bai, ma loi %d. Menu tiep tuc.\n", result.exit_code);
        } else if (result.kind == PROCESS_RESULT_SIGNALED) {
            printf("Tac vu bi signal %d ket thuc. Menu tiep tuc.\n", result.terminating_signal);
        }
    }
}
