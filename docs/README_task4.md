# Task 4: Blocked / Unblocked Signals

## Chuc nang

- Chan SIGINT va SIGUSR1 bang sigprocmask().
- Mo phong cong viec quan trong qua 5 luot xu ly.
- Lay tap tin hieu pending bang sigpending().
- Kiem tra tung tin hieu bang sigismember().
- Khoi phuc mask ban dau de xu ly tin hieu dang cho.
- Khoi phuc handler cu truoc khi ket thuc.
- Handler chi dat co volatile sig_atomic_t; khong goi printf/logger.

## Cac tep

- include/signal_masking.h: khai bao API.
- src/signal_masking.c: cai dat demo va callback task4_run().
- tests/test_masking.c: main() de chay thu doc lap.
- docs/README_task4.md: huong dan.

## Chay trong chuong trinh chung

Tai thu muc goc repo:

make
./build/signal-handler-demo

Chon 4. Chuong trinh tao tien trinh con de chay demo.
Mo Terminal khac va gui tin hieu den PID in trong phan demo:

kill -INT PID
kill -USR1 PID

Thay PID bang PID con cua lan chay hien tai.
Gui sau thong bao da chan va truoc khi cong viec ket thuc.
Khong nhan Ctrl+C trong Terminal chay menu chung:
Ctrl+C co the gui SIGINT den ca tien trinh cha.

Ket qua mong doi:
- Hai tin hieu hien DANG CHO trong luc bi chan.
- Cong viec hoan tat truoc khi mo chan.
- Handler xu ly hai tin hieu sau khi mo chan.
- Child ket thuc voi exit=0 va menu xuat hien lai.

## Chay thu doc lap

Tai thu muc goc repo:

gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude tests/test_masking.c src/signal_masking.c -o /tmp/task4-masking-test
/tmp/task4-masking-test

Ban doc lap co the thu SIGINT bang Ctrl+C sau thong bao da chan.

## Cac truong hop kiem thu

1. Khong gui tin hieu: ca hai handler chua nhan tin hieu.
2. Chi SIGINT: chi SIGINT pending va duoc xu ly.
3. Chi SIGUSR1: chi SIGUSR1 pending va duoc xu ly.
4. Gui ca hai: ca hai pending va duoc xu ly sau khi mo chan.

Thu tu in ket qua khong the hien thu tu handler thuc su chay.

## Quy uoc tich hop

- task4_run() tra 0 khi thanh cong, 1 khi co loi.
- demo_signal_masking() tra 0 khi thanh cong, -1 khi co loi.
- Su dung trong tien trinh don luong.
- De demo mo chan ca hai tin hieu, SIGINT va SIGUSR1
  phai khong bi chan truoc khi vao module.
- Khong dua tests/test_masking.c vao ban build chung vi co main() rieng.
