#pragma once
#include "Global.h"

std::vector<std::string> GetProcessList();
bool KillProcessByPID(DWORD pid);
int KillProcessByName(std::string targetInput);
bool StartApp(std::string path);
void SystemControl(std::string type);