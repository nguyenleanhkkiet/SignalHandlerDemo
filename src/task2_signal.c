#include "task2_signal.h"
#include "logger.h"
#include "ui_theme.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/*
 * Async-signal-safe flags.
 * According to POSIX, sig_atomic_t is guaranteed to be atomic with respect to signals.
 */
static volatile sig_atomic_t g_got_sigint = 0;
static volatile sig_atomic_t g_got_sigtstp = 0;
static volatile sig_atomic_t g_got_sigusr1 = 0;
static volatile sig_atomic_t g_got_sigusr2 = 0;

/*
 * Async-signal-safe string write helper.
 * Properly checks return values to satisfy -Werror=unused-result and handles EINTR.
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

/*
 * Signal Handlers: Strictly async-signal-safe.
 * We preserve errno, write using safe_write_all(), and set sig_atomic_t flags.
 * No printf, malloc, exit, or logger calls are permitted inside handlers.
 */
static void handle_sigint(int sig)
{
    int saved_errno = errno;
    (void)sig;
    g_got_sigint = 1;
    const char msg[] = "    [Signal Handler] Nhan duoc SIGINT (Signal 2)!\n";
    safe_write_all(STDOUT_FILENO, msg, sizeof(msg) - 1);
    errno = saved_errno;
}

static void handle_sigtstp(int sig)
{
    int saved_errno = errno;
    (void)sig;
    g_got_sigtstp = 1;
    const char msg[] = "    [Signal Handler] Nhan duoc SIGTSTP (Signal 20)!\n";
    safe_write_all(STDOUT_FILENO, msg, sizeof(msg) - 1);
    errno = saved_errno;
}

static void handle_sigusr1(int sig)
{
    int saved_errno = errno;
    (void)sig;
    g_got_sigusr1 = 1;
    const char msg[] = "    [Signal Handler] Nhan duoc SIGUSR1 (Signal 10)!\n";
    safe_write_all(STDOUT_FILENO, msg, sizeof(msg) - 1);
    errno = saved_errno;
}

static void handle_sigusr2(int sig)
{
    int saved_errno = errno;
    (void)sig;
    g_got_sigusr2 = 1;
    const char msg[] = "    [Signal Handler] Nhan duoc SIGUSR2 (Signal 12)!\n";
    safe_write_all(STDOUT_FILENO, msg, sizeof(msg) - 1);
    errno = saved_errno;
}

/*
 * Helper functions for reliable pipe-based synchronization between parent and child.
 * Avoids arbitrary sleep() calls and prevents signal loss / race conditions.
 */
static int sync_send_token(int fd, char token)
{
    ssize_t n;
    do {
        n = write(fd, &token, 1);
    } while (n < 0 && errno == EINTR);
    return (n == 1) ? 0 : -1;
}

static int sync_wait_token(int fd, char expected_token)
{
    char token = 0;
    ssize_t n;
    do {
        n = read(fd, &token, 1);
    } while (n < 0 && errno == EINTR);
    if (n != 1 || token != expected_token) {
        return -1;
    }
    return 0;
}

/*
 * Safely cleans up a child process on error branches.
 * Sends SIGKILL if force_kill is true to unblock/terminate stuck children,
 * then reaps with waitpid() to guarantee no zombies are left behind.
 * Returns 0 on success (child reaped or already gone), -1 on error.
 */
static int cleanup_child(pid_t pid, int force_kill)
{
    if (pid <= 0) {
        return -1;
    }

    if (force_kill) {
        int kill_rc;
        do {
            kill_rc = kill(pid, SIGKILL);
        } while (kill_rc < 0 && errno == EINTR);

        if (kill_rc < 0 && errno != ESRCH) {
            perror("cleanup_child: kill");
            return -1;
        }
    }

    int status;
    pid_t waited;
    do {
        waited = waitpid(pid, &status, 0);
    } while (waited < 0 && errno == EINTR);

    if (waited == pid) {
        return 0;
    }
    if (waited < 0 && errno == ECHILD) {
        /* Child is no longer waitable (may have already been reaped) */
        return 0;
    }

    perror("cleanup_child: waitpid");
    return -1;
}

/*
 * Sub-demo 1: Custom Signal Handling with signal() for SIGINT and SIGTSTP.
 * Uses targeted kill() to a dedicated child process to verify handlers safely
 * without broadcasting signals to the terminal foreground process group.
 */
static int demo_custom_handlers(void)
{
    puts("--------------------------------------------------");
    puts("[Phan 1] Minh hoa signal() voi SIGINT va SIGTSTP");
    puts("Giai thich bao ve tien trinh cha:");
    puts("  - Phim tat terminal (Ctrl+C, Ctrl+Z) phat tin hieu toi toan bo foreground process group.");
    puts("  - De bao ve tien trinh menu cha khong bi ket thuc hoac dung bat ngo,");
    puts("    ban demo su dung kill() co chu dich gui truc tiep vao PID tien trinh thu nghiem.");
    fflush(stdout);

    int sync_pipe[2];
    if (pipe(sync_pipe) < 0) {
        perror("pipe demo_custom_handlers");
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork demo_custom_handlers");
        close(sync_pipe[0]);
        close(sync_pipe[1]);
        return -1;
    }

    if (pid == 0) {
        /* Child process: Install custom handlers */
        close(sync_pipe[0]);

        if (signal(SIGINT, handle_sigint) == SIG_ERR ||
            signal(SIGTSTP, handle_sigtstp) == SIG_ERR) {
            const char err_msg[] = "Loi cai dat signal handler trong child!\n";
            safe_write_all(STDERR_FILENO, err_msg, sizeof(err_msg) - 1);
            close(sync_pipe[1]);
            _exit(1);
        }

        /* Notify parent that handlers are registered */
        if (sync_send_token(sync_pipe[1], 'R') != 0) {
            close(sync_pipe[1]);
            _exit(1);
        }

        /* Wait for SIGINT */
        while (!g_got_sigint) {
            pause();
        }
        /* Acknowledge SIGINT handled */
        if (sync_send_token(sync_pipe[1], '1') != 0) {
            close(sync_pipe[1]);
            _exit(1);
        }

        /* Wait for SIGTSTP */
        while (!g_got_sigtstp) {
            pause();
        }
        /* Acknowledge SIGTSTP handled */
        if (sync_send_token(sync_pipe[1], '2') != 0) {
            close(sync_pipe[1]);
            _exit(1);
        }

        close(sync_pipe[1]);
        _exit(0);
    }

    /* Parent process */
    close(sync_pipe[1]);

    if (sync_wait_token(sync_pipe[0], 'R') != 0) {
        fprintf(stderr, "Loi dong bo: Child khong san sang.\n");
        close(sync_pipe[0]);
        cleanup_child(pid, 1);
        return -1;
    }
    printf("  -> Tien trinh con (PID=%ld) da san sang voi custom handler.\n", (long)pid);

    printf("  -> Cha gui SIGINT toi PID %ld qua kill()...\n", (long)pid);
    fflush(stdout);
    if (kill(pid, SIGINT) < 0) {
        perror("kill SIGINT");
        close(sync_pipe[0]);
        cleanup_child(pid, 1);
        return -1;
    }

    if (sync_wait_token(sync_pipe[0], '1') != 0) {
        fprintf(stderr, "Loi dong bo: Child khong xac nhan SIGINT.\n");
        close(sync_pipe[0]);
        cleanup_child(pid, 1);
        return -1;
    }
    puts("  -> Cha nhan duoc xac nhan: Child da xu ly SIGINT thanh cong!");

    printf("  -> Cha gui SIGTSTP toi PID %ld qua kill()...\n", (long)pid);
    fflush(stdout);
    if (kill(pid, SIGTSTP) < 0) {
        perror("kill SIGTSTP");
        close(sync_pipe[0]);
        cleanup_child(pid, 1);
        return -1;
    }

    if (sync_wait_token(sync_pipe[0], '2') != 0) {
        fprintf(stderr, "Loi dong bo: Child khong xac nhan SIGTSTP.\n");
        close(sync_pipe[0]);
        cleanup_child(pid, 1);
        return -1;
    }
    puts("  -> Cha nhan duoc xac nhan: Child da xu ly SIGTSTP thanh cong!");

    close(sync_pipe[0]);

    int status;
    pid_t waited;
    do {
        waited = waitpid(pid, &status, 0);
    } while (waited < 0 && errno == EINTR);

    if (waited > 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        puts("  => Ket luan Phan 1: Custom handlers hoat dong dung va tien trinh con thoat 0.");
    } else {
        puts("  => Canh bao: Tien trinh con ket thuc bat thuong.");
        return -1;
    }
    return 0;
}

/*
 * Sub-demo 2: Inter-Process Signaling using kill() with SIGUSR1 and SIGUSR2.
 * Uses pipe synchronization after SIGUSR1 before sending SIGUSR2 to prevent
 * signal coalescence and race conditions.
 */
static int demo_ipc_kill(void)
{
    puts("\n--------------------------------------------------");
    puts("[Phan 2] Giao tiep tien trinh cha - con bang kill(SIGUSR1, SIGUSR2)");
    puts("Cơ che dong bo bang pipe:");
    puts("  1. Con bat dau va dang ky handler cho SIGUSR1 va SIGUSR2.");
    puts("  2. Con gui byte 'R' (Ready) thong bao da cai dat xong.");
    puts("  3. Cha gui SIGUSR1, cho con xu ly xong va xac nhan bang byte '1'.");
    puts("  4. Chi sau khi co xac nhan, cha moi gui SIGUSR2 (tranh gop tin hieu).");
    puts("  5. Con xu ly SIGUSR2, xac nhan byte '2' va ket thuc.");
    fflush(stdout);

    int sync_pipe[2];
    if (pipe(sync_pipe) < 0) {
        perror("pipe demo_ipc_kill");
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork demo_ipc_kill");
        close(sync_pipe[0]);
        close(sync_pipe[1]);
        return -1;
    }

    if (pid == 0) {
        /* Child */
        close(sync_pipe[0]);

        if (signal(SIGUSR1, handle_sigusr1) == SIG_ERR ||
            signal(SIGUSR2, handle_sigusr2) == SIG_ERR) {
            const char err_msg[] = "Loi cai dat SIGUSR1/SIGUSR2 handler!\n";
            safe_write_all(STDERR_FILENO, err_msg, sizeof(err_msg) - 1);
            close(sync_pipe[1]);
            _exit(1);
        }

        /* 1. Notify ready */
        if (sync_send_token(sync_pipe[1], 'R') != 0) {
            close(sync_pipe[1]);
            _exit(1);
        }

        /* 2. Wait for SIGUSR1 */
        while (!g_got_sigusr1) {
            pause();
        }
        /* Acknowledge SIGUSR1 */
        if (sync_send_token(sync_pipe[1], '1') != 0) {
            close(sync_pipe[1]);
            _exit(1);
        }

        /* 3. Wait for SIGUSR2 */
        while (!g_got_sigusr2) {
            pause();
        }
        /* Acknowledge SIGUSR2 */
        if (sync_send_token(sync_pipe[1], '2') != 0) {
            close(sync_pipe[1]);
            _exit(1);
        }

        close(sync_pipe[1]);
        _exit(0);
    }

    /* Parent */
    close(sync_pipe[1]);

    if (sync_wait_token(sync_pipe[0], 'R') != 0) {
        fprintf(stderr, "Loi dong bo: Child IPC khong san sang.\n");
        close(sync_pipe[0]);
        cleanup_child(pid, 1);
        return -1;
    }
    printf("  -> Tien trinh con IPC (PID=%ld) da san sang.\n", (long)pid);

    printf("  -> Cha gui SIGUSR1 toi con %ld...\n", (long)pid);
    fflush(stdout);
    if (kill(pid, SIGUSR1) < 0) {
        perror("kill SIGUSR1");
        close(sync_pipe[0]);
        cleanup_child(pid, 1);
        return -1;
    }

    if (sync_wait_token(sync_pipe[0], '1') != 0) {
        fprintf(stderr, "Loi: Khong nhan duoc xac nhan SIGUSR1 tu con.\n");
        close(sync_pipe[0]);
        cleanup_child(pid, 1);
        return -1;
    }
    puts("  -> Cha nhan xac nhan: Con da hoan tat xu ly SIGUSR1 an toan.");

    printf("  -> Cha gui tiep SIGUSR2 toi con %ld...\n", (long)pid);
    fflush(stdout);
    if (kill(pid, SIGUSR2) < 0) {
        perror("kill SIGUSR2");
        close(sync_pipe[0]);
        cleanup_child(pid, 1);
        return -1;
    }

    if (sync_wait_token(sync_pipe[0], '2') != 0) {
        fprintf(stderr, "Loi: Khong nhan duoc xac nhan SIGUSR2 tu con.\n");
        close(sync_pipe[0]);
        cleanup_child(pid, 1);
        return -1;
    }
    puts("  -> Cha nhan xac nhan: Con da hoan tat xu ly SIGUSR2 an toan.");

    close(sync_pipe[0]);

    int status;
    pid_t waited;
    do {
        waited = waitpid(pid, &status, 0);
    } while (waited < 0 && errno == EINTR);

    if (waited > 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        puts("  => Ket luan Phan 2: Giao tiep IPC cha-con bang kill() va dong bo pipe thanh cong tuyet doi.");
    } else {
        puts("  => Canh bao: Tien trinh con IPC ket thuc bat thuong.");
        return -1;
    }
    return 0;
}

/*
 * Sub-demo 3: Safe demonstration of SIG_IGN and SIG_DFL using dedicated child processes.
 * Ensures the main menu and Task 2 manager are never inadvertently killed.
 */
static int demo_sig_ign_and_dfl(void)
{
    puts("\n--------------------------------------------------");
    puts("[Phan 3] Minh hoa SIG_IGN va SIG_DFL trong tien trinh con rieng biet");
    fflush(stdout);

    /* 3.1 Test SIG_IGN */
    puts("  [3.1] Kiem tra SIG_IGN voi SIGUSR1:");
    int ready_pipe[2];
    int release_pipe[2];
    if (pipe(ready_pipe) < 0) {
        perror("pipe ready_pipe");
        return -1;
    }
    if (pipe(release_pipe) < 0) {
        perror("pipe release_pipe");
        close(ready_pipe[0]);
        close(ready_pipe[1]);
        return -1;
    }

    pid_t ign_child = fork();
    if (ign_child < 0) {
        perror("fork demo_sig_ign");
        close(ready_pipe[0]);
        close(ready_pipe[1]);
        close(release_pipe[0]);
        close(release_pipe[1]);
        return -1;
    }

    if (ign_child == 0) {
        /* Child: closes unused pipe ends */
        close(ready_pipe[0]);    /* Child writes to ready_pipe */
        close(release_pipe[1]);  /* Child reads from release_pipe */

        /* Set SIG_IGN: Ignore SIGUSR1 */
        if (signal(SIGUSR1, SIG_IGN) == SIG_ERR) {
            close(ready_pipe[1]);
            close(release_pipe[0]);
            _exit(1);
        }

        /* Notify parent that SIG_IGN is registered and child is ready */
        if (sync_send_token(ready_pipe[1], 'R') != 0) {
            close(ready_pipe[1]);
            close(release_pipe[0]);
            _exit(1);
        }

        /*
         * Wait for parent's release token 'C' to exit.
         * While child is blocked here, parent sends SIGUSR1.
         * Since SIGUSR1 is ignored, child survives undisturbed.
         */
        if (sync_wait_token(release_pipe[0], 'C') != 0) {
            close(ready_pipe[1]);
            close(release_pipe[0]);
            _exit(1);
        }

        close(ready_pipe[1]);
        close(release_pipe[0]);
        _exit(0);
    }

    /* Parent: closes unused pipe ends */
    close(ready_pipe[1]);    /* Parent reads from ready_pipe */
    close(release_pipe[0]);  /* Parent writes to release_pipe */

    if (sync_wait_token(ready_pipe[0], 'R') != 0) {
        fprintf(stderr, "Loi dong bo: ign_child khong san sang.\n");
        close(ready_pipe[0]);
        close(release_pipe[1]);
        cleanup_child(ign_child, 1);
        return -1;
    }

    printf("    -> Con (PID=%ld) da thiet lap signal(SIGUSR1, SIG_IGN).\n", (long)ign_child);
    printf("    -> Cha gui SIGUSR1 toi con qua kill()...\n");
    fflush(stdout);
    if (kill(ign_child, SIGUSR1) < 0) {
        perror("kill ign_child");
        close(ready_pipe[0]);
        close(release_pipe[1]);
        cleanup_child(ign_child, 1);
        return -1;
    }

    /* Send release token 'C' to allow child to complete and terminate cleanly */
    printf("    -> Cha gui token giai phong cho con qua release_pipe sau khi kill()...\n");
    fflush(stdout);
    if (sync_send_token(release_pipe[1], 'C') != 0) {
        fprintf(stderr, "Loi gui token giai phong toi ign_child.\n");
        close(ready_pipe[0]);
        close(release_pipe[1]);
        cleanup_child(ign_child, 1);
        return -1;
    }

    close(ready_pipe[0]);
    close(release_pipe[1]);

    int ign_status;
    pid_t waited_ign;
    do {
        waited_ign = waitpid(ign_child, &ign_status, 0);
    } while (waited_ign < 0 && errno == EINTR);

    if (waited_ign > 0 && WIFEXITED(ign_status) && WEXITSTATUS(ign_status) == 0) {
        printf("    -> Con ket thuc voi ma thoat %d binh thuong (khong bi terminate boi SIGUSR1).\n",
               WEXITSTATUS(ign_status));
        puts("    => Xac nhan SIG_IGN: Tin hieu bi bo qua hoan toan va tien trinh con thoat sach se.");
    } else {
        puts("    => Loi: Tien trinh con bi anh huong bat ngo.");
        return -1;
    }

    /* 3.2 Test SIG_DFL */
    puts("\n  [3.2] Kiem tra SIG_DFL (khoi phuc hanh vi mac dinh):");
    int dfl_pipe[2];
    if (pipe(dfl_pipe) < 0) {
        perror("pipe demo_sig_dfl");
        return -1;
    }

    pid_t dfl_child = fork();
    if (dfl_child < 0) {
        perror("fork demo_sig_dfl");
        close(dfl_pipe[0]);
        close(dfl_pipe[1]);
        return -1;
    }

    if (dfl_child == 0) {
        close(dfl_pipe[0]);
        /* First install custom handler, then reset to SIG_DFL */
        (void)signal(SIGUSR1, handle_sigusr1);
        if (signal(SIGUSR1, SIG_DFL) == SIG_ERR) {
            close(dfl_pipe[1]);
            _exit(1);
        }
        if (sync_send_token(dfl_pipe[1], 'R') != 0) {
            close(dfl_pipe[1]);
            _exit(1);
        }

        /* Wait to be terminated by default action of SIGUSR1 */
        for (;;) {
            pause();
        }
        _exit(0);
    }

    close(dfl_pipe[1]);
    if (sync_wait_token(dfl_pipe[0], 'R') != 0) {
        fprintf(stderr, "Loi dong bo dfl_child.\n");
        close(dfl_pipe[0]);
        cleanup_child(dfl_child, 1);
        return -1;
    }
    close(dfl_pipe[0]);

    printf("    -> Con (PID=%ld) da khoi phuc signal(SIGUSR1, SIG_DFL).\n", (long)dfl_child);
    printf("    -> Cha gui SIGUSR1 toi con qua kill()...\n");
    fflush(stdout);
    if (kill(dfl_child, SIGUSR1) < 0) {
        perror("kill dfl_child");
        cleanup_child(dfl_child, 1);
        return -1;
    }

    int dfl_status;
    pid_t waited_dfl;
    do {
        waited_dfl = waitpid(dfl_child, &dfl_status, 0);
    } while (waited_dfl < 0 && errno == EINTR);

    if (waited_dfl > 0 && WIFSIGNALED(dfl_status)) {
        int term_sig = WTERMSIG(dfl_status);
        printf("    -> Con PID=%ld da bi ket thuc boi signal %d (SIGUSR1).\n", (long)dfl_child, term_sig);
        printf("    => Xac nhan SIG_DFL: Hanh vi mac dinh (cham dut tien trinh) duoc thuc thi boi he dieu hanh.\n");
        puts("    => Tien trinh cha Task 2 va menu van hoan toan an toan va tiep tuc hoat dong.");
    } else {
        puts("    => Loi: dfl_child khong bi ket thuc boi tin hieu nhu mong doi.");
        return -1;
    }
    return 0;
}

/*
 * Sub-demo 4: Accurate technical explanation of signal() limitations compared to sigaction().
 */
static void explain_signal_limitations(void)
{
    puts("\n--------------------------------------------------");
    puts("[Phan 4] Phan tich han che ky thuat cua signal() so voi sigaction()");
    puts("1. Tinh di dong va hanh vi thiet lap lai (Signal Resetting):");
    puts("   - Tren he thong System V, moi khi handler duoc goi, he dieu hanh tu");
    puts("     dong reset xu ly ve SIG_DFL. Neu muon bat tiep, handler phai goi");
    puts("     lai signal(). Dieu nay tao race condition nguy hiem neu tin hieu");
    puts("     den truoc khi handler kip goi lai signal().");
    puts("   - Nguoc lai, tren BSD truyen thong, handler van duoc giu nguyen.");
    puts("   - Chuan POSIX.1 coi day la khong xac dinh (implementation-defined),");
    puts("     trong khi sigaction() quy dinh ro rang qua co SA_RESETHAND.");
    puts("\n2. Khong co kha nang cau hinh mat na tin hieu (Signal Mask):");
    puts("   - signal() khong cho phep chi dinh tap tin hieu can chan (blocked)");
    puts("     trong thoi gian handler dang thuc thi.");
    puts("   - sigaction() cung cap sa_mask (sigset_t), cho phep chan an toan bat");
    puts("     ky tap tin hieu nao de tranh bi ngat chong cheo (nested signals).");
    puts("\n3. Khong kiem soat duoc tinh chat restart system call:");
    puts("   - signal() khong kiem soat duoc lieu syscall bi ngat (nhu read) co tu");
    puts("     dong restart hay that bai voi ma loi EINTR.");
    puts("   - sigaction() cung cap co SA_RESTART de chu dong lua chon hanh vi nay.");
    puts("\n4. Thieu thong tin chi tiet ve nguon tin hieu (Lack of Signal Context):");
    puts("   - Handler cua signal() chi nhan duoc so hieu int sig.");
    puts("   - sigaction() ho tro co SA_SIGINFO va handler 3 tham so, cho phep biet");
    puts("     ro PID/UID cua ben gui, ma loi, dia chi loi va ngu canh CPU.");
    puts("--------------------------------------------------");
    fflush(stdout);
}

int task2_signal_demo(void)
{
    puts("");
    puts("==========================================================================");
    printf("        %s%sBAT DAU TASK 2: BASIC SIGNAL HANDLING (signal)%s              \n",
           ui_color_bold(), ui_color_cyan(), ui_color_reset());
    puts("==========================================================================");
    fflush(stdout);

    /*
     * Ignore SIGPIPE to ensure write() to a pipe whose read-end was closed
     * returns -1 with errno == EPIPE instead of killing the process with SIGPIPE.
     */
    void (*old_sigpipe)(int) = signal(SIGPIPE, SIG_IGN);

    int ret = 0;

    /* 1. Custom handlers for SIGINT & SIGTSTP */
    if (demo_custom_handlers() != 0) {
        fprintf(stderr, "That bai o Phan 1 (demo_custom_handlers).\n");
        ret = 1;
        goto cleanup;
    }

    /* 2. IPC signaling with kill() */
    if (demo_ipc_kill() != 0) {
        fprintf(stderr, "That bai o Phan 2 (demo_ipc_kill).\n");
        ret = 2;
        goto cleanup;
    }

    /* 3. SIG_IGN and SIG_DFL */
    if (demo_sig_ign_and_dfl() != 0) {
        fprintf(stderr, "That bai o Phan 3 (demo_sig_ign_and_dfl).\n");
        ret = 3;
        goto cleanup;
    }

    /* 4. Technical explanation of limitations */
    explain_signal_limitations();

    puts("Task 2 hoan tat thanh cong tat ca cac muc minh hoa!");
    fflush(stdout);

cleanup:
    if (old_sigpipe != SIG_ERR) {
        (void)signal(SIGPIPE, old_sigpipe);
    }
    return ret;
}
