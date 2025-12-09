import os
import base64
import tempfile
import subprocess
import shutil
from pathlib import Path
from flask import Flask, render_template, send_from_directory, request, jsonify
from flask_cors import CORS

# =======================================================================
# FIND FFMPEG PATH
# =======================================================================
def find_ffmpeg():
    """
    Find ffmpeg executable in system PATH or common installation locations
    """
    # Try to find in PATH first
    ffmpeg_path = shutil.which('ffmpeg')
    if ffmpeg_path:
        return ffmpeg_path
    
    # Common Windows installation paths
    common_paths = [
        r"C:\ffmpeg\bin\ffmpeg.exe",
        r"C:\Program Files\ffmpeg\bin\ffmpeg.exe",
        r"C:\Program Files (x86)\ffmpeg\bin\ffmpeg.exe",
    ]
    
    # Check WinGet installation path
    local_app_data = os.getenv('LOCALAPPDATA')
    if local_app_data:
        winget_base = Path(local_app_data) / 'Microsoft' / 'WinGet' / 'Packages'
        if winget_base.exists():
            for ffmpeg_dir in winget_base.glob('Gyan.FFmpeg*'):
                ffmpeg_exe = ffmpeg_dir / 'ffmpeg*' / 'bin' / 'ffmpeg.exe'
                for exe_path in Path(ffmpeg_dir).rglob('ffmpeg.exe'):
                    if exe_path.exists():
                        return str(exe_path)
    
    # Check common paths
    for path in common_paths:
        if os.path.exists(path):
            return path
    
    return None

# Find FFmpeg at startup
FFMPEG_PATH = find_ffmpeg()
if FFMPEG_PATH:
    print(f"✅ FFmpeg found at: {FFMPEG_PATH}")
else:
    print("⚠️  FFmpeg not found. Video will be returned in AVI format.")

# =======================================================================
# FLASK APP - SERVE REACT BUILD + VIDEO CONVERSION
# =======================================================================
app = Flask(__name__, static_folder='static', template_folder='templates')
CORS(app)  # Enable CORS for all routes

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
    Convert AVI video to MP4 using FFmpeg (or return AVI if FFmpeg not available)
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
        
        # Use found FFmpeg path
        if FFMPEG_PATH:
            # Convert using FFmpeg
            ffmpeg_cmd = [
                FFMPEG_PATH,  # Use full path instead of 'ffmpeg'
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
                print(f"❌ FFmpeg conversion failed: {result.stderr.decode()}")
                # Fallback to AVI
                os.unlink(avi_path)
                return jsonify({'error': 'FFmpeg conversion failed, please install FFmpeg'}), 500
            
            # Read MP4 and encode to base64
            with open(mp4_path, 'rb') as mp4_file:
                mp4_base64 = base64.b64encode(mp4_file.read()).decode('utf-8')
            
            # Cleanup
            os.unlink(avi_path)
            os.unlink(mp4_path)
            
            return jsonify({'mp4_data': mp4_base64})
        else:
            # Return AVI directly (browsers can play AVI too)
            print("📹 Returning AVI format (FFmpeg not available)")
            with open(avi_path, 'rb') as avi_file:
                avi_base64_return = base64.b64encode(avi_file.read()).decode('utf-8')
            
            os.unlink(avi_path)
            
            return jsonify({
                'mp4_data': avi_base64_return,
                'format': 'avi',
                'warning': 'FFmpeg not installed. Install FFmpeg for MP4 conversion.'
            })
        
    except subprocess.TimeoutExpired:
        return jsonify({'error': 'Conversion timeout'}), 500
    except Exception as e:
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