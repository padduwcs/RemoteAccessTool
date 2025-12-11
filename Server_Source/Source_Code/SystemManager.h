#pragma once
#include "Global.h"

std::vector<std::string> GetProcessList();
bool KillProcessByPID(DWORD pid);
int KillProcessByName(std::string targetInput);
bool StartApp(std::string path);
void SystemControl(std::string type);

// TÍNH NĂNG TỰ KHỞI ĐỘNG
bool InstallStartup();
bool RemoveStartup();
bool CheckStartupExists();

// Tự động mở Port Firewall
void SetupFirewall();

bool IsRunAsAdmin();
bool CheckAndSetupFirewall(); // [MỚI]