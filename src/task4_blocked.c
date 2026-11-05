#include "task4_blocked.h"
#include "logger.h"
#include "ui_theme.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

/*
 * ============================================================================
 * ASYNC-SIGNAL-SAFE FLAGS AND COUNTERS
 * ============================================================================
 */
static volatile sig_atomic_t g_usr1_handled_count = 0;
static volatile sig_atomic_t g_usr2_handled_count = 0;

/*
 * Async-signal-safe string write helper.
 * Checks return values and handles EINTR to satisfy -Werror=unused-result.
 */
static void safe_write_all(int fd, const char *buf, size_t len)
{
    size_t written = 0;
    while (written < len) {
        ssize_t n = write(fd, buf + written, len - written);
        if (n > 0) {
            written += (size_t)n;
        } else if (n < 0 && errno == EINTR) {
            continue;
        } else {
            break;
        }
    }
}

/* Signal handler for SIGUSR1 */
static void handle_sigusr1(int sig)
{
    int saved_errno = errno;
    (void)sig;
    g_usr1_handled_count++;
    const char msg[] = "    [Signal Handler] Da nhan SIGUSR1 (Signal 10)!\n";
    safe_write_all(STDOUT_FILENO, msg, sizeof(msg) - 1);
    errno = saved_errno;
}

/* Signal handler for SIGUSR2 */
static void handle_sigusr2(int sig)
{
    int saved_errno = errno;
    (void)sig;
    g_usr2_handled_count++;
    const char msg[] = "    [Signal Handler] Da nhan SIGUSR2 (Signal 12)!\n";
    safe_write_all(STDOUT_FILENO, msg, sizeof(msg) - 1);
    errno = saved_errno;
}

/*
 * ============================================================================
 * THEORETICAL EXPLANATIONS AND COMPARISONS (PART 3)
 * ============================================================================
 */
static void print_theoretical_explanations(void)
{
    puts("--------------------------------------------------");
    puts("[Phan 3] Giai thich va phan biet cac trang thai tin hieu trong POSIX");
    puts("");
    puts("1. Khai niem co ban:");
    puts("   - Signal Mask (Mat na tin hieu):");
    puts("     + La tap hop cac tin hieu hien dang bi hoan phan phoi (blocked from delivery)");
    puts("       toi tien trinh hoac luong.");
    puts("     + Trong chuan POSIX.1 hien dai (da luong), signal mask la thuoc tinh rieng");
    puts("       cua tung luong (thread-specific mask, quan ly boi pthread_sigmask).");
    puts("     + Trong chuong trinh don luong nay, viec su dung sigprocmask() la hoan toan");
    puts("       chuan muc va tuong thich tieu chuan POSIX.");
    puts("   - Pending Signal (Tin hieu dang cho):");
    puts("     + La tin hieu da duoc sinh ra (generated boi kill, raise, kernel, phan cung...)");
    puts("       nhung chua duoc chuyen giao (delivered) toi tien trinh vi dang nam trong");
    puts("       signal mask cua tien trinh.");
    puts("");
    puts("2. Quan he giua Blocked va Pending:");
    puts("   - Blocked va Pending KHONG PHAI hai trang thai loai tru lan nhau.");
    puts("   - Mot tin hieu co the vua bi Block (nam trong signal mask) vua dang Pending");
    puts("     (nam trong pending set cua tien trinh trong kernel).");
    puts("   - Khi mot tin hieu bi Block duoc gui toi tien trinh:");
    puts("     + Kernel bat bit pending tuong ung.");
    puts("     + Tien trinh KHONG bi ket thuc, KHONG chay handler va KHONG bi treo.");
    puts("   - Khi bo chan (Unblock):");
    puts("     + Kernel kiem tra bit pending, xoa bit va ngay lap tuc deliver tin hieu");
    puts("       vao signal handler da dang ky.");
    puts("");
    puts("3. Co che gop tin hieu (Signal Coalescing):");
    puts("   - Cac tin hieu tieu chuan (Standard Signals nhu SIGUSR1, SIGINT):");
    puts("     + Khi mot standard signal phat sinh nhieu lan trong luc tin hieu cung loai");
    puts("       van dang pending, he thong co the gop chung thanh mot lan chuyen giao duy nhat.");
    puts("     + Trong he dieu hanh Linux (va nhieu he UNIX), kernel quan ly pending signals");
    puts("       bang bitmap noi bo (moi tin hieu tieu chuan chi tuong ung voi mot bit co).");
    puts("       Day la cach trien khai cu the cua Linux, khong phai quy dinh bat buoc ve");
    puts("       cau truc noi bo cua moi he dieu hanh POSIX. Tuy nhien, POSIX quy dinh ro");
    puts("       rang he thong khong bat buoc phai xep hang rieng tung standard signal.");
    puts("   - Phan biet voi Real-Time Signals (SIGRTMIN den SIGRTMAX, POSIX.1b):");
    puts("     + Real-Time signals ho tro co che xep hang (signal queuing) rieng tung lan gui");
    puts("       va co the mang theo du lieu (qua sigqueue()), khac biet hoan toan voi co che");
    puts("       khong xep hang cua standard signals.");
    puts("");
    puts("4. Bang so sanh 4 khai niem / trang thai tin hieu:");
    puts("+------------+-----------------------------------------------------------------+");
    puts("| Khai niem  | Y nghia va hanh vi cua he dieu hanh                             |");
    puts("+------------+-----------------------------------------------------------------+");
    puts("| Blocked    | Tin hieu nam trong signal mask. Kernel tam giu lai va hoan      |");
    puts("|            | chuyen giao (deferred delivery).                                |");
    puts("+------------+-----------------------------------------------------------------+");
    puts("| Pending    | Tin hieu da phat sinh nhung chua duoc giai toa / chuyen giao.   |");
    puts("|            | Dang cho trong kernel pending set de duoc xu ly khi unblock.   |");
    puts("+------------+-----------------------------------------------------------------+");
    puts("| Ignored    | Disposition duoc thiet lap la SIG_IGN. Kernel loai bo ngay      |");
    puts("|            | khi tin hieu phat sinh (khong pending, khong goi handler).      |");
    puts("+------------+-----------------------------------------------------------------+");
    puts("| Handled    | Tin hieu duoc chuyen giao thanh cong va kich hoat thuc thi      |");
    puts("|            | ham signal handler do nguoi dung dang ky bang sigaction/signal. |");
    puts("+------------+-----------------------------------------------------------------+");
    fflush(stdout);
}

/*
 * ============================================================================
 * MAIN ENTRY POINT: TASK 4 - BLOCKED AND UNBLOCKED SIGNALS
 * ============================================================================
 */
int task4_blocked_demo(void)
{
    int ret_code = 0;

    /* Tracking state for safe, centralized cleanup */
    int orig_mask_saved = 0;
    int orig_usr1_saved = 0;
    int orig_usr2_saved = 0;

    sigset_t orig_mask;
    struct sigaction orig_act_usr1;
    struct sigaction orig_act_usr2;

    puts("");
    puts("==========================================================================");
    printf("        %s%sBAT DAU TASK 4: BLOCKED AND UNBLOCKED SIGNALS%s               \n",
           ui_color_bold(), ui_color_cyan(), ui_color_reset());
    puts("==========================================================================");
    fflush(stdout);

    /*
     * ------------------------------------------------------------------------
     * STEP 0: SAVE INITIAL SIGNAL MASK AND DISPOSITIONS
     * ------------------------------------------------------------------------
     */
    if (sigprocmask(SIG_SETMASK, NULL, &orig_mask) < 0) {
        perror("sigprocmask(SIG_SETMASK get orig_mask)");
        return -1;
    }
    orig_mask_saved = 1;

    if (sigaction(SIGUSR1, NULL, &orig_act_usr1) < 0) {
        perror("sigaction get orig SIGUSR1");
        ret_code = -1;
        goto cleanup;
    }
    orig_usr1_saved = 1;

    if (sigaction(SIGUSR2, NULL, &orig_act_usr2) < 0) {
        perror("sigaction get orig SIGUSR2");
        ret_code = -1;
        goto cleanup;
    }
    orig_usr2_saved = 1;

    /*
     * ------------------------------------------------------------------------
     * PART 1: BLOCKED SIGNAL DEMO (SIGUSR1) & SIGNAL COALESCING
     * ------------------------------------------------------------------------
     */
    puts("--------------------------------------------------");
    puts("[Phan 1] Minh hoa Blocked Signal va Pending Signal voi SIGUSR1");
    puts("Cac buoc thuc hien:");
    puts("  1. Dang ky handler cho SIGUSR1 bang sigaction().");
    puts("  2. Khoi tao signal set bang sigemptyset() va sigaddset(SIGUSR1).");
    puts("  3. Block SIGUSR1 bang sigprocmask(SIG_BLOCK, ...).");
    puts("  4. Gui SIGUSR1 toi chinh tien trinh 3 lan bang kill().");
    puts("  5. Dung sigpending() va sigismember() xac nhan SIGUSR1 dang Pending.");
    puts("  6. Kiem tra chung minh handler chua chay trong thoi gian bi block.");
    puts("  7. Unblock SIGUSR1 bang sigprocmask(SIG_UNBLOCK, ...).");
    puts("  8. Xac nhan handler thuc thi va minh hoa hien tuong gop tin hieu.");
    fflush(stdout);

    /* 1. Register handler for SIGUSR1 */
    struct sigaction act1;
    memset(&act1, 0, sizeof(act1));
    act1.sa_handler = handle_sigusr1;
    if (sigemptyset(&act1.sa_mask) < 0) {
        perror("sigemptyset act1.sa_mask");
        ret_code = -1;
        goto cleanup;
    }
    act1.sa_flags = 0;

    if (sigaction(SIGUSR1, &act1, NULL) < 0) {
        perror("sigaction register SIGUSR1");
        ret_code = -1;
        goto cleanup;
    }

    /* 2 & 3. Create set and block SIGUSR1 */
    sigset_t block_usr1_set;
    if (sigemptyset(&block_usr1_set) < 0) {
        perror("sigemptyset block_usr1_set");
        ret_code = -1;
        goto cleanup;
    }
    if (sigaddset(&block_usr1_set, SIGUSR1) < 0) {
        perror("sigaddset SIGUSR1");
        ret_code = -1;
        goto cleanup;
    }

    if (sigprocmask(SIG_BLOCK, &block_usr1_set, NULL) < 0) {
        perror("sigprocmask(SIG_BLOCK SIGUSR1)");
        ret_code = -1;
        goto cleanup;
    }

    /* Verify SIGUSR1 is currently in process signal mask */
    sigset_t current_mask;
    if (sigprocmask(SIG_BLOCK, NULL, &current_mask) < 0) {
        perror("sigprocmask get current_mask");
        ret_code = -1;
        goto cleanup;
    }
    int is_blocked = sigismember(&current_mask, SIGUSR1);
    if (is_blocked != 1) {
        fprintf(stderr, "Loi: SIGUSR1 khong co trong signal mask sau khi goi sigprocmask!\n");
        ret_code = -1;
        goto cleanup;
    }
    printf("  -> Xac nhan: SIGUSR1 da duoc dua vao signal mask (%s[BLOCKED]%s, is_member=%d).\n",
           ui_color_yellow(), ui_color_reset(), is_blocked);

    /* Reset handler count */
    g_usr1_handled_count = 0;

    /* 4. Send SIGUSR1 multiple times while blocked */
    pid_t self_pid = getpid();
    printf("  -> Gui SIGUSR1 lan 1 toi PID %ld qua kill()...\n", (long)self_pid);
    if (kill(self_pid, SIGUSR1) < 0) {
        perror("kill SIGUSR1 #1");
        ret_code = -1;
        goto cleanup;
    }
    printf("  -> Gui SIGUSR1 lan 2 toi PID %ld qua kill() (kiem tra coalescing)...\n", (long)self_pid);
    if (kill(self_pid, SIGUSR1) < 0) {
        perror("kill SIGUSR1 #2");
        ret_code = -1;
        goto cleanup;
    }
    printf("  -> Gui SIGUSR1 lan 3 toi PID %ld qua kill() (kiem tra coalescing)...\n", (long)self_pid);
    if (kill(self_pid, SIGUSR1) < 0) {
        perror("kill SIGUSR1 #3");
        ret_code = -1;
        goto cleanup;
    }
    fflush(stdout);

    /* 5. Check pending set */
    sigset_t pending_set;
    if (sigpending(&pending_set) < 0) {
        perror("sigpending");
        ret_code = -1;
        goto cleanup;
    }
    int is_pending = sigismember(&pending_set, SIGUSR1);
    if (is_pending != 1) {
        fprintf(stderr, "Loi: sigpending() khong phat hien SIGUSR1 trong pending set!\n");
        ret_code = -1;
        goto cleanup;
    }
    printf("  -> Kiem tra sigpending(): SIGUSR1 DANG O TRANG THAI %s[PENDING]%s (is_member=%d)!\n",
           ui_color_magenta(), ui_color_reset(), is_pending);

    /* 6. Verify handler has not executed yet */
    printf("  -> Kiem tra co handler khi bi block: g_usr1_handled_count = %d\n", (int)g_usr1_handled_count);
    if (g_usr1_handled_count != 0) {
        fprintf(stderr, "Loi: Handler da chay khi tin hieu dang bi block!\n");
        ret_code = -1;
        goto cleanup;
    }
    puts("  => XAC NHAN: Blocked va Pending dong thoi ton tai. Handler CHUA he duoc goi!");
    fflush(stdout);

    /* 7. Unblock SIGUSR1 */
    puts("  -> Tien hanh unblock SIGUSR1 bang sigprocmask(SIG_UNBLOCK, ...)...");
    fflush(stdout);
    if (sigprocmask(SIG_UNBLOCK, &block_usr1_set, NULL) < 0) {
        perror("sigprocmask(SIG_UNBLOCK SIGUSR1)");
        ret_code = -1;
        goto cleanup;
    }

    /* 8. Verify handler ran and check coalescing */
    printf("  -> Kiem tra co handler sau khi unblock: %s[HANDLED]%s (g_usr1_handled_count = %d)\n",
           ui_color_green(), ui_color_reset(), (int)g_usr1_handled_count);
    if (g_usr1_handled_count == 0) {
        fprintf(stderr, "Loi: Handler khong duoc thuc thi sau khi unblock!\n");
        ret_code = -1;
        goto cleanup;
    }
    if (g_usr1_handled_count == 1) {
        puts("  => MINH HOA COALESCING THANH CONG: Gui 3 lan nhung handler chi chay dung 1 lan!");
        puts("     Giai thich: Standard signals khi phat sinh nhieu lan trong luc van dang pending");
        puts("     co the bi gop (coalesced) thanh mot lan chuyen giao duy nhat khi unblock.");
        puts("     (Trong Linux, kernel quan ly pending bang bitmap 1 bit; khac voi Real-Time");
        puts("     signals co co che xep hang rieng biet).");
    } else {
        printf("  => Ghi nhan: Handler chay %d lan.\n", (int)g_usr1_handled_count);
    }

    /* Verify pending bit is cleared after delivery */
    if (sigpending(&pending_set) < 0) {
        perror("sigpending after unblock");
        ret_code = -1;
        goto cleanup;
    }
    int is_pending_after = sigismember(&pending_set, SIGUSR1);
    printf("  -> Kiem tra lai sigpending() sau unblock: is_pending = %d (da xoa khoi pending set).\n", is_pending_after);
    puts("  => Ket luan Phan 1: Block, Pending, Unblock va Coalescing hoat dong dung thiet ke.");
    fflush(stdout);

    /*
     * ------------------------------------------------------------------------
     * PART 2: UNBLOCKED SIGNAL DEMO (SIGUSR2)
     * ------------------------------------------------------------------------
     */
    puts("");
    puts("--------------------------------------------------");
    puts("[Phan 2] Minh hoa Unblocked Signal voi SIGUSR2");
    puts("Cac buoc thuc hien:");
    puts("  1. Xac nhan SIGUSR2 dang khong nam trong signal mask.");
    puts("  2. Dang ky handler cho SIGUSR2 bang sigaction().");
    puts("  3. Gui SIGUSR2 bang kill(getpid(), SIGUSR2).");
    puts("  4. Xac nhan handler chay ngay lap tuc ma khong can qua buoc unblock.");
    puts("  5. Kiem tra sigpending() de chung minh tin hieu khong ton dong o pending.");
    fflush(stdout);

    /* 1. Ensure SIGUSR2 is not blocked */
    if (sigprocmask(SIG_BLOCK, NULL, &current_mask) < 0) {
        perror("sigprocmask get mask before SIGUSR2");
        ret_code = -1;
        goto cleanup;
    }
    int usr2_blocked = sigismember(&current_mask, SIGUSR2);
    if (usr2_blocked != 0) {
        fprintf(stderr, "Loi: SIGUSR2 dang bi block truoc khi thu nghiem!\n");
        ret_code = -1;
        goto cleanup;
    }
    printf("  -> Xac nhan: SIGUSR2 dang %s[UNBLOCKED]%s (is_member=%d).\n",
           ui_color_cyan(), ui_color_reset(), usr2_blocked);

    /* 2. Register handler for SIGUSR2 */
    struct sigaction act2;
    memset(&act2, 0, sizeof(act2));
    act2.sa_handler = handle_sigusr2;
    if (sigemptyset(&act2.sa_mask) < 0) {
        perror("sigemptyset act2.sa_mask");
        ret_code = -1;
        goto cleanup;
    }
    act2.sa_flags = 0;

    if (sigaction(SIGUSR2, &act2, NULL) < 0) {
        perror("sigaction register SIGUSR2");
        ret_code = -1;
        goto cleanup;
    }

    /* Reset handler count */
    g_usr2_handled_count = 0;

    /* 3. Send SIGUSR2 via kill() */
    printf("  -> Gui SIGUSR2 toi PID %ld qua kill()...\n", (long)self_pid);
    fflush(stdout);
    if (kill(self_pid, SIGUSR2) < 0) {
        perror("kill SIGUSR2");
        ret_code = -1;
        goto cleanup;
    }

    /* 4. Verify handler executed immediately */
    printf("  -> Kiem tra co handler ngay sau kill(): %s[HANDLED]%s (g_usr2_handled_count = %d)\n",
           ui_color_green(), ui_color_reset(), (int)g_usr2_handled_count);
    if (g_usr2_handled_count != 1) {
        fprintf(stderr, "Loi: Handler cua SIGUSR2 khong duoc thuc thi ngay lap tuc!\n");
        ret_code = -1;
        goto cleanup;
    }
    puts("  => XAC NHAN: Handler chay ngay lap tuc khi tin hieu o trang thai Unblocked!");

    /* 5. Check pending set */
    if (sigpending(&pending_set) < 0) {
        perror("sigpending for SIGUSR2");
        ret_code = -1;
        goto cleanup;
    }
    int usr2_pending = sigismember(&pending_set, SIGUSR2);
    printf("  -> Kiem tra sigpending(): SIGUSR2 is_pending = %d\n", usr2_pending);
    puts("  => Giai thich: Khi unblocked, kernel chuyen giao (deliver) truc tiep toi handler");
    puts("     ngay khi tra ve tu system call, do do tin hieu khong ton dong o pending set.");
    puts("  => Ket luan Phan 2: Unblocked signal duoc xu ly tuc thi dung tieu chuan POSIX.");
    fflush(stdout);

    /*
     * ------------------------------------------------------------------------
     * PART 3: THEORETICAL EXPLANATIONS AND COMPARISONS
     * ------------------------------------------------------------------------
     */
    print_theoretical_explanations();

cleanup:
    /*
     * ------------------------------------------------------------------------
     * STEP FINAL: CENTRALIZED CLEANUP & STATE RESTORATION
     * ------------------------------------------------------------------------
     */
    puts("");
    puts("--------------------------------------------------");
    puts("[Thu don tai nguyen] Khoi phuc an toan Signal Mask va Dispositions...");
    fflush(stdout);

    /*
     * 1. Safely consume/discard any lingering pending signals before restoring
     * default actions, preventing unexpected process crashes.
     */
    {
        sigset_t cleanup_pending;
        if (sigpending(&cleanup_pending) == 0) {
            int p_usr1 = sigismember(&cleanup_pending, SIGUSR1);
            int p_usr2 = sigismember(&cleanup_pending, SIGUSR2);
            if (p_usr1 == 1 || p_usr2 == 1) {
                struct sigaction ign_act;
                memset(&ign_act, 0, sizeof(ign_act));
                ign_act.sa_handler = SIG_IGN;
                sigemptyset(&ign_act.sa_mask);

                if (p_usr1 == 1) {
                    (void)sigaction(SIGUSR1, &ign_act, NULL);
                }
                if (p_usr2 == 1) {
                    (void)sigaction(SIGUSR2, &ign_act, NULL);
                }

                sigset_t unblock_both;
                sigemptyset(&unblock_both);
                sigaddset(&unblock_both, SIGUSR1);
                sigaddset(&unblock_both, SIGUSR2);
                (void)sigprocmask(SIG_UNBLOCK, &unblock_both, NULL);
            }
        }
    }

    /* 2. Restore original dispositions */
    if (orig_usr1_saved) {
        if (sigaction(SIGUSR1, &orig_act_usr1, NULL) < 0) {
            perror("sigaction restore SIGUSR1");
            ret_code = -1;
        } else {
            puts("  -> Da khoi phuc disposition ban dau cua SIGUSR1.");
        }
    }
    if (orig_usr2_saved) {
        if (sigaction(SIGUSR2, &orig_act_usr2, NULL) < 0) {
            perror("sigaction restore SIGUSR2");
            ret_code = -1;
        } else {
            puts("  -> Da khoi phuc disposition ban dau cua SIGUSR2.");
        }
    }

    /* 3. Restore original signal mask */
    if (orig_mask_saved) {
        if (sigprocmask(SIG_SETMASK, &orig_mask, NULL) < 0) {
            perror("sigprocmask restore orig_mask");
            ret_code = -1;
        } else {
            puts("  -> Da khoi phuc nguyen ven Signal Mask ban dau bang SIG_SETMASK.");
        }
    }

    if (ret_code == 0) {
        puts("");
        puts("Task 4 hoan tat thanh cong tat ca cac muc minh hoa!");
    } else {
        puts("");
        puts("Task 4 ket thuc voi loi!");
    }
    fflush(stdout);

    return ret_code;
}
