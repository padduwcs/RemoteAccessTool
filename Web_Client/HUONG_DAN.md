# Hướng Dẫn Sử Dụng Web Client React

## ⚠️ LƯU Ý QUAN TRỌNG

**Đã sửa lỗi JSON parse error!** React app bây giờ gửi đúng format JSON mà Server C++ mong đợi.

## Cài Đặt và Chạy

### Bước 1: Cài đặt Node.js dependencies (Chỉ cần làm 1 lần)
```bash
cd "d:\--- Study ---\Nam 2 - 2025-2026\HK1\MMT\code duck\RemoteAccessTool\Web_Client"
npm install
```

### Bước 2: Build React application
```bash
npm run build
```
Lệnh này sẽ tạo file `bundle.js` trong thư mục `static/`

### Bước 3: Chạy Flask server
```bash
python app.py
```

### Bước 4: Mở trình duyệt
Truy cập: http://127.0.0.1:5000

## Cách Sử Dụng

### 1. Kết Nối
- **Nhập địa chỉ IP Target**: Ví dụ `192.168.1.100` hoặc `10.217.40.76`
- **Cổng WebSocket**: Mặc định là `9002` (không cần thay đổi)
- Click **Kết nối** để thiết lập kết nối WebSocket

### 2. Các Chức Năng

#### Tab Thông tin (📊)
- Hiển thị thông tin kết nối hiện tại
- Trạng thái WebSocket

#### Tab Hệ Thống (🖥️)
- **Danh sách tiến trình**: Lệnh `LIST_PROC`
- **Khóa máy**: Lệnh `SYSTEM_CONTROL` với type `LOCK`
- **Tắt máy**: Lệnh `SYSTEM_CONTROL` với type `SHUTDOWN`
- **Khởi động lại**: Lệnh `SYSTEM_CONTROL` với type `RESTART`

#### Tab Media (📸)
- **Chụp màn hình**: Lệnh `SCREENSHOT` - Tự động tải file ảnh
- **Bật Live Stream Webcam**: Lệnh `START_CAM`
- **Tắt Live Stream**: Lệnh `STOP_CAM`
- **Ghi hình 10s**: Lệnh `RECORD_CAM` - Tự động tải file video

#### Tab Keylogger (⌨️)
- **Reset & Bắt đầu ghi phím**: Lệnh `START_KEYLOG`
- **Xem log phím**: Lệnh `GET_KEYLOG`

### 3. System Logs
Tất cả các hoạt động và phản hồi từ server sẽ được hiển thị trong panel **System Logs** ở dưới cùng với màu sắc khác nhau:
- 🔵 **Info**: Thông tin thông thường
- 🟢 **Success**: Lệnh thành công
- 🔴 **Error**: Lỗi
- 🟡 **Warning**: Cảnh báo

## Cấu Trúc Thư Mục

```
Web_Client/
├── src/
│   ├── components/
│   │   ├── App.js          # Component chính (ĐÃ SỬA)
│   │   └── App.css         # Styles
│   └── index.js            # Entry point
├── static/
│   └── bundle.js           # File build (tự động tạo)
├── templates/
│   └── index.html          # HTML template
├── app.py                  # Flask server
├── package.json            # Node dependencies
└── webpack.config.js       # Webpack config
```

## Development Mode

Để phát triển với live reload:
```bash
npm run dev
```
Sau mỗi lần sửa code, chạy `npm run build` để rebuild.

## Format JSON Gửi Đến Server

Tất cả lệnh được gửi dưới dạng JSON:

```json
{"cmd": "LIST_PROC"}
{"cmd": "SYSTEM_CONTROL", "type": "LOCK"}
{"cmd": "KILL_PROC", "pid": "1234"}
{"cmd": "START_PROC", "name": "notepad"}
```

## Xử Lý Response Từ Server

Server trả về JSON với các `type`:
- `ACTION_RESULT`: Kết quả thực thi lệnh
- `LIST_RESULT`: Danh sách tiến trình
- `KEYLOG_RESULT`: Dữ liệu keylog
- `SCREENSHOT_RESULT`: Ảnh base64 (tự động download)
- `CAM_FRAME`: Frame webcam
- `RECORD_RESULT`: Video base64 (tự động download)

## Lưu Ý
- Đảm bảo Server C++ đã chạy trên máy target
- IP và port phải khớp với cấu hình server
- Firewall có thể chặn kết nối WebSocket
- **ĐÃ SỬA**: Lỗi JSON parse error do format không đúng
