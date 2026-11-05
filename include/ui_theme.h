#ifndef UI_THEME_H
#define UI_THEME_H

#include <sys/types.h>

/*
 * Terminal environment detection:
 * Returns 1 if stdout is a TTY and NO_COLOR is not set; 0 otherwise.
 */
int ui_is_color_enabled(void);

/* ANSI Color sequences (return empty string "" if color is disabled) */
const char *ui_color_reset(void);
const char *ui_color_bold(void);
const char *ui_color_dim(void);
const char *ui_color_red(void);
const char *ui_color_green(void);
const char *ui_color_yellow(void);
const char *ui_color_blue(void);
const char *ui_color_magenta(void);
const char *ui_color_cyan(void);
const char *ui_color_white(void);

/*
 * Modern Terminal UI Dashboard Components (Max width: 74 columns <= 78 columns)
 */
void ui_print_header(pid_t menu_pid);
void ui_print_task_dashboard(void);
void ui_print_footer_prompt(void);

#endif /* UI_THEME_H */
