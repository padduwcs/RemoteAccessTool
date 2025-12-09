#pragma once
#include "Global.h"

// Khởi động CMD session với quyền admin
bool StartCmdSession(bool showWindow);

// Gửi lệnh vào CMD session
void SendCmdCommand(const std::string& command);

// Đọc output từ CMD (chạy trong thread riêng)
void CmdOutputReaderThread();

// Dừng CMD session
void StopCmdSession();

// Chạy file .bat hoặc .exe với quyền admin
bool RunExecutableFile(const std::string& filePath, bool showWindow);

// Terminate process đang chạy
void TerminateCmdProcess();
