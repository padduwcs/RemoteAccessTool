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