
#pragma once
#include "GpuDetector.h"
#include "GameScanner.h"
#include <string>

namespace OSA {

struct InstallOptions {
    std::wstring gamePath;
    std::wstring resolution;     
    std::wstring injection;     
    std::wstring upscaler;      
    bool fg = true;
    bool sharpen = true;
    std::wstring hotkeyMenu = L"Insert";
    GpuInfo gpu;
    GameInfo game;
};

struct ProfileDecision {
    
    std::wstring injectDllName;  
    
    std::wstring upscalerFinal;  
    
    std::wstring qualityPreset;  
    
    bool enableFG = false;
    
    float sharpness = 0.5f;
    
    std::wstring dlssPreset;     
    
    std::string iniContent;      
    
    std::vector<std::wstring> reasons;
};

class ProfileAdvisor {
public:
    static ProfileDecision advise(const InstallOptions& opt);
    static std::wstring toJson(const ProfileDecision& d);
};

} 
