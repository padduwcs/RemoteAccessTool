import React, { useState, useEffect } from 'react';
import './FileManager.css';

const FileManager = ({ ws, addLog }) => {
  const [currentPath, setCurrentPath] = useState(null);
  const [items, setItems] = useState([]);
  const [selectedItem, setSelectedItem] = useState(null);
  const [loading, setLoading] = useState(false);
  const [pathHistory, setPathHistory] = useState([]);
  const [historyIndex, setHistoryIndex] = useState(-1);
  
  // Drive picker state
  const [showDrivePicker, setShowDrivePicker] = useState(true);
  
  // File editor states
  const [editingFile, setEditingFile] = useState(null);
  const [fileContent, setFileContent] = useState('');
  const [isEditorOpen, setIsEditorOpen] = useState(false);
  
  // Context menu states
  const [contextMenu, setContextMenu] = useState({ visible: false, x: 0, y: 0 });
  
  // Create/Rename dialog states
  const [dialog, setDialog] = useState({ type: null, visible: false, value: '' });
  
  // Upload/Download states
  const [uploading, setUploading] = useState(false);
  const [downloading, setDownloading] = useState(false);
  
  // Available drives
  const [drives] = useState(['C:\\', 'D:\\', 'E:\\', 'F:\\', 'G:\\']);
  
  // Custom notification system
  const [notification, setNotification] = useState({ 
    visible: false, 
    type: 'confirm', // 'confirm' or 'alert'
    title: '', 
    message: '', 
    onConfirm: null,
    onCancel: null
  });

  // Load directory on mount and when path changes
  useEffect(() => {
    if (currentPath && ws && ws.readyState === WebSocket.OPEN) {
      loadDirectory(currentPath);
    }
  }, [currentPath]);

  // Handle WebSocket messages
  useEffect(() => {
    if (!ws) return;

    const handleMessage = (event) => {
      try {
        const data = JSON.parse(event.data);
        
        if (data.type === 'DIR_LIST') {
          setLoading(false);
          if (data.success) {
            const itemCount = data.items ? data.items.length : 0;
            
            // Sort items: folders first, then files by extension, then by name
            const sortedItems = (data.items || []).sort((a, b) => {
              // Folders always come first
              if (a.isDirectory && !b.isDirectory) return -1;
              if (!a.isDirectory && b.isDirectory) return 1;
              
              // If both are files, sort by extension first
              if (!a.isDirectory && !b.isDirectory) {
                const extA = (a.extension || '').toLowerCase();
                const extB = (b.extension || '').toLowerCase();
                
                if (extA !== extB) {
                  return extA.localeCompare(extB);
                }
              }
              
              // Then sort by name (case-insensitive)
              return a.name.toLowerCase().localeCompare(b.name.toLowerCase());
            });
            
            setItems(sortedItems);
            addLog(`📁 Loaded ${itemCount} item(s) from ${data.path || currentPath}`, 'info');
          } else {
            addLog('❌ Error: ' + (data.error || 'Unknown error'), 'error');
            setItems([]);
          }
        }
        else if (data.type === 'FILE_CONTENT') {
          if (data.success) {
            let content = data.content || '';
            
            // Check if content is Base64 encoded
            if (data.encoding === 'base64') {
              // Decode Base64 to text (for binary files that can be displayed)
              try {
                content = atob(content);
              } catch (e) {
                addLog('⚠️ Binary file - showing raw data', 'warning');
              }
            }
            
            setFileContent(content);
            setIsEditorOpen(true);
            addLog('📖 File opened in editor', 'info');
          } else {
            addLog('❌ Failed to read file', 'error');
          }
        }
        else if (data.type === 'FILE_DOWNLOAD') {
          setDownloading(false);
          if (data.success && data.data) {
            downloadBase64File(data.data, data.filename);
            addLog('⬇️ Downloaded: ' + data.filename, 'success');
          } else {
            addLog('❌ Download failed', 'error');
          }
        }
        else if (data.type === 'ACTION_RESULT' && data.msg) {
          if (data.success !== false) {
            // Rút gọn message
            const shortMsg = data.msg.length > 50 ? data.msg.substring(0, 47) + '...' : data.msg;
            addLog('✅ ' + shortMsg, 'success');
            // Refresh directory after action
            loadDirectory(currentPath);
          } else {
            addLog('❌ ' + data.msg, 'error');
          }
        }
      } catch (e) {
        console.error('Error parsing WebSocket message:', e);
      }
    };

    ws.addEventListener('message', handleMessage);
    return () => ws.removeEventListener('message', handleMessage);
  }, [ws, currentPath]);

  const loadDirectory = (path) => {
    if (!ws || ws.readyState !== WebSocket.OPEN) {
      addLog('❌ Not connected to server', 'error');
      return;
    }
    
    setLoading(true);
    setSelectedItem(null);
    ws.send(JSON.stringify({
      cmd: 'LIST_DIR',
      path: path
    }));
  };

  const navigateToPath = (newPath) => {
    // Ẩn drive picker nếu đang hiển thị
    if (showDrivePicker) {
      setShowDrivePicker(false);
    }
    
    // Update history
    const newHistory = pathHistory.slice(0, historyIndex + 1);
    newHistory.push(newPath);
    setPathHistory(newHistory);
    setHistoryIndex(newHistory.length - 1);
    setCurrentPath(newPath);
  };

  const goBack = () => {
    if (historyIndex > 0) {
      const newIndex = historyIndex - 1;
      setHistoryIndex(newIndex);
      setCurrentPath(pathHistory[newIndex]);
    } else if (historyIndex === 0) {
      // Quay về drive picker
      setShowDrivePicker(true);
      setCurrentPath(null);
      setHistoryIndex(-1);
      setPathHistory([]);
      setItems([]);
    }
  };

  const goForward = () => {
    if (historyIndex < pathHistory.length - 1) {
      const newIndex = historyIndex + 1;
      setHistoryIndex(newIndex);
      setCurrentPath(pathHistory[newIndex]);
    }
  };

  const goUp = () => {
    const parts = currentPath.split('\\').filter(p => p);
    if (parts.length > 1) {
      const newPath = parts.slice(0, -1).join('\\') + '\\';
      navigateToPath(newPath);
    } else {
      // Đã ở root của ổ đĩa, quay về drive picker
      setShowDrivePicker(true);
      setCurrentPath(null);
      setHistoryIndex(-1);
      setPathHistory([]);
      setItems([]);
    }
  };

  const handleItemDoubleClick = (item) => {
    if (item.isDirectory) {
      navigateToPath(item.path);
    } else {
      // Open file for viewing/editing
      openFile(item);
    }
  };

  const handleItemClick = (item, event) => {
    event.stopPropagation();
    setSelectedItem(item);
  };

  const handleContextMenu = (event, item) => {
    event.preventDefault();
    event.stopPropagation();
    
    if (item) {
      setSelectedItem(item);
    }
    
    setContextMenu({
      visible: true,
      x: event.clientX,
      y: event.clientY,
      item: item
    });
  };

  const closeContextMenu = () => {
    setContextMenu({ visible: false, x: 0, y: 0 });
  };

  const openFile = (item) => {
    setEditingFile(item);
    setFileContent('');
    ws.send(JSON.stringify({
      cmd: 'READ_FILE',
      path: item.path
    }));
  };

  const saveFile = () => {
    if (!editingFile) return;
    
    ws.send(JSON.stringify({
      cmd: 'WRITE_FILE',
      path: editingFile.path,
      content: fileContent
    }));
    
    addLog('💾 Saving: ' + editingFile.name, 'info');
  };

  const closeEditor = () => {
    setIsEditorOpen(false);
    setEditingFile(null);
    setFileContent('');
  };

  const createNewFile = () => {
    setDialog({ type: 'file', visible: true, value: '' });
    closeContextMenu();
  };

  const createNewFolder = () => {
    setDialog({ type: 'folder', visible: true, value: '' });
    closeContextMenu();
  };

  const renameItem = () => {
    if (!selectedItem) return;
    setDialog({ type: 'rename', visible: true, value: selectedItem.name });
    closeContextMenu();
  };

  const deleteItem = () => {
    if (!selectedItem) return;
    
    showConfirm(
      'Delete Confirmation',
      `Are you sure you want to delete "${selectedItem.name}"?`,
      () => {
        ws.send(JSON.stringify({
          cmd: 'DELETE_ITEM',
          path: selectedItem.path
        }));
        addLog('🗑️ Deleting: ' + selectedItem.name, 'info');
      }
    );
    closeContextMenu();
  };

  const handleDialogSubmit = () => {
    const value = dialog.value.trim();
    if (!value) return;

    if (dialog.type === 'file') {
      const newPath = currentPath + (currentPath.endsWith('\\') ? '' : '\\') + value;
      ws.send(JSON.stringify({
        cmd: 'CREATE_FILE',
        path: newPath,
        content: ''
      }));
      addLog('📄+ ' + value, 'info');
    }
    else if (dialog.type === 'folder') {
      const newPath = currentPath + (currentPath.endsWith('\\') ? '' : '\\') + value;
      ws.send(JSON.stringify({
        cmd: 'CREATE_FOLDER',
        path: newPath
      }));
      addLog('📁+ ' + value, 'info');
    }
    else if (dialog.type === 'rename') {
      if (!selectedItem) return;
      const parentPath = selectedItem.path.substring(0, selectedItem.path.lastIndexOf('\\'));
      const newPath = parentPath + '\\' + value;
      ws.send(JSON.stringify({
        cmd: 'RENAME_ITEM',
        oldPath: selectedItem.path,
        newPath: newPath
      }));
      addLog('✏️ ' + selectedItem.name + ' → ' + value, 'info');
    }

    setDialog({ type: null, visible: false, value: '' });
  };

  const downloadFile = () => {
    if (!selectedItem || selectedItem.isDirectory) return;
    
    setDownloading(true);
    ws.send(JSON.stringify({
      cmd: 'DOWNLOAD_FILE',
      path: selectedItem.path
    }));
    addLog('⬇️ ' + selectedItem.name, 'info');
    closeContextMenu();
  };

  const downloadBase64File = (base64Data, filename) => {
    try {
      const byteCharacters = atob(base64Data);
      const byteNumbers = new Array(byteCharacters.length);
      for (let i = 0; i < byteCharacters.length; i++) {
        byteNumbers[i] = byteCharacters.charCodeAt(i);
      }
      const byteArray = new Uint8Array(byteNumbers);
      const blob = new Blob([byteArray]);
      
      const url = window.URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = filename;
      document.body.appendChild(a);
      a.click();
      window.URL.revokeObjectURL(url);
      document.body.removeChild(a);
    } catch (e) {
      addLog('❌ Error downloading file: ' + e.message, 'error');
    }
  };

  const uploadFile = () => {
    const input = document.createElement('input');
    input.type = 'file';
    input.onchange = (e) => {
      const file = e.target.files[0];
      if (!file) return;

      setUploading(true);
      const reader = new FileReader();
      reader.onload = (event) => {
        const base64Data = btoa(
          new Uint8Array(event.target.result)
            .reduce((data, byte) => data + String.fromCharCode(byte), '')
        );
        
        const uploadPath = currentPath + (currentPath.endsWith('\\') ? '' : '\\') + file.name;
        ws.send(JSON.stringify({
          cmd: 'UPLOAD_FILE',
          path: uploadPath,
          data: base64Data
        }));
        addLog('⬆️ ' + file.name, 'info');
        setUploading(false);
      };
      reader.readAsArrayBuffer(file);
    };
    input.click();
    closeContextMenu();
  };

  // Custom notification helpers
  const showConfirm = (title, message, onConfirm, onCancel = null) => {
    setNotification({
      visible: true,
      type: 'confirm',
      title,
      message,
      onConfirm,
      onCancel
    });
  };

  const showAlert = (title, message, onConfirm = null) => {
    setNotification({
      visible: true,
      type: 'alert',
      title,
      message,
      onConfirm,
      onCancel: null
    });
  };

  const closeNotification = () => {
    setNotification({
      visible: false,
      type: 'confirm',
      title: '',
      message: '',
      onConfirm: null,
      onCancel: null
    });
  };

  const handleNotificationConfirm = () => {
    if (notification.onConfirm) {
      notification.onConfirm();
    }
    closeNotification();
  };

  const handleNotificationCancel = () => {
    if (notification.onCancel) {
      notification.onCancel();
    }
    closeNotification();
  };

  const getFileIcon = (item) => {
    if (item.isDirectory) return '📁';
    
    const ext = item.extension?.toLowerCase();
    if (['.txt', '.log', '.md'].includes(ext)) return '📄';
    if (['.jpg', '.jpeg', '.png', '.gif', '.bmp'].includes(ext)) return '🖼️';
    if (['.mp4', '.avi', '.mkv', '.mov'].includes(ext)) return '🎬';
    if (['.mp3', '.wav', '.flac'].includes(ext)) return '🎵';
    if (['.exe', '.msi'].includes(ext)) return '⚙️';
    if (['.zip', '.rar', '.7z'].includes(ext)) return '📦';
    if (['.pdf'].includes(ext)) return '📕';
    if (['.doc', '.docx'].includes(ext)) return '📘';
    if (['.xls', '.xlsx'].includes(ext)) return '📊';
    if (['.ppt', '.pptx'].includes(ext)) return '📑';
    if (['.cpp', '.h', '.c', '.java', '.py', '.js', '.html', '.css'].includes(ext)) return '💻';
    
    return '📃';
  };

  // Close context menu when clicking outside
  useEffect(() => {
    const handleClick = () => closeContextMenu();
    document.addEventListener('click', handleClick);
    return () => document.removeEventListener('click', handleClick);
  }, []);

  return (
    <div className="file-manager">
      {/* Drive Picker Screen */}
      {showDrivePicker ? (
        <div className="fm-drive-picker">
          <div className="fm-drive-picker-header">
            <h2>📁 Select Drive</h2>
            <p>Choose a drive to browse</p>
          </div>
          <div className="fm-drive-grid">
            {drives.map(drive => (
              <div
                key={drive}
                className="fm-drive-card"
                onClick={() => navigateToPath(drive)}
              >
                <div className="fm-drive-icon">💾</div>
                <div className="fm-drive-label">{drive.replace('\\', '')}</div>
              </div>
            ))}
          </div>
        </div>
      ) : (
        <>
          {/* Toolbar */}
          <div className="fm-toolbar">
            <div className="fm-nav-buttons">
              <button 
                onClick={goBack} 
                disabled={historyIndex < 0}
                title="Back"
              >
                ◀
              </button>
              <button 
                onClick={goForward} 
                disabled={historyIndex >= pathHistory.length - 1}
                title="Forward"
              >
                ▶
              </button>
              <button onClick={goUp} title="Up">
                ⬆
              </button>
              <button onClick={() => loadDirectory(currentPath)} title="Refresh">
                🔄
              </button>
            </div>
            
            <div className="fm-drives">
              {drives.map(drive => (
                <button
                  key={drive}
                  onClick={() => navigateToPath(drive)}
                  className={`fm-drive-btn ${currentPath && currentPath.startsWith(drive) ? 'active' : ''}`}
                  title={`Go to ${drive}`}
                >
                  💾 {drive.replace('\\', '')}
                </button>
              ))}
            </div>
            
            <div className="fm-path-bar">
              <input 
                type="text" 
                value={currentPath || ''}
                onChange={(e) => setCurrentPath(e.target.value)}
                onKeyPress={(e) => {
                  if (e.key === 'Enter') {
                    navigateToPath(currentPath);
                  }
                }}
              />
            </div>
            
            <div className="fm-action-buttons">
              <button onClick={createNewFile} title="New File">📄+</button>
              <button onClick={createNewFolder} title="New Folder">📁+</button>
              <button onClick={uploadFile} title="Upload File" disabled={uploading}>
                {uploading ? '⏳' : '⬆️'}
              </button>
            </div>
          </div>

          {/* File List */}
          <div 
            className="fm-content"
            onContextMenu={(e) => handleContextMenu(e, null)}
          >
            {loading ? (
              <div className="fm-loading">Loading...</div>
        ) : items.length === 0 ? (
          <div className="fm-empty">Empty folder</div>
        ) : (
          <div className="fm-items">
            {items.map((item, index) => (
              <div
                key={index}
                className={`fm-item ${selectedItem?.path === item.path ? 'selected' : ''}`}
                onClick={(e) => handleItemClick(item, e)}
                onDoubleClick={() => handleItemDoubleClick(item)}
                onContextMenu={(e) => handleContextMenu(e, item)}
              >
                <div className="fm-item-icon">{getFileIcon(item)}</div>
                <div className="fm-item-info">
                  <div className="fm-item-name">{item.name}</div>
                  <div className="fm-item-details">
                    {item.sizeFormatted && <span>{item.sizeFormatted}</span>}
                    {item.modified && <span>{item.modified}</span>}
                  </div>
                </div>
              </div>
            ))}
          </div>
        )}
      </div>

      {/* Status Bar */}
      <div className="fm-statusbar">
        {selectedItem ? (
          <span>Selected: {selectedItem.name} {selectedItem.sizeFormatted && `(${selectedItem.sizeFormatted})`}</span>
        ) : (
          <span>{items.length} item(s)</span>
        )}
      </div>

      {/* Context Menu */}
      {contextMenu.visible && (
        <div 
          className="fm-context-menu"
          style={{ 
            left: contextMenu.x + 'px', 
            top: contextMenu.y + 'px' 
          }}
          onClick={(e) => e.stopPropagation()}
        >
          <div className="fm-menu-item" onClick={createNewFile}>📄 New File</div>
          <div className="fm-menu-item" onClick={createNewFolder}>📁 New Folder</div>
          {contextMenu.item && (
            <>
              <div className="fm-menu-divider"></div>
              {!contextMenu.item.isDirectory && (
                <div className="fm-menu-item" onClick={() => { openFile(contextMenu.item); closeContextMenu(); }}>
                  📖 Open
                </div>
              )}
              <div className="fm-menu-item" onClick={renameItem}>✏️ Rename</div>
              <div className="fm-menu-item" onClick={deleteItem}>🗑️ Delete</div>
              {!contextMenu.item.isDirectory && (
                <div className="fm-menu-item" onClick={downloadFile}>
                  ⬇️ Download
                </div>
              )}
            </>
          )}
          <div className="fm-menu-divider"></div>
          <div className="fm-menu-item" onClick={uploadFile}>⬆️ Upload File</div>
        </div>
      )}
        </>
      )}

      {/* Custom Notification */}
      {notification.visible && (
        <div className="fm-notification-overlay">
          <div className="fm-notification">
            <div className="fm-notification-header">
              <span className="fm-notification-icon">
                {notification.type === 'confirm' ? '❓' : 'ℹ️'}
              </span>
              <span className="fm-notification-title">{notification.title}</span>
            </div>
            <div className="fm-notification-message">
              {notification.message}
            </div>
            <div className="fm-notification-buttons">
              {notification.type === 'confirm' ? (
                <>
                  <button className="fm-btn-confirm" onClick={handleNotificationConfirm}>
                    Yes
                  </button>
                  <button className="fm-btn-cancel" onClick={handleNotificationCancel}>
                    No
                  </button>
                </>
              ) : (
                <button className="fm-btn-ok" onClick={handleNotificationConfirm}>
                  OK
                </button>
              )}
            </div>
          </div>
        </div>
      )}

      {/* Dialog */}
      {dialog.visible && (
        <div className="fm-dialog-overlay">
          <div className="fm-dialog">
            <div className="fm-dialog-title">
              {dialog.type === 'file' && 'Create New File'}
              {dialog.type === 'folder' && 'Create New Folder'}
              {dialog.type === 'rename' && 'Rename Item'}
            </div>
            <input
              type="text"
              value={dialog.value}
              onChange={(e) => setDialog({ ...dialog, value: e.target.value })}
              onKeyPress={(e) => {
                if (e.key === 'Enter') handleDialogSubmit();
              }}
              autoFocus
            />
            <div className="fm-dialog-buttons">
              <button onClick={handleDialogSubmit}>OK</button>
              <button onClick={() => setDialog({ type: null, visible: false, value: '' })}>
                Cancel
              </button>
            </div>
          </div>
        </div>
      )}

      {/* File Editor */}
      {isEditorOpen && editingFile && (
        <div className="fm-editor-overlay">
          <div className="fm-editor">
            <div className="fm-editor-header">
              <span>📝 {editingFile.name}</span>
              <button onClick={closeEditor}>✖</button>
            </div>
            <textarea
              className="fm-editor-content"
              value={fileContent}
              onChange={(e) => setFileContent(e.target.value)}
            />
            <div className="fm-editor-footer">
              <button onClick={saveFile}>💾 Save</button>
              <button onClick={closeEditor}>Cancel</button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default FileManager;
