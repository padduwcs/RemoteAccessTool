#include "FileManager.h"
#include <Windows.h>
#include <iomanip>
#include <thread>
#include <chrono>

// Helper: Check if string is valid UTF-8
bool IsValidUTF8(const std::string& str) {
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(str.c_str());
    size_t len = str.length();
    
    for (size_t i = 0; i < len; ) {
        unsigned char c = bytes[i];
        
        // ASCII (0-127)
        if (c <= 0x7F) {
            i++;
            continue;
        }
        
        // Multi-byte sequence
        int extraBytes = 0;
        if ((c & 0xE0) == 0xC0) extraBytes = 1;      // 110xxxxx
        else if ((c & 0xF0) == 0xE0) extraBytes = 2; // 1110xxxx
        else if ((c & 0xF8) == 0xF0) extraBytes = 3; // 11110xxx
        else return false; // Invalid start byte
        
        // Check we have enough bytes
        if (i + extraBytes >= len) return false;
        
        // Check continuation bytes (10xxxxxx)
        for (int j = 1; j <= extraBytes; j++) {
            if ((bytes[i + j] & 0xC0) != 0x80) return false;
        }
        
        i += extraBytes + 1;
    }
    
    return true;
}

// Helper: Convert file time to string
std::string FileTimeToString(const std::filesystem::file_time_type& ftime) {
    try {
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now()
        );
        auto time = std::chrono::system_clock::to_time_t(sctp);
        
        std::tm tm_buf;
        localtime_s(&tm_buf, &time);
        
        std::ostringstream oss;
        oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }
    catch (...) {
        return "Unknown";
    }
}

// Helper: Format file size
std::string FormatFileSize(uintmax_t size) {
    const char* units[] = { "B", "KB", "MB", "GB", "TB" };
    int unit = 0;
    double dsize = static_cast<double>(size);
    
    while (dsize >= 1024.0 && unit < 4) {
        dsize /= 1024.0;
        unit++;
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << dsize << " " << units[unit];
    return oss.str();
}

// Lấy danh sách file/folder trong thư mục
std::string ListDirectory(const std::string& path) {
    json result;
    result["success"] = false;
    result["items"] = json::array();
    
    try {
        // Check if path exists
        std::error_code ec;
        if (!fs::exists(path, ec)) {
            result["error"] = ec ? ec.message() : "Path does not exist";
            return result.dump();
        }
        
        if (!fs::is_directory(path, ec)) {
            result["error"] = ec ? ec.message() : "Path is not a directory";
            return result.dump();
        }
        
        // Iterate with error handling
        for (const auto& entry : fs::directory_iterator(path, ec)) {
            if (ec) {
                result["warning"] = "Some items could not be accessed: " + ec.message();
                ec.clear();
                continue;
            }
            
            try {
                json item;
                item["name"] = entry.path().filename().string();
                item["path"] = entry.path().string();
                
                bool isDir = entry.is_directory(ec);
                if (ec) {
                    ec.clear();
                    continue;
                }
                
                item["isDirectory"] = isDir;
                
                if (!isDir) {
                    try {
                        auto sz = entry.file_size(ec);
                        if (!ec) {
                            item["size"] = sz;
                            item["sizeFormatted"] = FormatFileSize(sz);
                        } else {
                            item["size"] = 0;
                            item["sizeFormatted"] = "N/A";
                            ec.clear();
                        }
                    }
                    catch (...) {
                        item["size"] = 0;
                        item["sizeFormatted"] = "N/A";
                    }
                }
                else {
                    item["size"] = 0;
                    item["sizeFormatted"] = "";
                }
                
                try {
                    auto ftime = entry.last_write_time(ec);
                    if (!ec) {
                        item["modified"] = FileTimeToString(ftime);
                    } else {
                        item["modified"] = "Unknown";
                        ec.clear();
                    }
                }
                catch (...) {
                    item["modified"] = "Unknown";
                }
                
                // Get file extension
                if (!isDir) {
                    item["extension"] = entry.path().extension().string();
                }
                else {
                    item["extension"] = "";
                }
                
                result["items"].push_back(item);
            }
            catch (...) {
                // Skip problematic entries
                continue;
            }
        }
        
        result["success"] = true;
        result["path"] = path;
    }
    catch (const std::exception& e) {
        result["error"] = e.what();
    }
    
    return result.dump();
}

// Lấy cấu trúc thư mục dưới dạng tree (recursive, limit depth)
std::string GetDirectoryStructure(const std::string& path) {
    json result;
    result["success"] = false;
    
    try {
        if (!fs::exists(path)) {
            result["error"] = "Path does not exist";
            return result.dump();
        }
        
        std::function<json(const fs::path&, int)> buildTree;
        buildTree = [&](const fs::path& p, int depth) -> json {
            json node;
            node["name"] = p.filename().string();
            node["path"] = p.string();
            node["isDirectory"] = fs::is_directory(p);
            
            // Limit depth to prevent huge JSON
            if (fs::is_directory(p) && depth < 3) {
                node["children"] = json::array();
                try {
                    for (const auto& entry : fs::directory_iterator(p)) {
                        node["children"].push_back(buildTree(entry.path(), depth + 1));
                    }
                }
                catch (...) {
                    // Access denied or other error
                }
            }
            
            return node;
        };
        
        result["tree"] = buildTree(fs::path(path), 0);
        result["success"] = true;
    }
    catch (const std::exception& e) {
        result["error"] = e.what();
    }
    
    return result.dump();
}

// Tạo file mới
bool CreateNewFile(const std::string& path, const std::string& content) {
    try {
        // Create parent directories if needed
        fs::path filePath(path);
        if (filePath.has_parent_path()) {
            std::error_code ec;
            fs::create_directories(filePath.parent_path(), ec);
        }
        
        // Remove readonly if exists
        if (fs::exists(path)) {
            DWORD attrs = GetFileAttributesA(path.c_str());
            if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_READONLY)) {
                SetFileAttributesA(path.c_str(), attrs & ~FILE_ATTRIBUTE_READONLY);
            }
        }
        
        // Scope để đảm bảo file handle được release
        {
            // Create/overwrite file with binary mode
            std::ofstream file(path, std::ios::binary | std::ios::trunc);
            if (!file.is_open()) {
                return false;
            }
            
            // Write content
            if (!content.empty()) {
                file.write(content.c_str(), content.size());
            } else {
                // Write empty content to ensure file is created
                file.write("", 0);
            }
            
            // Explicit flush before close
            file.flush();
            
            // Check state before closing
            if (file.fail()) {
                file.close();
                return false;
            }
            
            file.close();
            
            // Verify close was successful
            if (file.fail()) {
                return false;
            }
        } // File handle released here
        
        // Small delay to ensure OS completes write
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        // Verify file exists
        if (!fs::exists(path)) {
            return false;
        }
        
        // Ensure file is writable and set normal attributes
        DWORD attrs = GetFileAttributesA(path.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES) {
            SetFileAttributesA(path.c_str(), FILE_ATTRIBUTE_NORMAL);
        }
        
        // Final verification - try to open for read
        std::ifstream testRead(path, std::ios::binary);
        bool canRead = testRead.is_open();
        if (canRead) {
            testRead.close();
        }
        
        return canRead;
    }
    catch (...) {
        return false;
    }
}

// Tạo thư mục mới
bool CreateNewFolder(const std::string& path) {
    try {
        return fs::create_directories(path);
    }
    catch (...) {
        return false;
    }
}

// Xóa file hoặc folder
bool DeleteFileOrFolder(const std::string& path) {
    try {
        if (!fs::exists(path)) {
            return false;
        }
        
        if (fs::is_directory(path)) {
            return fs::remove_all(path) > 0;
        }
        else {
            return fs::remove(path);
        }
    }
    catch (...) {
        return false;
    }
}

// Rename/Move file hoặc folder
bool RenameFileOrFolder(const std::string& oldPath, const std::string& newPath) {
    try {
        if (!fs::exists(oldPath)) {
            return false;
        }
        
        fs::rename(oldPath, newPath);
        return true;
    }
    catch (...) {
        return false;
    }
}

// Đọc nội dung file dưới dạng text
std::string ReadFileContent(const std::string& path) {
    try {
        std::error_code ec;
        if (!fs::exists(path, ec) || ec) {
            return "";
        }
        
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            return "";
        }
        
        // Read entire file
        std::ostringstream oss;
        oss << file.rdbuf();
        file.close();
        
        return oss.str();
    }
    catch (...) {
        return "";
    }
}

// Ghi nội dung vào file
bool WriteFileContent(const std::string& path, const std::string& content) {
    try {
        // Create parent directories if needed
        fs::path filePath(path);
        if (filePath.has_parent_path()) {
            std::error_code ec;
            fs::create_directories(filePath.parent_path(), ec);
        }
        
        // Remove readonly attribute if exists
        if (fs::exists(path)) {
            DWORD attrs = GetFileAttributesA(path.c_str());
            if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_READONLY)) {
                SetFileAttributesA(path.c_str(), attrs & ~FILE_ATTRIBUTE_READONLY);
            }
        }
        
        // Scope để đảm bảo file handle được release
        {
            std::ofstream file(path, std::ios::binary | std::ios::trunc);
            if (!file.is_open()) {
                return false;
            }
            
            // Write using write() instead of operator<<
            file.write(content.c_str(), content.size());
            
            // Explicit flush before close
            file.flush();
            
            // Check state
            if (file.fail()) {
                file.close();
                return false;
            }
            
            file.close();
            
            if (file.fail()) {
                return false;
            }
        } // File handle released here
        
        // Small delay to ensure OS completes write
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        // Verify file exists
        if (!fs::exists(path)) {
            return false;
        }
        
        // Ensure file is readable/writable
        DWORD attrs = GetFileAttributesA(path.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES) {
            SetFileAttributesA(path.c_str(), FILE_ATTRIBUTE_NORMAL);
        }
        
        // Final verification - try to read it back
        std::ifstream testRead(path, std::ios::binary);
        bool canRead = testRead.is_open();
        if (canRead) {
            testRead.close();
        }
        
        return canRead;
    }
    catch (...) {
        return false;
    }
}

// Đọc file dưới dạng Base64 (để download)
std::string ReadFileAsBase64(const std::string& path) {
    try {
        std::error_code ec;
        if (!fs::exists(path, ec) || ec) {
            return "";
        }
        
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            return "";
        }
        
        std::vector<unsigned char> buffer((std::istreambuf_iterator<char>(file)),
                                          std::istreambuf_iterator<char>());
        file.close();
        
        if (buffer.empty()) {
            return "";
        }
        
        // Sử dụng Base64Encode từ Global.h
        return Base64Encode(buffer.data(), static_cast<unsigned int>(buffer.size()));
    }
    catch (...) {
        return "";
    }
}

// Ghi file từ Base64 (để upload)
bool WriteFileFromBase64(const std::string& path, const std::string& base64Data) {
    try {
        if (base64Data.empty()) {
            return false;
        }
        
        // Sử dụng Base64Decode từ Global.h (trả về std::string)
        std::string decoded = Base64Decode(base64Data);
        
        if (decoded.empty()) {
            return false;
        }
        
        // Create parent directories if needed
        fs::path filePath(path);
        if (filePath.has_parent_path()) {
            std::error_code ec;
            fs::create_directories(filePath.parent_path(), ec);
        }
        
        // Remove readonly attribute if exists
        if (fs::exists(path)) {
            DWORD attrs = GetFileAttributesA(path.c_str());
            if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_READONLY)) {
                SetFileAttributesA(path.c_str(), attrs & ~FILE_ATTRIBUTE_READONLY);
            }
        }
        
        // Scope để đảm bảo file handle được release
        {
            std::ofstream file(path, std::ios::binary | std::ios::trunc);
            if (!file.is_open()) {
                return false;
            }
            
            file.write(decoded.c_str(), decoded.size());
            
            // Explicit flush before close
            file.flush();
            
            // Check state
            if (file.fail()) {
                file.close();
                return false;
            }
            
            file.close();
            
            if (file.fail()) {
                return false;
            }
        } // File handle released here
        
        // Small delay to ensure OS completes write
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        // Verify file exists
        if (!fs::exists(path)) {
            return false;
        }
        
        // Ensure file is readable/writable
        DWORD attrs = GetFileAttributesA(path.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES) {
            SetFileAttributesA(path.c_str(), FILE_ATTRIBUTE_NORMAL);
        }
        
        // Final verification - try to read it back
        std::ifstream testRead(path, std::ios::binary);
        bool canRead = testRead.is_open();
        if (canRead) {
            testRead.close();
        }
        
        return canRead;
    }
    catch (...) {
        return false;
    }
}

// Lấy thông tin file (size, date, etc.)
std::string GetFileInfo(const std::string& path) {
    json result;
    result["success"] = false;
    
    try {
        if (!fs::exists(path)) {
            result["error"] = "File does not exist";
            return result.dump();
        }
        
        result["name"] = fs::path(path).filename().string();
        result["path"] = path;
        result["isDirectory"] = fs::is_directory(path);
        
        if (!fs::is_directory(path)) {
            result["size"] = fs::file_size(path);
            result["sizeFormatted"] = FormatFileSize(fs::file_size(path));
            result["extension"] = fs::path(path).extension().string();
        }
        
        auto ftime = fs::last_write_time(path);
        result["modified"] = FileTimeToString(ftime);
        
        // Get file attributes
        DWORD attrs = GetFileAttributesA(path.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES) {
            result["readonly"] = (attrs & FILE_ATTRIBUTE_READONLY) != 0;
            result["hidden"] = (attrs & FILE_ATTRIBUTE_HIDDEN) != 0;
            result["system"] = (attrs & FILE_ATTRIBUTE_SYSTEM) != 0;
        }
        
        result["success"] = true;
    }
    catch (const std::exception& e) {
        result["error"] = e.what();
    }
    
    return result.dump();
}
