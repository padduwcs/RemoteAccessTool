import os
import base64
import tempfile
import subprocess
from pathlib import Path
from flask import Flask, render_template, send_from_directory, request, jsonify

# =======================================================================
# FLASK APP - SERVE REACT BUILD + VIDEO CONVERSION
# =======================================================================
app = Flask(__name__, static_folder='static', template_folder='templates')

@app.route('/')
def index():
    """
    Route chính: Serve React application
    """
    return render_template('index.html')

@app.route('/static/<path:filename>')
def serve_static(filename):
    """
    Serve static files (JS, CSS)
    """
    return send_from_directory('static', filename)

@app.route('/convert-video', methods=['POST'])
def convert_video():
    """
    Convert AVI video to MP4 using FFmpeg
    """
    try:
        data = request.get_json()
        avi_base64 = data.get('video_data')
        
        if not avi_base64:
            return jsonify({'error': 'No video data'}), 400
        
        # Decode base64
        avi_bytes = base64.b64decode(avi_base64)
        
        # Create temp files
        with tempfile.NamedTemporaryFile(suffix='.avi', delete=False) as avi_file:
            avi_file.write(avi_bytes)
            avi_path = avi_file.name
        
        mp4_path = avi_path.replace('.avi', '.mp4')
        
        # Convert using FFmpeg
        ffmpeg_cmd = [
            'ffmpeg',
            '-i', avi_path,
            '-c:v', 'libx264',
            '-preset', 'fast',
            '-crf', '23',
            '-c:a', 'aac',
            '-b:a', '128k',
            '-movflags', '+faststart',
            '-y',
            mp4_path
        ]
        
        result = subprocess.run(
            ffmpeg_cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=30
        )
        
        if result.returncode != 0:
            # Cleanup
            os.unlink(avi_path)
            if os.path.exists(mp4_path):
                os.unlink(mp4_path)
            return jsonify({'error': 'FFmpeg conversion failed'}), 500
        
        # Read MP4 and encode to base64
        with open(mp4_path, 'rb') as mp4_file:
            mp4_base64 = base64.b64encode(mp4_file.read()).decode('utf-8')
        
        # Cleanup
        os.unlink(avi_path)
        os.unlink(mp4_path)
        
        return jsonify({'mp4_data': mp4_base64})
        
    except subprocess.TimeoutExpired:
        return jsonify({'error': 'Conversion timeout'}), 500
    except Exception as e:
        return jsonify({'error': str(e)}), 500

@app.route('/api/scan')
def api_scan():
    print(">>> Đang quét mạng LAN...")
    servers = scan_lan()
    return jsonify(servers)

if __name__ == '__main__':
    print("===============================================================")
    print(">>> REMOTE ACCESS TOOL - WEB CLIENT")
    print(">>> Built with React.js + Flask")
    print("===============================================================")
    print(f"🌐 Giao diện Web: http://127.0.0.1:5000")
    print("📝 Nhập IP target trên giao diện web để kết nối")
    print("⚠️  Yêu cầu: FFmpeg phải được cài đặt và có trong PATH")
    print("===============================================================")
    
    # use_reloader=False: Tắt tự động reload để tránh lỗi trên Windows
    app.run(port=5000, debug=True, use_reloader=False)