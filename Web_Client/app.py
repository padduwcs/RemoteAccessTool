import os
from flask import Flask, render_template

# =======================================================================
# 1. CẤU HÌNH KẾT NỐI (CONNECTION CONFIGURATION)
# =======================================================================

# [QUAN TRỌNG] THAY ĐỔI ĐỊA CHỈ IP NÀY KHI CHẠY TRÊN MẠNG LAN
# - LOCALHOST ("127.0.0.1"): Dùng khi Server C++ chạy trên CÙNG MÁY với Flask.
# - LAN IP: Dùng IP của máy Target (Ví dụ: "192.168.1.100")
TARGET_IP = "127.0.0.1" 

# Cổng WebSocket của Server C++ (Không cần thay đổi)
WS_PORT = 9002

# =======================================================================
# 2. KHỞI TẠO FLASK APP
# =======================================================================
app = Flask(__name__)

@app.route('/')
def index():
    """
    Route chính: Truyền địa chỉ WebSocket Server vào template HTML.
    """
    ws_url = f"ws://{TARGET_IP}:{WS_PORT}"
    return render_template('index.html', ws_url=ws_url)

if __name__ == '__main__':
    print("===============================================================")
    print(">>> FLASK WEB CLIENT STARTED")
    print(f"1. Giao diện Web: http://127.0.0.1:5000")
    print(f"2. WebSocket Target: {TARGET_IP}:{WS_PORT}")
    print("===============================================================")
    
    # use_reloader=False: Tắt tự động reload để tránh lỗi trên Windows
    app.run(port=5000, debug=True, use_reloader=False)