# RemoteAccessTool — Hướng dẫn nhanh & Tuyên bố

**Mô tả ngắn:**
Bộ công cụ truy cập từ xa gồm **Server (C++ / Windows)** và **Client (Web)** dùng WebSocket để kết nối, truyền lệnh và quản lý file/media. Chỉ dùng cho mục đích học thuật và quản trị được phép.

---

## Quick start (Prebuilt)

### Server
1. Giải nén `Server_Side_(hide_console).zip` (ẩn console) hoặc `Server_Side.zip` (hiện console).
2. Chạy `ServerApp.exe`.
3. Kiểm tra console: nếu thành công sẽ thấy: `WebSocket server listening on port 9002`.

### Client
1. Giải nén `Client_Side.zip`.
2. Chạy `START_CLIENT.bat` (script sẽ tự kiểm tra và cài dependencies: `npm install` hoặc `pip install`).
3. Khi giao diện hiện, nhập IP/Port của Server (mặc định `9002`) hoặc chọn quét tự động, sau đó nhấn **Connect**.

> **Lưu ý:** Mở cổng `9002` trên firewall nếu cần.

---

## Build từ mã nguồn (Developer)

**Server (C++ / Windows)**
- Mở `ServerApp.sln` bằng Visual Studio 2019+.
- Cài thư viện phụ thuộc qua NuGet: `websocketpp`, `OpenCV`, `nlohmann/json`.
- Chọn cấu hình **Release** và **x64**, rồi Build.

**Client (Web)**
- Vào thư mục `Web_Client/`.
- Chạy `START_CLIENT.bat` hoặc: `npm install` rồi `npm start` (hoặc `pip install -r requirements.txt` và `python app.py` nếu cần).

---

## Cấu hình nhanh
- Cổng mặc định: **9002** (thay đổi trong mã nguồn nếu cần).

---

## Tuyên bố (Disclaimer, ngắn gọn)
- Phần mềm này **chỉ phục vụ mục đích học thuật và quản trị hệ thống được phép**.
- **Cấm** sử dụng để truy cập, điều khiển hoặc thu thập dữ liệu từ hệ thống mà bạn không có quyền hợp lệ.
- Tác giả **không chịu trách nhiệm** cho mọi hậu quả pháp lý hoặc thiệt hại phát sinh từ việc sử dụng trái phép.
- Mọi hoạt động kiểm thử bảo mật phải có **sự cho phép bằng văn bản** của chủ hệ thống.

---

## Liên hệ & đóng góp
- Mở Issue hoặc gửi PR trên repository để đóng góp hoặc báo lỗi.

---

*Ngắn gọn, rõ ràng; muốn tôi chỉnh tone (trang trọng/học thuật/khẩn) hoặc thêm ví dụ lệnh cụ thể không?*