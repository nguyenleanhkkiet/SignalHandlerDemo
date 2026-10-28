#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "signal_masking.h"

/*
 * Handler chi dat co kieu volatile sig_atomic_t.
 * Khong goi printf() hay logger trong handler.
 */
static volatile sig_atomic_t sigint_received = 0;
static volatile sig_atomic_t sigusr1_received = 0;

static void handle_signal(int signo)
{
    if (signo == SIGINT) {
        sigint_received = 1;
    } else if (signo == SIGUSR1) {
        sigusr1_received = 1;
    }
}

/* Mo phong cong viec trong 2 giay, tiep tuc neu bi ngat. */
static int simulate_work(void)
{
    struct timespec remaining = {6, 0};

    while (nanosleep(&remaining, &remaining) == -1) {
        if (errno != EINTR) {
            perror("nanosleep");
            return -1;
        }
    }

    return 0;
}

/* Kiem tra va in cac tin hieu pending thuoc pham vi demo. */
static int print_pending_signals(void)
{
    sigset_t pending_set;

    /*
     * sigpending() lay tap tin hieu dang cho.
     * Thao tac nay khong xoa tin hieu khoi tap pending.
     */
    if (sigpending(&pending_set) == -1) {
        perror("sigpending");
        return -1;
    }

    int pending_int = sigismember(&pending_set, SIGINT);
    int pending_usr1 = sigismember(&pending_set, SIGUSR1);

    if (pending_int == -1 || pending_usr1 == -1) {
        perror("sigismember");
        return -1;
    }

    printf("[PENDING] SIGINT : %s\n",
           pending_int ? "DANG CHO" : "Khong co");

    printf("[PENDING] SIGUSR1: %s\n",
           pending_usr1 ? "DANG CHO" : "Khong co");

    return 0;
}

int demo_signal_masking(void)
{
    sigset_t block_set;
    sigset_t old_mask;

    struct sigaction action = {0};
    struct sigaction old_int_action;
    struct sigaction old_usr1_action;

    int result = 0;

    sigint_received = 0;
    sigusr1_received = 0;

    /* Tao tap tin hieu can chan: SIGINT va SIGUSR1. */
    if (sigemptyset(&block_set) == -1 ||
        sigaddset(&block_set, SIGINT) == -1 ||
        sigaddset(&block_set, SIGUSR1) == -1) {
        perror("Tao tap tin hieu");
        return -1;
    }

    /* Cau hinh handler chung cho hai tin hieu. */
    action.sa_handler = handle_signal;
    action.sa_flags = 0;

    if (sigemptyset(&action.sa_mask) == -1) {
        perror("sigemptyset");
        return -1;
    }

    /*
     * Cai handler SIGINT, dong thoi luu handler cu
     * de khoi phuc khi ket thuc module.
     */
    if (sigaction(SIGINT, &action, &old_int_action) == -1) {
        perror("sigaction SIGINT");
        return -1;
    }

    /* Cai handler SIGUSR1 va luu handler cu. */
    if (sigaction(SIGUSR1, &action, &old_usr1_action) == -1) {
        perror("sigaction SIGUSR1");

        if (sigaction(SIGINT, &old_int_action, NULL) == -1) {
            perror("Khoi phuc handler SIGINT");
        }

        return -1;
    }

    /*
     * Them hai tin hieu vao mask dang chan.
     * Luu mask ban dau vao old_mask.
     */
    if (sigprocmask(SIG_BLOCK, &block_set, &old_mask) == -1) {
        perror("sigprocmask SIG_BLOCK");
        result = -1;
        goto restore_handlers;
    }

    printf("\n===== TASK 4: SIGNAL MASKING =====\n");
    printf("PID cua tien trinh: %ld\n", (long)getpid());
    printf("Da chan SIGINT va SIGUSR1.\n");
    printf("Khi chay qua menu chung, khong nhan Ctrl+C.\n");
    printf("Mo Terminal khac, gui tin hieu den PID demo ben duoi:\n");
    printf("  kill -INT %ld\n", (long)getpid());
    printf("  kill -USR1 %ld\n\n", (long)getpid());
    fflush(stdout);

    /*
     * Mo phong vung xu ly du lieu quan trong.
     * Hai tin hieu dang bi chan khong the goi handler
     * trong giai doan nay.
     */
    for (int i = 1; i <= 5; i++) {
        printf("[WORK] Dang xu ly phan %d/5...\n", i);
        fflush(stdout);

        if (simulate_work() == -1) {
            result = -1;
            break;
        }

        if (print_pending_signals() == -1) {
            result = -1;
            break;
        }

        printf("\n");
        fflush(stdout);
    }

    if (result == 0) {
        printf("[WORK] Da hoan tat cong viec quan trong.\n");
    } else {
        printf("[WORK] Co loi, dung demo va khoi phuc trang thai.\n");
    }

    printf("[DEMO] Chuan bi khoi phuc mask ban dau.\n");
    fflush(stdout);

    /*
     * Khoi phuc dung mask truoc khi vao module.
     * Tin hieu nao ban dau khong bi chan se duoc mo chan.
     * Handler co the chay truoc khi loi goi nay tra ve.
     */
    if (sigprocmask(SIG_SETMASK, &old_mask, NULL) == -1) {
        perror("sigprocmask SIG_SETMASK");
        result = -1;
    } else {
        printf("[DEMO] Da khoi phuc mask ban dau.\n");

        if (sigint_received) {
            printf("[RESULT] Handler da xu ly SIGINT.\n");
        } else {
            printf("[RESULT] Handler chua nhan SIGINT.\n");
        }

        if (sigusr1_received) {
            printf("[RESULT] Handler da xu ly SIGUSR1.\n");
        } else {
            printf("[RESULT] Handler chua nhan SIGUSR1.\n");
        }
    }

restore_handlers:
    /* Tra lai handler cu cho chuong trinh goi module. */
    if (sigaction(SIGINT, &old_int_action, NULL) == -1) {
        perror("Khoi phuc handler SIGINT");
        result = -1;
    }

    if (sigaction(SIGUSR1, &old_usr1_action, NULL) == -1) {
        perror("Khoi phuc handler SIGUSR1");
        result = -1;
    }

    printf("===== KET THUC DEMO =====\n\n");

    return result;
}
/*
 * Chuyen ket qua cua demo sang quy uoc callback cua nhom.
 * Process manager se goi ham nay trong tien trinh con.
 */
int task4_run(void)
{
    return demo_signal_masking() == 0 ? 0 : 1;
}
