import os
import socket
from concurrent.futures import ThreadPoolExecutor
from flask import Flask, render_template, jsonify

app = Flask(__name__)
WS_PORT = 9002

# --- MODULE QUÉT LAN ---
def get_local_ip_prefix():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        local_ip = s.getsockname()[0]
        s.close()
        return ".".join(local_ip.split(".")[:3])
    except:
        return None

def check_ip(ip, active_servers):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(0.05) # 50ms timeout để quét nhanh
    try:
        if s.connect_ex((ip, WS_PORT)) == 0:
            try:
                hostname = socket.gethostbyaddr(ip)[0]
            except:
                hostname = "Unknown Device"
            active_servers.append({"ip": ip, "name": hostname})
    except: pass
    finally: s.close()

def scan_lan():
    prefix = get_local_ip_prefix()
    if not prefix: return []
    active_servers = []
    # Quét từ .1 đến .254
    ip_list = [f"{prefix}.{i}" for i in range(1, 255)]
    with ThreadPoolExecutor(max_workers=100) as executor:
        for ip in ip_list:
            executor.submit(check_ip, ip, active_servers)
    return active_servers

# --- ROUTES ---
@app.route('/')
def index():
    return render_template('index.html', default_ip="127.0.0.1")

@app.route('/api/scan')
def api_scan():
    print(">>> Đang quét mạng LAN...")
    servers = scan_lan()
    return jsonify(servers)

if __name__ == '__main__':
    print(">>> WEB CLIENT STARTED: http://127.0.0.1:5000")
    app.run(port=5000, debug=True, use_reloader=False)