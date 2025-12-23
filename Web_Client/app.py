import os
import base64
import socket
import threading
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
from flask import Flask, render_template, send_from_directory, request, jsonify
from flask_cors import CORS
import tempfile

# =======================================================================
# LAN SCANNING - FIND AVAILABLE RAT SERVERS
# =======================================================================
# Cấu hình
WS_PORT = 9002
TARGET_IP = "127.0.0.1"

def get_local_ip_prefix():
    """
    Lấy prefix IP của máy local (VD: "192.168.1")
    """
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        local_ip = s.getsockname()[0]
        s.close()
        # Lấy 3 octet đầu (VD: "192.168.1" từ "192.168.1.100")
        return ".".join(local_ip.split(".")[:3])
    except:
        return None

def check_ip(ip):
    """
    Kiểm tra xem IP có chạy WebSocket Server C++ không
    Trả về dict nếu tìm thấy server, None nếu không
    """
    sock = None
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(0.15)  # 150ms timeout
        
        result = sock.connect_ex((ip, WS_PORT))
        
        if result == 0:
            # Shutdown socket trước khi đóng
            try:
                sock.shutdown(socket.SHUT_RDWR)
            except:
                pass
            
            # Lấy hostname (nhanh hơn nếu cache DNS)
            try:
                hostname = socket.gethostbyaddr(ip)[0]
            except:
                hostname = "Unknown Device"
            
            return {
                "ip": ip, 
                "name": hostname
            }
    except Exception:
        pass
    finally:
        if sock:
            try:
                sock.close()
            except:
                pass
    return None

def scan_lan():
    """
    Quét toàn bộ dải IP local để tìm các Server C++ đang chạy
    Trả về list các IP có thể điều khiển
    """
    prefix = get_local_ip_prefix()
    if not prefix:
        return []
    
    # Quét từ 1 đến 254
    ip_list = [f"{prefix}.{i}" for i in range(1, 255)]
    
    print(f">>> Đang quét LAN (subnet {prefix}.0/24)...")
    
    active_servers = []
    
    # Sử dụng as_completed để xử lý kết quả ngay khi có
    with ThreadPoolExecutor(max_workers=50) as executor:
        # Submit tất cả tasks
        future_to_ip = {executor.submit(check_ip, ip): ip for ip in ip_list}
        
        # Đợi và thu thập kết quả
        for future in as_completed(future_to_ip):
            try:
                result = future.result()
                if result:
                    active_servers.append(result)
                    print(f"    ✓ Tìm thấy: {result['ip']} ({result['name']})")
            except Exception as e:
                pass
    
    print(f">>> Quét xong. Tìm thấy {len(active_servers)} server(s)")
    return active_servers

# =======================================================================
# FLASK APP - SERVE REACT BUILD + VIDEO CONVERSION
# =======================================================================
app = Flask(__name__, static_folder='static', template_folder='templates')
CORS(app)  # Enable CORS for all routes

@app.route('/')
def index():
    """
    Route chính: Truyền IP mặc định ở nhập liệu
    """
    return render_template('index.html', default_ip=f"{TARGET_IP}:{WS_PORT}")

@app.route('/static/<path:filename>')
def serve_static(filename):
    """
    Serve static files (JS, CSS)
    """
    return send_from_directory('static', filename)

@app.route('/receive-video', methods=['POST'])
def receive_video():
    """
    Receive video from server and return directly (server already generates MP4)
    """
    try:
        data = request.get_json()
        video_base64 = data.get('video_data')
        
        if not video_base64:
            return jsonify({'error': 'No video data'}), 400
        
        # Server already generates MP4 format - return directly!
        print("✅ Received video from server - returning directly (no conversion needed)")
        
        return jsonify({'mp4_data': video_base64})
        
    except Exception as e:
        print(f"❌ Error handling video: {str(e)}")
        return jsonify({'error': str(e)}), 500

@app.route('/upload', methods=['POST'])
def upload_file():
    """
    Upload .bat, .exe, or .cmd file to temp directory
    """
    try:
        print(">>> Upload request received")
        print(f">>> Files in request: {list(request.files.keys())}")
        print(f">>> Form data: {list(request.form.keys())}")
        
        if 'file' not in request.files:
            print("❌ No 'file' field in request")
            return jsonify({'success': False, 'error': 'No file uploaded'}), 400
        
        file = request.files['file']
        print(f">>> File received: {file.filename}")
        
        if file.filename == '':
            print("❌ Empty filename")
            return jsonify({'success': False, 'error': 'Empty filename'}), 400
        
        # Check file extension
        allowed_extensions = {'.bat', '.exe', '.cmd'}
        file_ext = Path(file.filename).suffix.lower()
        print(f">>> File extension: {file_ext}")
        
        if file_ext not in allowed_extensions:
            print(f"❌ Invalid extension: {file_ext}")
            return jsonify({
                'success': False, 
                'error': f'Invalid file type "{file_ext}". Allowed: .bat, .exe, .cmd'
            }), 400
        
        # Save to temp directory
        upload_dir = Path(tempfile.gettempdir()) / 'rat_uploads'
        upload_dir.mkdir(exist_ok=True)
        print(f">>> Upload directory: {upload_dir}")
        
        file_path = upload_dir / file.filename
        file.save(str(file_path))
        print(f"✅ File saved: {file_path}")
        
        return jsonify({
            'success': True,
            'filePath': str(file_path),
            'filename': file.filename
        })
        
    except Exception as e:
        print(f"❌ Upload error: {str(e)}")
        import traceback
        traceback.print_exc()
        return jsonify({'success': False, 'error': str(e)}), 500

@app.route('/api/scan')
def api_scan():
    """
    API quét mạng LAN để tìm các server C++ đang chạy
    Trả về list JSON: [{"ip": "192.168.1.100", "name": "DESKTOP-ABC"}, ...]
    """
    print(">>> Đang quét mạng LAN...")
    servers = scan_lan()
    print(f"✅ Tìm thấy {len(servers)} server(s)")
    return jsonify(servers)

if __name__ == '__main__':
    print("===============================================================")
    print(">>> REMOTE ACCESS TOOL - WEB CLIENT")
    print(">>> Built with React.js + Flask")
    print("===============================================================")
    print(f"#1. Giao diện Web: http://127.0.0.1:5000")
    print(f"#2. WebSocket Target: {TARGET_IP}:{WS_PORT}")
    print("===============================================================")
    
    # use_reloader=False: Tắt tự động reload để tránh lỗi trên Windows
    app.run(port=5000, debug=True, use_reloader=False)