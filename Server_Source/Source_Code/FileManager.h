#pragma once
#include "Global.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

// Helper function to check UTF-8 validity
bool IsValidUTF8(const std::string& str);

// Lấy cấu trúc thư mục dưới dạng JSON
std::string GetDirectoryStructure(const std::string& path);

// Lấy danh sách file/folder trong thư mục
std::string ListDirectory(const std::string& path);

// Tạo file mới
bool CreateNewFile(const std::string& path, const std::string& content = "");

// Xóa file hoặc folder
bool DeleteFileOrFolder(const std::string& path);

// Rename/Move file hoặc folder
bool RenameFileOrFolder(const std::string& oldPath, const std::string& newPath);

// Đọc nội dung file dưới dạng text
std::string ReadFileContent(const std::string& path);

// Ghi nội dung vào file
bool WriteFileContent(const std::string& path, const std::string& content);

// Đọc file dưới dạng Base64 (để download)
std::string ReadFileAsBase64(const std::string& path);

// Ghi file từ Base64 (để upload)
bool WriteFileFromBase64(const std::string& path, const std::string& base64Data);

// Tạo thư mục mới
bool CreateNewFolder(const std::string& path);

// Lấy thông tin file (size, date, etc.)
std::string GetFileInfo(const std::string& path);
