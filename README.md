# Signal Handler Demo — nền tảng Task 1

Dự án C về `signal` và `sigaction` (signal bị block và không bị block). Bản hiện tại thực hiện phần nền tảng thuộc Task 1: kiến trúc module, menu terminal, logger và quản lý tiến trình cha-con bằng `fork()`/`waitpid()`. Task 2, Task 3 và Task 4 chưa được triển khai.

## Cấu trúc

- `src/main.c`: khởi tạo logger, chạy menu và đóng logger.
- `src/menu.c`: kiểm tra lựa chọn, tra registry và dispatch task qua process manager.
- `src/process_manager.c`: chạy callback trong child, đợi child và trả kết quả thoát hoặc signal cho parent.
- `src/logger.c`: logger append-only có timestamp, mức log, PID và khóa ghi giữa parent/child.
- `src/tasks.c`, `include/tasks.h`, `include/task_api.h`: registry và kiểu callback chung. Entry `NULL` nghĩa là module chưa được tích hợp.
- `include/process_manager.h`, `include/logger.h`, `include/menu.h`: API các module nền tảng.

## Quy ước callback và tích hợp

Callback task có kiểu `int (*)(void)`. Trả `0` khi thành công, hoặc mã lỗi dương từ `1` đến `125`. Process manager thực thi callback ở child và gửi mã trả về qua exit status của child. Parent nhận trạng thái từ `waitpid()`; thay đổi biến C thông thường trong child không được chia sẻ lại cho parent.

Để tích hợp module thật, mỗi thành viên cung cấp hàm `int taskN_run(void)` cho task của mình; nhóm bổ sung nguồn vào Makefile và đăng ký function pointer trong `src/tasks.c`. Tasks 2–4 hiện có entry `NULL`, không có callback giả hay demo signal giả.

Logger là tiện ích cho luồng xử lý thông thường, **không async-signal-safe**. Không gọi logger hoặc hàm định dạng/stdio từ asynchronous signal handler.

## Build và chạy trên Ubuntu WSL

```sh
make
./build/signal-handler-demo
```

Chọn `1` để xem demo parent-child; chọn `2`–`4` để thấy thông báo chưa tích hợp; chọn `0` hoặc gửi EOF để thoát. Logger ghi vào `app.log` trong thư mục hiện hành. `make clean` chỉ xóa thư mục build, giữ nguyên `app.log`.

## Chạy từ VS Code trên Windows

1. Mở thư mục `D:\java\LTHT` trong VS Code.
2. Chọn **Terminal → Run Task**.
3. Chọn **Build (WSL)** để build, hoặc **Run Demo (WSL)** để build rồi chạy trong Terminal tích hợp.
4. Với **Run Demo (WSL)**, nhập `1` để chạy Task 1 và `0` để thoát.

Hai task gọi `wsl.exe` và chạy lệnh trong `/mnt/d/java/LTHT`; chúng dùng distro WSL mặc định của Windows. Tác vụ build cũng được đặt làm build task mặc định (`Ctrl+Shift+B`).

## Kiểm tra tương tác

```sh
printf '1\n2\n0\n' | ./build/signal-handler-demo
```

Lệnh trên lần lượt chạy Task 1, chọn mục Task 2 chưa tích hợp, rồi thoát.

## Compiler và giới hạn

Makefile dùng biến `CC` của GNU Make; build hiện tại gọi compiler `cc`. Đã xác minh bằng `cc -v`: đây là GCC 15.2.0 (Ubuntu 15.2.0-16ubuntu1).

Workspace không có PDF yêu cầu gốc. Tài liệu này chỉ mô tả phần nền tảng Task 1 và không tuyên bố tuân thủ PDF. Các demo `signal()`, `sigaction()`, block/unblock thuộc Tasks 2–4 và chưa được triển khai.
