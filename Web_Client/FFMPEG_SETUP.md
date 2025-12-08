# Hướng dẫn cài đặt FFmpeg

Web client cần FFmpeg để chuyển đổi video từ AVI sang MP4 (để phát trên trình duyệt).

## Cài đặt FFmpeg trên Windows

### Cách 1: Sử dụng Chocolatey (Khuyến nghị)
```bash
choco install ffmpeg
```

### Cách 2: Tải thủ công

1. Tải FFmpeg từ: https://www.gyan.dev/ffmpeg/builds/
   - Chọn bản: **ffmpeg-release-essentials.zip**

2. Giải nén vào thư mục (ví dụ: `C:\ffmpeg`)

3. Thêm vào PATH:
   - Mở "Environment Variables" (Biến môi trường)
   - Tìm biến `Path` trong "System variables"
   - Thêm đường dẫn: `C:\ffmpeg\bin`
   - Click OK

4. Kiểm tra cài đặt:
```bash
ffmpeg -version
```

## Kiểm tra FFmpeg đã hoạt động

Mở terminal và chạy:
```bash
ffmpeg -version
```

Nếu thấy thông tin phiên bản → Cài đặt thành công! ✅

## Khởi động Web Client

Sau khi cài đặt FFmpeg:
```bash
python app.py
```

Server sẽ tự động convert video AVI từ webcam sang MP4 để phát trực tiếp trên trình duyệt.

## Lưu ý

- FFmpeg chỉ cần cài **1 lần**
- Video sẽ được convert tự động khi nhận từ server
- Nếu lỗi convert, video AVI sẽ tự động tải xuống máy
