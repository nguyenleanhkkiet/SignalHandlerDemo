#ifndef SIGNAL_MASKING_H
#define SIGNAL_MASKING_H

/*
 * Demo chan/mo chan SIGINT va SIGUSR1.
 *
 * Tra ve:
 *   0  khi thanh cong.
 *  -1  khi co loi.
 *
 * Su dung trong tien trinh don luong.
 */
int demo_signal_masking(void);

/*
 * Callback tich hop vao menu chung cua nhom.
 *
 * Tra ve:
 *   0 khi thanh cong.
 *   1 khi co loi.
 */
int task4_run(void);

#endif
