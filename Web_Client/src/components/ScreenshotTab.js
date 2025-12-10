import React, { useState, useRef, useEffect } from 'react';

const ScreenshotTab = ({ isConnected, sendCommand, addLog, ws, screenFrame, setScreenFrame, isStreamingScreen, setIsStreamingScreen }) => {
  const [screenshotData, setScreenshotData] = useState(null);
  const [availableMonitors, setAvailableMonitors] = useState([]);
  const [selectedMonitor, setSelectedMonitor] = useState(-1); // -1 = all monitors
  const [isScanningMonitors, setIsScanningMonitors] = useState(false);
  const [streamQuality, setStreamQuality] = useState(50);
  const [streamSize, setStreamSize] = useState(0);

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
          // Update quality info nếu có
          if (data.quality) setStreamQuality(data.quality);
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
      addLog('Screen streaming stopped', 'info');
    }
  };



  return (
    <div className="command-section screenshot-tab">
      <h3>🖥️ Screen Capture</h3>

      {/* Monitor Selection */}
      <div className="section-group">
        <h4>📺 Monitor Selection</h4>
        <button 
          onClick={handleScanMonitors} 
          className="btn btn-scan-cam"
          disabled={!isConnected || isScanningMonitors || isStreamingScreen}
        >
          {isScanningMonitors ? '🔄 Scanning...' : '📺 Scan Monitors'}
        </button>
        
        {availableMonitors.length > 0 && (
          <div className="camera-radio-group">
            <label className="camera-group-label">📺 Available Monitors:</label>
            <div className="camera-radio-list">
              {/* All Monitors Option */}
              <label className="camera-radio-item">
                <input
                  type="radio"
                  name="monitor"
                  value={-1}
                  checked={selectedMonitor === -1}
                  onChange={(e) => setSelectedMonitor(parseInt(e.target.value))}
                  disabled={isStreamingScreen}
                />
                <div className="camera-info">
                  <span className="camera-name">🖥️ All Monitors (Virtual Screen)</span>
                  <span className="camera-specs">Combined view of all displays</span>
                </div>
              </label>
              
              {/* Individual Monitors */}
              {availableMonitors.map((mon) => (
                <label key={mon.index} className="camera-radio-item">
                  <input
                    type="radio"
                    name="monitor"
                    value={mon.index}
                    checked={selectedMonitor === mon.index}
                    onChange={(e) => setSelectedMonitor(parseInt(e.target.value))}
                    disabled={isStreamingScreen}
                  />
                  <div className="camera-info">
                    <span className="camera-name">
                      {mon.isPrimary ? '⭐ ' : ''}
                      {mon.name}
                      {mon.isPrimary ? ' (Primary)' : ''}
                    </span>
                    <span className="camera-specs">
                      {mon.resolution} • {mon.frequency} • Position: ({mon.position})
                    </span>
                  </div>
                </label>
              ))}
            </div>
          </div>
        )}
      </div>

      {/* Screenshot Controls */}
      <div className="section-group">
        <h4>📸 Screenshot</h4>
        <button 
          onClick={handleTakeScreenshot} 
          className="btn btn-command"
          disabled={!isConnected}
        >
          📸 Take Screenshot
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

      {/* Live Screen Streaming */}
      <div className="section-group">
        <h4>📡 Live Screen Stream</h4>
        <div className="camera-controls">
          <button 
            onClick={handleStartScreenStream} 
            className="btn btn-stream"
            disabled={!isConnected || isStreamingScreen}
          >
            {isStreamingScreen ? '🔴 Streaming...' : '▶️ Start Stream'}
          </button>
          <button 
            onClick={handleStopScreenStream} 
            className="btn btn-stop"
            disabled={!isConnected || !isStreamingScreen}
          >
            ⏹️ Stop
          </button>
        </div>

        {isStreamingScreen && screenFrame && (
          <div className="media-preview screen-stream-container">
            <div className="stream-badge">🔴 LIVE | Quality: {streamQuality}% | {(streamSize / 1024).toFixed(1)}KB</div>
            <img 
              src={`data:image/jpeg;base64,${screenFrame}`} 
              alt="Screen Stream" 
              className="preview-image stream"
            />
          </div>
        )}

        {isStreamingScreen && !screenFrame && (
          <div className="media-preview">
            <div className="loading-placeholder">
              <div className="spinner"></div>
              <p>Waiting for screen stream...</p>
            </div>
          </div>
        )}
      </div>


    </div>
  );
};

export default ScreenshotTab;
