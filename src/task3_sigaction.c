#include "task3_sigaction.h"
#include "logger.h"
#include "ui_theme.h"

#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/*
 * ============================================================================
 * ASYNC-SIGNAL-SAFE FLAGS AND DATA STORAGE
 * ============================================================================
 */
static volatile sig_atomic_t g_basic_got_sigint = 0;
static volatile sig_atomic_t g_basic_got_sigusr1 = 0;

static volatile sig_atomic_t g_siginfo_received = 0;
static volatile sig_atomic_t g_siginfo_signo = 0;
static volatile pid_t g_siginfo_pid = 0;
static volatile uid_t g_siginfo_uid = 0;
static volatile int g_siginfo_code = 0;

static volatile sig_atomic_t g_restart_got_sigusr1 = 0;

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
 * ============================================================================
 * SIGNAL HANDLERS (STRICTLY ASYNC-SIGNAL-SAFE)
 * ============================================================================
 */

/* Handlers for Part 1: Basic sigaction */
static void handle_basic_sigint(int sig)
{
    int saved_errno = errno;
    (void)sig;
    g_basic_got_sigint = 1;
    const char msg[] = "    [Signal Handler (sigaction)] Nhan duoc SIGINT (Signal 2)!\n";
    safe_write_all(STDOUT_FILENO, msg, sizeof(msg) - 1);
    errno = saved_errno;
}

static void handle_basic_sigusr1(int sig)
{
    int saved_errno = errno;
    (void)sig;
    g_basic_got_sigusr1 = 1;
    const char msg[] = "    [Signal Handler (sigaction)] Nhan duoc SIGUSR1 (Signal 10)!\n";
    safe_write_all(STDOUT_FILENO, msg, sizeof(msg) - 1);
    errno = saved_errno;
}

/* Handler for Part 2: SA_SIGINFO with 3 arguments */
static void handle_siginfo(int signo, siginfo_t *info, void *context)
{
    int saved_errno = errno;
    (void)context;
    if (info != NULL) {
        g_siginfo_received = 1;
        g_siginfo_signo = signo;
        g_siginfo_pid = info->si_pid;
        g_siginfo_uid = info->si_uid;
        g_siginfo_code = info->si_code;
    }
    const char msg[] = "    [Signal Handler (SA_SIGINFO)] Nhan SIGUSR1 va trich xuat siginfo_t!\n";
    safe_write_all(STDOUT_FILENO, msg, sizeof(msg) - 1);
    errno = saved_errno;
}

/* Handler for Part 3: SA_RESTART demonstration */
static void handle_restart_sigusr1(int sig)
{
    int saved_errno = errno;
    (void)sig;
    g_restart_got_sigusr1 = 1;
    const char msg[] = "    [Signal Handler (SA_RESTART)] Nhan SIGUSR1 trong khi syscall dang cho!\n";
    safe_write_all(STDOUT_FILENO, msg, sizeof(msg) - 1);
    errno = saved_errno;
}

/*
 * ============================================================================
 * SYNCHRONIZATION AND CLEANUP HELPERS
 * ============================================================================
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
 * Bounded timeout wait for tokens to prevent parent hanging indefinitely
 * if child is blocked, signal was dropped, or race conditions occur.
 *
 * Uses CLOCK_MONOTONIC to compute an absolute deadline, guaranteeing that
 * repeated EINTR interrupts do not reset or extend the total timeout.
 * Inspects POLLIN, POLLHUP, POLLERR, and POLLNVAL explicitly.
 */
static int sync_wait_token_timeout(int fd, char expected_token, int timeout_ms)
{
    if (fd < 0 || timeout_ms < 0) {
        errno = (fd < 0) ? EBADF : EINVAL;
        return -1;
    }

    struct timespec start_ts;
    if (clock_gettime(CLOCK_MONOTONIC, &start_ts) != 0) {
        return -1;
    }

    /* Calculate absolute deadline in monotonic milliseconds */
    long long deadline_ms = (long long)start_ts.tv_sec * 1000LL +
                           (long long)start_ts.tv_nsec / 1000000LL +
                           (long long)timeout_ms;

    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    while (1) {
        struct timespec now_ts;
        if (clock_gettime(CLOCK_MONOTONIC, &now_ts) != 0) {
            return -1;
        }

        long long now_ms = (long long)now_ts.tv_sec * 1000LL +
                           (long long)now_ts.tv_nsec / 1000000LL;
        long long remaining_ms = deadline_ms - now_ms;

        if (remaining_ms <= 0) {
            errno = ETIMEDOUT;
            return -1;
        }

        int poll_timeout = (remaining_ms > (long long)INT_MAX) ? INT_MAX : (int)remaining_ms;

        pfd.revents = 0;
        int ret = poll(&pfd, 1, poll_timeout);
        if (ret < 0) {
            if (errno == EINTR) {
                /* Loop again; remaining timeout will be recalculated based on monotonic clock */
                continue;
            }
            return -1;
        }
        if (ret == 0) {
            errno = ETIMEDOUT;
            return -1;
        }

        /* Check for error conditions on the file descriptor */
        if (pfd.revents & (POLLERR | POLLNVAL)) {
            errno = (pfd.revents & POLLNVAL) ? EBADF : EIO;
            return -1;
        }

        /* Check for readable data or hang-up (EOF) */
        if (pfd.revents & (POLLIN | POLLHUP)) {
            char token = 0;
            ssize_t n;
            do {
                n = read(fd, &token, 1);
            } while (n < 0 && errno == EINTR);

            if (n == 1) {
                if (token == expected_token) {
                    return 0;
                }
                /* Token mismatch */
                errno = EPROTO;
                return -1;
            }
            if (n == 0) {
                /* EOF reached without receiving the token (e.g. child closed pipe) */
                errno = EPIPE;
                return -1;
            }
            /* read error (other than EINTR) */
            return -1;
        }

        /* Unrecognized revents */
        return -1;
    }
}

static void sleep_ms(long ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    while (nanosleep(&ts, &ts) < 0 && errno == EINTR) {
        /* continue sleeping remainder if interrupted */
    }
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
 * ============================================================================
 * PHẦN 1: SỬ DỤNG sigaction() ĐỂ ĐĂNG KÝ SIGNAL HANDLER
 * ============================================================================
 */
static int demo_basic_sigaction(void)
{
    puts("--------------------------------------------------");
    puts("[Phan 1] Su dung sigaction() de dang ky handler (SIGINT va SIGUSR1)");
    puts("Dac diem thiet ke:");
    puts("  - Su dung struct sigaction voi sa_handler.");
    puts("  - Khoi tao sa_mask bang sigemptyset().");
    puts("  - Kiem tra gia tri tra ve cua sigaction() khi dang ky.");
    puts("  - Gui tin hieu co chu dich bang kill() de tranh anh huong toi menu cha.");
    fflush(stdout);

    int sync_pipe[2];
    if (pipe(sync_pipe) < 0) {
        perror("pipe demo_basic_sigaction");
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork demo_basic_sigaction");
        close(sync_pipe[0]);
        close(sync_pipe[1]);
        return -1;
    }

    if (pid == 0) {
        /* Child: Closes read end */
        close(sync_pipe[0]);

        struct sigaction sa_int;
        memset(&sa_int, 0, sizeof(sa_int));
        sa_int.sa_handler = handle_basic_sigint;
        if (sigemptyset(&sa_int.sa_mask) < 0) {
            close(sync_pipe[1]);
            _exit(1);
        }
        sa_int.sa_flags = 0;

        struct sigaction sa_usr1;
        memset(&sa_usr1, 0, sizeof(sa_usr1));
        sa_usr1.sa_handler = handle_basic_sigusr1;
        if (sigemptyset(&sa_usr1.sa_mask) < 0) {
            close(sync_pipe[1]);
            _exit(1);
        }
        sa_usr1.sa_flags = 0;

        /* Register handlers via sigaction() and verify return value */
        if (sigaction(SIGINT, &sa_int, NULL) < 0 ||
            sigaction(SIGUSR1, &sa_usr1, NULL) < 0) {
            const char err_msg[] = "Loi sigaction() khi dang ky handler!\n";
            safe_write_all(STDERR_FILENO, err_msg, sizeof(err_msg) - 1);
            close(sync_pipe[1]);
            _exit(1);
        }

        /* Notify parent: Handlers registered and ready */
        if (sync_send_token(sync_pipe[1], 'R') != 0) {
            close(sync_pipe[1]);
            _exit(1);
        }

        /* Wait for SIGINT */
        while (!g_basic_got_sigint) {
            pause();
        }
        /* Acknowledge SIGINT handled */
        if (sync_send_token(sync_pipe[1], '1') != 0) {
            close(sync_pipe[1]);
            _exit(1);
        }

        /* Wait for SIGUSR1 */
        while (!g_basic_got_sigusr1) {
            pause();
        }
        /* Acknowledge SIGUSR1 handled */
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
        fprintf(stderr, "Loi dong bo: Child khong san sang trong Phan 1.\n");
        close(sync_pipe[0]);
        (void)cleanup_child(pid, 1);
        return -1;
    }
    printf("  -> Tien trinh con (PID=%ld) da dang ky thanh cong voi sigaction().\n", (long)pid);

    /* Send SIGINT */
    printf("  -> Cha gui SIGINT toi con (PID=%ld) qua kill()...\n", (long)pid);
    fflush(stdout);
    if (kill(pid, SIGINT) < 0) {
        perror("kill SIGINT");
        close(sync_pipe[0]);
        (void)cleanup_child(pid, 1);
        return -1;
    }

    if (sync_wait_token(sync_pipe[0], '1') != 0) {
        fprintf(stderr, "Loi dong bo: Child khong xac nhan SIGINT.\n");
        close(sync_pipe[0]);
        (void)cleanup_child(pid, 1);
        return -1;
    }
    puts("  -> Cha nhan xac nhan: Child da xu ly SIGINT an toan bang sigaction()!");

    /* Send SIGUSR1 */
    printf("  -> Cha gui SIGUSR1 toi con (PID=%ld) qua kill()...\n", (long)pid);
    fflush(stdout);
    if (kill(pid, SIGUSR1) < 0) {
        perror("kill SIGUSR1");
        close(sync_pipe[0]);
        (void)cleanup_child(pid, 1);
        return -1;
    }

    if (sync_wait_token(sync_pipe[0], '2') != 0) {
        fprintf(stderr, "Loi dong bo: Child khong xac nhan SIGUSR1.\n");
        close(sync_pipe[0]);
        (void)cleanup_child(pid, 1);
        return -1;
    }
    puts("  -> Cha nhan xac nhan: Child da xu ly SIGUSR1 an toan bang sigaction()!");

    close(sync_pipe[0]);

    int status;
    pid_t waited;
    do {
        waited = waitpid(pid, &status, 0);
    } while (waited < 0 && errno == EINTR);

    if (waited > 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        puts("  => Ket luan Phan 1: Dang ky va xu ly tin hieu bang sigaction() thanh cong tuyet doi.");
    } else {
        puts("  => Canh bao: Tien trinh con Phan 1 ket thuc bat thuong.");
        return -1;
    }
    return 0;
}

/*
 * ============================================================================
 * PHẦN 2: SỬ DỤNG SA_SIGINFO VÀ siginfo_t
 * ============================================================================
 */
static int demo_siginfo(void)
{
    puts("\n--------------------------------------------------");
    puts("[Phan 2] Su dung SA_SIGINFO va cau truc siginfo_t");
    puts("Muc tieu:");
    puts("  - Dang ky qua sa_sigaction voi co SA_SIGINFO.");
    puts("  - Handler 3 tham so: (int signo, siginfo_t *info, void *context).");
    puts("  - Trich xuat: signo, si_pid, si_uid, si_code (SI_USER tu kill()).");
    puts("  - Khong printf trong handler; luu an toan va in o luong chinh ngoai handler.");
    fflush(stdout);

    int sync_pipe[2];
    if (pipe(sync_pipe) < 0) {
        perror("pipe demo_siginfo");
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork demo_siginfo");
        close(sync_pipe[0]);
        close(sync_pipe[1]);
        return -1;
    }

    if (pid == 0) {
        /* Child: Receiver process */
        close(sync_pipe[0]);

        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_sigaction = handle_siginfo;
        if (sigemptyset(&sa.sa_mask) < 0) {
            close(sync_pipe[1]);
            _exit(1);
        }
        sa.sa_flags = SA_SIGINFO; /* Bat co SA_SIGINFO */

        if (sigaction(SIGUSR1, &sa, NULL) < 0) {
            const char err_msg[] = "Loi sigaction(SA_SIGINFO) trong child!\n";
            safe_write_all(STDERR_FILENO, err_msg, sizeof(err_msg) - 1);
            close(sync_pipe[1]);
            _exit(1);
        }

        /* Notify parent: Ready to receive signal with SA_SIGINFO */
        if (sync_send_token(sync_pipe[1], 'R') != 0) {
            close(sync_pipe[1]);
            _exit(1);
        }

        /* Wait for signal */
        while (!g_siginfo_received) {
            pause();
        }

        /* Outside handler: Safely format and display siginfo_t details */
        printf("\n  [Child %ld] Nhan duoc thong tin chi tiet tu siginfo_t (in ben ngoai handler):\n",
               (long)getpid());
        printf("    + signo           : %d (SIGUSR1)\n", (int)g_siginfo_signo);
        printf("    + info->si_pid    : %ld (PID tien trinh gui)\n", (long)g_siginfo_pid);
        printf("    + getppid() thuc te: %ld -> %s\n",
               (long)getppid(),
               (g_siginfo_pid == getppid()) ? "KHOP HOAN TOAN" : "KHONG KHOP");
        printf("    + info->si_uid    : %ld (UID tien trinh gui)\n", (long)g_siginfo_uid);
        printf("    + info->si_code   : %d ", g_siginfo_code);
        if (g_siginfo_code == SI_USER) {
            puts("(SI_USER: Tin hieu duoc gui boi kill()/raise() tu user-space)");
        } else {
            printf("(Code: %d)\n", g_siginfo_code);
        }
        fflush(stdout);

        /* Send acknowledgement token to parent */
        if (sync_send_token(sync_pipe[1], 'A') != 0) {
            close(sync_pipe[1]);
            _exit(1);
        }

        close(sync_pipe[1]);
        _exit(0);
    }

    /* Parent: Sender process */
    close(sync_pipe[1]);

    if (sync_wait_token(sync_pipe[0], 'R') != 0) {
        fprintf(stderr, "Loi dong bo: Child SA_SIGINFO khong san sang.\n");
        close(sync_pipe[0]);
        (void)cleanup_child(pid, 1);
        return -1;
    }

    printf("  -> Cha (PID=%ld, UID=%ld) chuan bi gui SIGUSR1 toi con (PID=%ld)...\n",
           (long)getpid(), (long)getuid(), (long)pid);
    fflush(stdout);

    if (kill(pid, SIGUSR1) < 0) {
        perror("kill SIGUSR1");
        close(sync_pipe[0]);
        (void)cleanup_child(pid, 1);
        return -1;
    }

    if (sync_wait_token(sync_pipe[0], 'A') != 0) {
        fprintf(stderr, "Loi: Khong nhan duoc xac nhan SA_SIGINFO tu con.\n");
        close(sync_pipe[0]);
        (void)cleanup_child(pid, 1);
        return -1;
    }
    puts("  -> Cha nhan duoc xac nhan: Child da in va kiem tra siginfo_t thanh cong!");

    close(sync_pipe[0]);

    int status;
    pid_t waited;
    do {
        waited = waitpid(pid, &status, 0);
    } while (waited < 0 && errno == EINTR);

    if (waited > 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        puts("  => Ket luan Phan 2: SA_SIGINFO trich xuat ngu canh nguon gui chinh xac.");
    } else {
        puts("  => Canh bao: Tien trinh con Phan 2 ket thuc bat thuong.");
        return -1;
    }
    return 0;
}

/*
 * ============================================================================
 * PHẦN 3: SỬ DỤNG SA_RESTART ĐỐI VỚI BLOCKING SYSTEM CALL (read trên pipe)
 * ============================================================================
 */
static int demo_sa_restart(void)
{
    puts("\n--------------------------------------------------");
    puts("[Phan 3] Minh hoa SA_RESTART tren blocking system call (read pipe)");
    puts("Nguyen ly:");
    puts("  - Khi mot blocking syscall (nhu read) dang cho I/O tren pipe rong:");
    puts("    + Khong co SA_RESTART: Syscall bi ngat boi tin hieu va tra ve -1, errno == EINTR.");
    puts("    + Co SA_RESTART: Kernel tu dong khoi dong lai syscall mot cach trong suot,");
    puts("      tiep tuc cho den khi co du lieu duoc ghi vao pipe.");
    puts("  * Ghi chu ky thuat ve gioi han dong bo POSIX:");
    puts("    - sleep_ms(10) chi la do tre thuc nghiem de nhuong CPU cho tien trinh con");
    puts("      chuyen ngu canh vao syscall read(), khong phai bao dam chac chan theo POSIX");
    puts("      rang con da vao trang thai block trong kernel wait queue.");
    puts("    - Tien trinh cha ap dung co che timeout co gioi han (3000ms qua poll) khi cho");
    puts("      ket qua. Neu tin hieu den qua som hoac ket qua khong xuat hien, cha se ngat,");
    puts("      thu gom child bang cleanup_child() va bao loi ro rang, khong de treo hoac zombie.");
    fflush(stdout);

    /* ------------------------------------------------------------------------
     * 3.1 THỬ NGHIỆM KHÔNG CÓ SA_RESTART (flags = 0)
     * ------------------------------------------------------------------------ */
    puts("\n  [3.1] Truong hop KHONG CO SA_RESTART (sa_flags = 0):");
    fflush(stdout);
    int sync_pipe1[2];
    int data_pipe1[2];
    if (pipe(sync_pipe1) < 0 || pipe(data_pipe1) < 0) {
        perror("pipe 3.1");
        return -1;
    }

    g_restart_got_sigusr1 = 0;
    pid_t child1 = fork();
    if (child1 < 0) {
        perror("fork 3.1");
        close(sync_pipe1[0]); close(sync_pipe1[1]);
        close(data_pipe1[0]); close(data_pipe1[1]);
        return -1;
    }

    if (child1 == 0) {
        /* Child 1 */
        close(sync_pipe1[0]);
        close(data_pipe1[1]); /* Child reads from data_pipe1 */

        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = handle_restart_sigusr1;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0; /* KHONG CO SA_RESTART */

        if (sigaction(SIGUSR1, &sa, NULL) < 0) {
            close(sync_pipe1[1]);
            close(data_pipe1[0]);
            _exit(1);
        }

        /* Announce Ready right before entering read() */
        if (sync_send_token(sync_pipe1[1], 'R') != 0) {
            close(sync_pipe1[1]);
            close(data_pipe1[0]);
            _exit(1);
        }

        /* Calling read() on empty pipe -> Will block */
        char buf[32];
        ssize_t n = read(data_pipe1[0], buf, sizeof(buf));
        int saved_read_errno = errno;

        /* Verify that read() returned -1 with EINTR */
        if (n < 0 && saved_read_errno == EINTR) {
            printf("    [Child %ld] read() tra ve -1, errno == %d (EINTR: %s)!\n",
                   (long)getpid(), saved_read_errno, strerror(saved_read_errno));
            puts("    => Xac nhan: Khi KHONG CO SA_RESTART, syscall bi ngat boi tin hieu dung nhu thiet ke.");
            fflush(stdout);
            (void)sync_send_token(sync_pipe1[1], '1');
            close(sync_pipe1[1]);
            close(data_pipe1[0]);
            _exit(0);
        } else {
            printf("    [Child %ld] Ket qua bat ngo: read() tra ve %zd, errno=%d\n",
                   (long)getpid(), n, saved_read_errno);
            close(sync_pipe1[1]);
            close(data_pipe1[0]);
            _exit(1);
        }
    }

    /* Parent 1 */
    close(sync_pipe1[1]);
    close(data_pipe1[0]); /* Parent writes to data_pipe1 (if needed) */

    if (sync_wait_token_timeout(sync_pipe1[0], 'R', 3000) != 0) {
        if (errno == ETIMEDOUT) {
            fprintf(stderr, "Loi: Timeout cho Ready tu child1 (qua 3000ms)!\n");
        } else {
            fprintf(stderr, "Loi dong bo child1.\n");
        }
        close(sync_pipe1[0]);
        close(data_pipe1[1]);
        (void)cleanup_child(child1, 1);
        return -1;
    }

    /* Delay slightly (10ms) to ensure child1 enters the blocking read syscall */
    sleep_ms(10);

    printf("    -> Cha gui SIGUSR1 toi con (PID=%ld) dang cho o read()...\n", (long)child1);
    fflush(stdout);
    if (kill(child1, SIGUSR1) < 0) {
        perror("kill child1");
        close(sync_pipe1[0]);
        close(data_pipe1[1]);
        (void)cleanup_child(child1, 1);
        return -1;
    }

    /* Bounded timeout (3000ms) waiting for EINTR acknowledgment from child1 */
    if (sync_wait_token_timeout(sync_pipe1[0], '1', 3000) != 0) {
        if (errno == ETIMEDOUT) {
            fprintf(stderr, "Loi: Timeout cho phan hoi tu child1 (qua 3000ms). Khong the xac nhan hanh vi EINTR!\n");
        } else {
            fprintf(stderr, "Loi: Child1 khong xac nhan EINTR.\n");
        }
        close(sync_pipe1[0]);
        close(data_pipe1[1]);
        (void)cleanup_child(child1, 1);
        return -1;
    }

    close(sync_pipe1[0]);
    close(data_pipe1[1]);

    int status1;
    pid_t waited1;
    do {
        waited1 = waitpid(child1, &status1, 0);
    } while (waited1 < 0 && errno == EINTR);

    if (waited1 > 0 && WIFEXITED(status1) && WEXITSTATUS(status1) == 0) {
        puts("    => Ket qua 3.1: Child1 da xu ly EINTR va ket thuc thanh cong.");
    } else {
        puts("    => Canh bao: Child1 ket thuc bat thuong.");
        return -1;
    }

    /* ------------------------------------------------------------------------
     * 3.2 THỬ NGHIỆM CÓ SA_RESTART (flags = SA_RESTART)
     * ------------------------------------------------------------------------ */
    puts("\n  [3.2] Truong hop CO SA_RESTART (sa_flags = SA_RESTART):");
    fflush(stdout);
    int sync_pipe2[2];
    int data_pipe2[2];
    if (pipe(sync_pipe2) < 0 || pipe(data_pipe2) < 0) {
        perror("pipe 3.2");
        return -1;
    }

    g_restart_got_sigusr1 = 0;
    pid_t child2 = fork();
    if (child2 < 0) {
        perror("fork 3.2");
        close(sync_pipe2[0]); close(sync_pipe2[1]);
        close(data_pipe2[0]); close(data_pipe2[1]);
        return -1;
    }

    if (child2 == 0) {
        /* Child 2 */
        close(sync_pipe2[0]);
        close(data_pipe2[1]); /* Child reads from data_pipe2 */

        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = handle_restart_sigusr1;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART; /* CO SA_RESTART */

        if (sigaction(SIGUSR1, &sa, NULL) < 0) {
            close(sync_pipe2[1]);
            close(data_pipe2[0]);
            _exit(1);
        }

        /* Announce Ready right before entering read() */
        if (sync_send_token(sync_pipe2[1], 'R') != 0) {
            close(sync_pipe2[1]);
            close(data_pipe2[0]);
            _exit(1);
        }

        /* Calling read() on empty pipe -> Will block, gets interrupted, and auto-restarts */
        char buf[32];
        memset(buf, 0, sizeof(buf));
        ssize_t n = read(data_pipe2[0], buf, sizeof(buf) - 1);

        if (n > 0 && g_restart_got_sigusr1) {
            printf("    [Child %ld] read() KHONG bi huy boi EINTR! Tu dong restart va doc thanh cong: '%s' (%zd bytes)\n",
                   (long)getpid(), buf, n);
            puts("    => Xac nhan: Voi SA_RESTART, kernel tu dong khoi dong lai syscall mot cach trong suot!");
            fflush(stdout);
            (void)sync_send_token(sync_pipe2[1], '2');
            close(sync_pipe2[1]);
            close(data_pipe2[0]);
            _exit(0);
        } else {
            printf("    [Child %ld] Loi: read() that bai n=%zd, got_sig=%d, errno=%d (%s)\n",
                   (long)getpid(), n, (int)g_restart_got_sigusr1, errno, strerror(errno));
            close(sync_pipe2[1]);
            close(data_pipe2[0]);
            _exit(1);
        }
    }

    /* Parent 2 */
    close(sync_pipe2[1]);
    close(data_pipe2[0]); /* Parent writes to data_pipe2 */

    if (sync_wait_token_timeout(sync_pipe2[0], 'R', 3000) != 0) {
        if (errno == ETIMEDOUT) {
            fprintf(stderr, "Loi: Timeout cho Ready tu child2 (qua 3000ms)!\n");
        } else {
            fprintf(stderr, "Loi dong bo child2.\n");
        }
        close(sync_pipe2[0]);
        close(data_pipe2[1]);
        (void)cleanup_child(child2, 1);
        return -1;
    }

    /* Delay slightly (10ms) to ensure child2 enters the blocking read syscall */
    sleep_ms(10);

    printf("    -> Cha gui SIGUSR1 toi con (PID=%ld) dang cho o read()...\n", (long)child2);
    fflush(stdout);
    if (kill(child2, SIGUSR1) < 0) {
        perror("kill child2");
        close(sync_pipe2[0]);
        close(data_pipe2[1]);
        (void)cleanup_child(child2, 1);
        return -1;
    }

    /* Wait a moment for signal handler to run and kernel to restart read() */
    sleep_ms(10);

    /* Now parent feeds data into data_pipe2 to satisfy the restarted read() */
    const char test_data[] = "HELLO_RESTART";
    printf("    -> Cha ghi du lieu ('%s') vao data_pipe2 de danh thuc read() da restart...\n", test_data);
    fflush(stdout);
    if (write(data_pipe2[1], test_data, sizeof(test_data) - 1) < 0) {
        perror("write data_pipe2");
        close(sync_pipe2[0]);
        close(data_pipe2[1]);
        (void)cleanup_child(child2, 1);
        return -1;
    }

    /* Bounded timeout (3000ms) waiting for SA_RESTART acknowledgment from child2 */
    if (sync_wait_token_timeout(sync_pipe2[0], '2', 3000) != 0) {
        if (errno == ETIMEDOUT) {
            fprintf(stderr, "Loi: Timeout cho phan hoi tu child2 (qua 3000ms). Khong the xac nhan hanh vi SA_RESTART!\n");
        } else {
            fprintf(stderr, "Loi: Child2 khong xac nhan SA_RESTART hoan tat.\n");
        }
        close(sync_pipe2[0]);
        close(data_pipe2[1]);
        (void)cleanup_child(child2, 1);
        return -1;
    }

    close(sync_pipe2[0]);
    close(data_pipe2[1]);

    int status2;
    pid_t waited2;
    do {
        waited2 = waitpid(child2, &status2, 0);
    } while (waited2 < 0 && errno == EINTR);

    if (waited2 > 0 && WIFEXITED(status2) && WEXITSTATUS(status2) == 0) {
        puts("    => Ket qua 3.2: Child2 nhan du lieu sau khi restart thanh cong tuyet doi.");
    } else {
        puts("    => Canh bao: Child2 ket thuc bat thuong.");
        return -1;
    }

    puts("  => Ket luan Phan 3: Su khac biet giua co va khong co SA_RESTART duoc chung minh ro rang.");
    return 0;
}

/*
 * ============================================================================
 * PHẦN 4: SO SÁNH TRỰC QUAN GIỮA signal() VÀ sigaction()
 * ============================================================================
 */
static void explain_sigaction_comparison(void)
{
    puts("\n--------------------------------------------------");
    puts("[Phan 4] Bang so sanh truc quan giua signal() va sigaction()");
    puts("+---------------------+---------------------------------+---------------------------------+");
    puts("| Tieu chi            | signal()                        | sigaction() (POSIX.1-2008)      |");
    puts("+---------------------+---------------------------------+---------------------------------+");
    puts("| 1. Cau truc API     | Don gian: truyen con tro ham    | Struct sigaction linh hoat      |");
    puts("|                     | void (*)(int)                   | (sa_handler / sa_sigaction)     |");
    puts("+---------------------+---------------------------------+---------------------------------+");
    puts("| 2. Mat na tin hieu  | Khong ho tro                    | sa_mask (sigset_t): chan tin    |");
    puts("|    (sa_mask)        | Nguy co bi ngat chong cheo      | hieu tuy bien trong khi handler |");
    puts("|                     | (nested signal interruption)    | dang chay de tranh race condition|");
    puts("+---------------------+---------------------------------+---------------------------------+");
    puts("| 3. Ngu canh tin hieu| Chi co so hieu: int signo       | Co SA_SIGINFO va siginfo_t:     |");
    puts("|    (Context Info)   | Khong biet PID/UID cua ben gui  | Cung cap si_pid, si_uid, si_code|");
    puts("|                     |                                 | dia chi loi bo nho (si_addr)... |");
    puts("+---------------------+---------------------------------+---------------------------------+");
    puts("| 4. Khoi dong lai    | Khong kiem soat duoc            | Co SA_RESTART: chu dong chon    |");
    puts("|    syscall bi ngat  | (Phu thuoc cai dat he thong)    | tu dong restart syscall hoac    |");
    puts("|    (Restartability) |                                 | tra ve -1 kem errno == EINTR    |");
    puts("+---------------------+---------------------------------+---------------------------------+");
    puts("| 5. Tinh nhat quan   | System V: tu reset ve SIG_DFL   | POSIX tieu chuan hoa hoan toan  |");
    puts("|    va di dong       | BSD: giu nguyen handler         | Hanh vi ro rang, nhat quan tren |");
    puts("|    (Portability)    | POSIX coi la implementation-def | moi ban phan phoi UNIX / Linux  |");
    puts("+---------------------+---------------------------------+---------------------------------+");
    fflush(stdout);
}

/*
 * ============================================================================
 * MAIN ENTRY POINT FOR TASK 3
 * ============================================================================
 */
int task3_sigaction_demo(void)
{
    puts("");
    puts("==========================================================================");
    printf("        %s%sBAT DAU TASK 3: ADVANCED SIGNAL (sigaction)%s                 \n",
           ui_color_bold(), ui_color_cyan(), ui_color_reset());
    puts("==========================================================================");
    fflush(stdout);

    /*
     * Ignore SIGPIPE to ensure write() to a pipe whose read-end was closed
     * returns -1 with errno == EPIPE instead of killing the process with SIGPIPE.
     */
    void (*old_sigpipe)(int) = signal(SIGPIPE, SIG_IGN);

    int ret = 0;

    /* Phan 1: Basic sigaction() with SIGINT and SIGUSR1 */
    if (demo_basic_sigaction() != 0) {
        fprintf(stderr, "That bai o Phan 1 (demo_basic_sigaction).\n");
        ret = 1;
        goto cleanup;
    }

    /* Phan 2: SA_SIGINFO and siginfo_t */
    if (demo_siginfo() != 0) {
        fprintf(stderr, "That bai o Phan 2 (demo_siginfo).\n");
        ret = 2;
        goto cleanup;
    }

    /* Phan 3: SA_RESTART vs non-SA_RESTART on blocking read() */
    if (demo_sa_restart() != 0) {
        fprintf(stderr, "That bai o Phan 3 (demo_sa_restart).\n");
        ret = 3;
        goto cleanup;
    }

    /* Phan 4: Comprehensive comparison between signal() and sigaction() */
    explain_sigaction_comparison();

    puts("\nTask 3 hoan tat thanh cong tat ca cac muc minh hoa!");
    fflush(stdout);

cleanup:
    if (old_sigpipe != SIG_ERR) {
        (void)signal(SIGPIPE, old_sigpipe);
    }
    return ret;
}
