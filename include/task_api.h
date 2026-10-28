#ifndef TASK_API_H
#define TASK_API_H

/* Task callbacks return 0 on success or an error code in the range 1..125. */
typedef int (*task_callback_fn)(void);

#endif
