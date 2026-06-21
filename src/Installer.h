
#pragma once
#include "ProfileAdvisor.h"
#include <string>

namespace OSA {

class Installer {
public:
    
    static bool optimize(const InstallOptions& opt, std::wstring& errOut);

    static bool uninstall(const std::wstring& gameDir, std::wstring& errOut);

    static bool launchGame(const std::wstring& exePath, std::wstring& errOut);

    static bool copyInjectionDll(const std::wstring& gameDir, const std::wstring& injectName);

    static bool safeRemove(const std::wstring& path);

    static bool isOurs(const std::wstring& path);
};

} 
