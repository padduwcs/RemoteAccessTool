#include "CmdTerminal.h"
#include <algorithm>

// Khởi động CMD session với quyền admin
bool StartCmdSession(bool showWindow) {
    if (g_CmdRunning) {
        return false; // Session đã chạy rồi
    }

    g_CmdShowWindow = showWindow;

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE hStdOutRead, hStdOutWrite;
    HANDLE hStdInRead, hStdInWrite;

    // Tạo pipe cho stdout
    if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0)) {
        return false;
    }
    SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);

    // Tạo pipe cho stdin
    if (!CreatePipe(&hStdInRead, &hStdInWrite, &sa, 0)) {
        CloseHandle(hStdOutRead);
        CloseHandle(hStdOutWrite);
        return false;
    }
    SetHandleInformation(hStdInWrite, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdError = hStdOutWrite;
    si.hStdOutput = hStdOutWrite;
    si.hStdInput = hStdInRead;
    si.dwFlags |= STARTF_USESTDHANDLES;

    if (!showWindow) {
        si.dwFlags |= STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
    }

    ZeroMemory(&pi, sizeof(pi));

    // Khởi động cmd.exe
    char cmdLine[] = "cmd.exe";
    BOOL success = CreateProcessA(
        NULL,
        cmdLine,
        NULL,
        NULL,
        TRUE,
        CREATE_NEW_CONSOLE,
        NULL,
        NULL,
        &si,
        &pi
    );

    CloseHandle(hStdOutWrite);
    CloseHandle(hStdInRead);

    if (!success) {
        CloseHandle(hStdOutRead);
        CloseHandle(hStdInWrite);
        return false;
    }

    g_CmdProcess = pi.hProcess;
    g_CmdStdInWrite = hStdInWrite;
    g_CmdStdOutRead = hStdOutRead;
    g_CmdRunning = true;

    CloseHandle(pi.hThread);

    // Tạo thread để đọc output
    g_CmdReaderThread = new std::thread(CmdOutputReaderThread);
    g_CmdReaderThread->detach();

    return true;
}

// Gửi lệnh vào CMD session
void SendCmdCommand(const std::string& command) {
    if (!g_CmdRunning || g_CmdStdInWrite == NULL) {
        return;
    }

    std::string cmd = command + "\r\n";
    DWORD written;
    WriteFile(g_CmdStdInWrite, cmd.c_str(), (DWORD)cmd.length(), &written, NULL);
}

// Đọc output từ CMD (chạy trong thread riêng)
void CmdOutputReaderThread() {
    char buffer[4096];
    DWORD bytesRead;

    while (g_CmdRunning) {
        if (ReadFile(g_CmdStdOutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            
            // Gửi output realtime về client
            if (g_ClientConnected && g_ServerPtr != nullptr) {
                try {
                    json j_output;
                    j_output["type"] = "CMD_OUTPUT";
                    j_output["data"] = std::string(buffer, bytesRead);
                    g_ServerPtr->send(g_ClientHdl, j_output.dump(), websocketpp::frame::opcode::text);
                }
                catch (...) {
                    // Bỏ qua lỗi gửi
                }
            }
        }
        else {
            // Process đã tắt hoặc lỗi
            break;
        }
    }

    g_CmdRunning = false;
}

// Dừng CMD session
void StopCmdSession() {
    if (!g_CmdRunning) {
        return;
    }

    g_CmdRunning = false;

    // Gửi lệnh exit
    if (g_CmdStdInWrite != NULL) {
        std::string exitCmd = "exit\r\n";
        DWORD written;
        WriteFile(g_CmdStdInWrite, exitCmd.c_str(), (DWORD)exitCmd.length(), &written, NULL);
    }

    // Đợi 1 giây để process tự tắt
    if (g_CmdProcess != NULL) {
        WaitForSingleObject(g_CmdProcess, 1000);
        
        // Nếu vẫn chưa tắt thì force kill
        DWORD exitCode;
        if (GetExitCodeProcess(g_CmdProcess, &exitCode) && exitCode == STILL_ACTIVE) {
            TerminateProcess(g_CmdProcess, 0);
        }
        
        CloseHandle(g_CmdProcess);
        g_CmdProcess = NULL;
    }

    if (g_CmdStdInWrite != NULL) {
        CloseHandle(g_CmdStdInWrite);
        g_CmdStdInWrite = NULL;
    }

    if (g_CmdStdOutRead != NULL) {
        CloseHandle(g_CmdStdOutRead);
        g_CmdStdOutRead = NULL;
    }
}

// Chạy file .bat hoặc .exe với tùy chọn quyền admin
bool RunExecutableFile(const std::string& filePath, bool showWindow) {
    SHELLEXECUTEINFOA sei = { sizeof(sei) };
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    
    // Chỉ dùng "runas" cho .exe, còn .bat/.cmd/.vbs chạy bình thường
    std::string fileExt = filePath.substr(filePath.find_last_of("."));
    std::transform(fileExt.begin(), fileExt.end(), fileExt.begin(), ::tolower);
    
    if (fileExt == ".exe") {
        sei.lpVerb = "runas"; // Chạy với quyền admin
    } else {
        sei.lpVerb = "open";  // Chạy bình thường (có thể hiện UI)
    }
    
    sei.lpFile = filePath.c_str();
    sei.nShow = showWindow ? SW_SHOW : SW_HIDE;

    if (!ShellExecuteExA(&sei)) {
        return false;
    }

    if (sei.hProcess != NULL) {
        // Lưu process handle để có thể terminate sau
        g_CmdProcess = sei.hProcess;
        g_CmdRunning = true;
        
        // Tạo thread để monitor process
        std::thread([=]() {
            WaitForSingleObject(sei.hProcess, INFINITE);
            
            DWORD exitCode;
            GetExitCodeProcess(sei.hProcess, &exitCode);
            
            // Gửi thông báo process đã kết thúc
            if (g_ClientConnected && g_ServerPtr != nullptr) {
                try {
                    json j_result;
                    j_result["type"] = "CMD_PROCESS_ENDED";
                    j_result["exitCode"] = exitCode;
                    g_ServerPtr->send(g_ClientHdl, j_result.dump(), websocketpp::frame::opcode::text);
                }
                catch (...) {}
            }
            
            CloseHandle(sei.hProcess);
            g_CmdProcess = NULL;
            g_CmdRunning = false;
        }).detach();
        
        return true;
    }

    return false;
}

// Terminate process đang chạy
void TerminateCmdProcess() {
    if (g_CmdProcess != NULL) {
        TerminateProcess(g_CmdProcess, 1);
        CloseHandle(g_CmdProcess);
        g_CmdProcess = NULL;
    }
    g_CmdRunning = false;
}
