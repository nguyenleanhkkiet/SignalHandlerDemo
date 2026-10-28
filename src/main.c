#include "logger.h"
#include "menu.h"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    if (logger_init("app.log") != 0) {
        perror("Khong mo duoc app.log");
        return EXIT_FAILURE;
    }
    logger_info("Ung dung khoi dong");
    int result = menu_run();
    logger_info("Ung dung ket thuc");
    if (logger_close() != 0) {
        perror("Khong dong duoc app.log");
        result = EXIT_FAILURE;
    }
    return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
