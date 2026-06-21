
#pragma once
#include <string>
#include <vector>

namespace OSA {

struct AntiCheatHit {
    std::wstring name;        
    std::wstring library;     
    std::wstring level;       
    std::wstring evidence;    
};

class AntiCheatScanner {
public:
    
    static std::vector<AntiCheatHit> scan(const std::wstring& gameDir);

    static std::wstring highestLevel(const std::vector<AntiCheatHit>& hits);

    static bool isExtreme(const std::wstring& name);  
    static bool isStrict(const std::wstring& name);   
};

} 
