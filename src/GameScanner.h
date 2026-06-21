
#pragma once
#include "AntiCheatScanner.h"
#include <string>
#include <vector>
#include <cstdint>

namespace OSA {

struct GameInfo {
    std::wstring gameDir;
    std::wstring exeName;       
    std::wstring exePath;       
    std::wstring gameType;      
    std::wstring arch;          
    uint64_t exeSize = 0;
    std::wstring antiCheat;     
    std::wstring acLevel;       
    std::vector<AntiCheatHit> acHits;
};

class GameScanner {
public:
    
    static GameInfo scan(const std::wstring& dir);

    static std::wstring detectType(const std::wstring& exePath);

    static std::wstring toJson(const GameInfo& g);
};

} 
