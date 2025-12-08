# 💻 RAT Web Controller (Remote Administration Tool)

**Đồ án Mạng Máy Tính:** Hệ thống điều khiển máy tính từ xa qua mạng LAN sử dụng kiến trúc **Server C++ (WebSocket)** và **Client Web (Python Flask)**.

  

## 🚀 Tính Năng Nổi Bật

Hệ thống cung cấp giao diện Web trực quan để thực hiện các tác vụ:

1.  **Quản lý Tiến trình (Process):**
      * Xem danh sách Process đang chạy (Real-time).
      * Diệt Process (theo PID hoặc Tên).
      * Khởi chạy ứng dụng/Website từ xa.
2.  **Keylogger (Giám sát bàn phím):**
      * Bắt phím thô (Raw Input), hỗ trợ bắt tổ hợp phím.
      * **Anti-Unikey:** Hiển thị chính xác phím gõ kể cả khi dùng bộ gõ tiếng Việt.
      * Xem log trực tiếp trên Web.
3.  **Đa phương tiện (Media):**
      * **Screenshot:** Chụp màn hình (Full HD, DPI Aware) và tải về ngay lập tức.
      * **Webcam:** Xem Live Stream trực tiếp (OpenCV).
      * **Record:** Quay video 10s và tải về máy Client.
4.  **Điều khiển nguồn:** Shutdown, Restart, Lock máy.

-----

## 📂 Cấu Trúc Dự Án

Dự án được tổ chức thành 3 module chính để thuận tiện cho việc Phát triển và Sử dụng:

```text
RAT_Project_Final/
│
├── 📁 1_Server_Source/       # Dành cho Developer C++
│   ├── ServerApp.sln         (Solution Visual Studio 2022)
│   ├── Source_Code/          (Mã nguồn: Keylogger, GDI+, OpenCV...)
│   └── packages.config       (Cấu hình NuGet: OpenCV, WebSocket++, Json)
│
├── 📁 2_Web_Client/          # Dành cho Developer Web/Python
│   ├── app.py                (Flask Server - Host giao diện)
│   ├── templates/index.html  (Giao diện điều khiển HTML/JS)
│   └── requirements.txt      (Thư viện Python cần thiết)
│
└── 📁 3_Ready_To_Run/        # Dành cho Tester (Chạy ngay không cần code)
    ├── ServerApp.exe         (Server đã đóng gói)
    ├── opencv_world4x0.dll   (Thư viện ảnh đi kèm)
    └── ...                   (Các DLL hỗ trợ khác)
```

-----

## 🛠️ Hướng Dẫn Cài Đặt & Chạy (Quick Start)

### 1\. Trên Máy Bị Điều Khiển (Target Machine)

*Yêu cầu: Windows 10/11 (x64).*

1.  Tải thư mục **`3_Ready_To_Run`** về máy.
2.  Chạy file **`ServerApp.exe`**.
3.  **(Lần đầu tiên)** Nếu Windows Firewall hiện thông báo, chọn **Allow Access** (cho phép cả Private & Public network).
      * *Cổng mặc định mở:* **9002**.

### 2\. Trên Máy Điều Khiển (Client Machine)

*Yêu cầu: Python 3.x đã cài đặt.*

1.  Vào thư mục **`2_Web_Client`**.
2.  Cài đặt thư viện cần thiết:
    ```bash
    pip install -r requirements.txt
    ```
3.  **(Quan trọng)** Mở file `app.py`, sửa dòng cấu hình IP:
    ```python
    # Nếu chạy cùng máy: để "127.0.0.1"
    # Nếu chạy khác máy trong LAN: Nhập IP của máy Target (ví dụ "192.168.1.10")
    TARGET_IP = "192.168.1.xxx" 
    ```
4.  Chạy ứng dụng Web:
    ```bash
    python app.py
    ```
5.  Mở trình duyệt và truy cập: `http://127.0.0.1:5000`.

-----

## ⚙️ Hướng Dẫn Build (Dành cho Dev)

Nếu bạn muốn chỉnh sửa code C++:

1.  Mở **`1_Server_Source/ServerApp.sln`** bằng **Visual Studio 2022**.
2.  Vào menu **Project** -\> **Manage NuGet Packages** -\> Nhấn **Restore** để tải thư viện.
3.  Chọn chế độ Build: **Release** - **x64**.
4.  Nhấn **F7** (Build Solution).
5.  File `.exe` mới sẽ nằm trong `1_Server_Source/x64/Release`.

-----

## ⚠️ Lưu ý Quan trọng

  * **Tường lửa (Firewall):** Nếu kết nối thất bại giữa 2 máy khác nhau, hãy kiểm tra xem máy Target đã mở **Port 9002 (TCP)** trong Windows Firewall chưa.
  * **Webcam:** Khi chuyển đổi giữa chế độ *Live Stream* và *Record*, hãy đảm bảo tắt chế độ này trước khi bật chế độ kia để tránh xung đột phần cứng.

-----

**Disclaimer:** Dự án này được xây dựng cho mục đích giáo dục và học tập môn Mạng máy tính. Tác giả không chịu trách nhiệm cho bất kỳ hành vi sử dụng sai mục đích nào.
