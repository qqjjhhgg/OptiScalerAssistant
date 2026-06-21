
#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace OSA {

struct GpuInfo {
    std::wstring name;            
    std::wstring vendor;          
    std::wstring vendorId;        
    std::wstring deviceId;        
    uint64_t vramBytes = 0;       
    uint32_t vramMB = 0;          
    std::wstring driverVersion;   
    std::string tier;             
    bool isIntegrated = false;    
    bool isVirtual   = false;     
    bool isSelected  = false;     
    std::wstring reason;          
};

class GpuDetector {
public:
    
    static std::vector<GpuInfo> detectAll();

    static GpuInfo detectBest();

    static std::wstring toJson(const GpuInfo& g);

    static std::wstring readDriverVersion();
};

} 
