#ifndef TASK4_BLOCKED_H
#define TASK4_BLOCKED_H

/*
 * Task 4: Blocked and Unblocked Signals Demo
 * Demonstrates: sigset_t, sigemptyset, sigaddset, sigprocmask,
 *               sigpending, sigismember, signal coalescing,
 *               and safe mask/disposition restoration.
 *
 * Returns 0 on success, or non-zero error code.
 */
int task4_blocked_demo(void);

#endif /* TASK4_BLOCKED_H */
