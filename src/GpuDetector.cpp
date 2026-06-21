
#include "GpuDetector.h"
#include "Logger.h"
#include <dxgi1_4.h>
#include <stringapiset.h>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cctype>

#pragma comment(lib, "dxgi.lib")

namespace OSA {

namespace {

std::wstring lower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](wchar_t c){ return (wchar_t)towlower(c); });
    return s;
}

std::string w2s(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s; s.resize(n);
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}

bool containsAny(const std::wstring& s, std::initializer_list<const wchar_t*> subs) {
    auto ls = lower(s);
    for (auto p : subs) {
        std::wstring lp = lower(p);
        if (ls.find(lp) != std::wstring::npos) return true;
    }
    return false;
}

const wchar_t* vendorFromId(uint32_t vid) {
    switch (vid) {
    case 0x10DE: return L"NVIDIA";
    case 0x1002: return L"AMD";
    case 0x8086: return L"Intel";
    case 0x15AD: return L"VMware";
    case 0x1AB8: return L"Parallels";
    case 0x1234: return L"Bochs";
    default:     return L"Unknown";
    }
}

bool isIntegratedIntel(const GpuInfo& g) {
    if (g.vendorId != L"0x8086") return false;
    if (g.isVirtual) return false;
    return containsAny(g.name, {L"UHD", L"Iris", L"HD Graphics", L"Arc", L"Data Center GPU"});
}

bool isIntegratedAmd(const GpuInfo& g) {
    if (g.vendorId != L"0x1002") return false;
    return containsAny(g.name, {L"Vega", L"Radeon(TM) Graphics", L"Renoir", L"Cezanne", L"Phoenix"});
}

bool isIntegrated(const GpuInfo& g) {
    if (g.vendor == L"Intel")  return isIntegratedIntel(g);
    if (g.vendor == L"AMD")    return isIntegratedAmd(g);
    if (g.vendor == L"NVIDIA") return false;
    return false;
}

bool looksVirtual(const GpuInfo& g) {
    if (containsAny(g.name, {
        L"Virtual", L"Hyper-V", L"RemoteFX", L"Basic Render",
        L"VMware", L"Parallels", L"QEMU", L"VirtualBox",
        L"Microsoft Basic", L"Software Adapter", L"Software Renderer",
        L"Sunlogin", L"\u5411\u65e5\u83ca", L"ToDesk", L"Parsec",
        L"\u7f51\u6613\u4e91\u6e38\u620f",
        L"Shadow", L"GeForce NOW", L"Stadia", L"Luna",
        L"Citrix", L"RDP", L"XDisplay", L"Splashtop"
    })) return true;
    if (g.vendorId == L"0x15AD" || g.vendorId == L"0x1AB8") return true;
    return false;
}

int computeTier(const GpuInfo& g) {
    if (g.isVirtual) return 0;
    auto n = lower(g.name);
    auto vramGB = g.vramMB / 1024.0;

    if (g.vendor == L"NVIDIA") {
        if (containsAny(n, {L"rtx 40", L"rtx 50", L"rtx 3090", L"rtx 3080"})) {
            return vramGB >= 16 ? 4 : 3;
        }
        if (containsAny(n, {L"rtx 4070 ti", L"rtx 4080", L"rtx 4090", L"rtx 5070", L"rtx 5080", L"rtx 5090"}))
            return 4;
        if (containsAny(n, {L"rtx 4070", L"rtx 4060 ti", L"rtx 3070", L"rtx 3060 ti", L"rtx a", L"rtx 4000", L"rtx 5000", L"rtx 6000"}))
            return vramGB >= 10 ? 3 : 2;
        if (containsAny(n, {L"rtx 4060", L"rtx 4050", L"rtx 3060", L"rtx 3050", L"gtx 16", L"gtx 1080", L"gtx 1070"}))
            return vramGB >= 8 ? 2 : 1;
    }
    if (g.vendor == L"AMD") {
        if (containsAny(n, {L"rx 7900", L"rx 7800", L"rx 7700", L"rx 6900", L"rx 6800", L"rx 6700"}))
            return vramGB >= 12 ? 3 : 2;
        if (containsAny(n, {L"rx 7600", L"rx 6600", L"rx 6500", L"rx 5700", L"rx 5600", L"rx 5500"}))
            return vramGB >= 8 ? 2 : 1;
    }
    if (g.vendor == L"Intel") {
        if (containsAny(n, {L"arc a7", L"arc a5", L"arc b5", L"arc b7"}))
            return vramGB >= 8 ? 2 : 1;
    }
    if (vramGB >= 16) return 3;
    if (vramGB >= 8)  return 2;
    if (vramGB >= 4)  return 1;
    return 0;
}

const char* tierName(int t) {
    switch (t) {
    case 4: return "ultra";
    case 3: return "ultra";
    case 2: return "high";
    case 1: return "mid";
    default: return "low";
    }
}

std::wstring escapeJsonW(const std::wstring& s) {
    std::wstring out;
    out.reserve(s.size() + 8);
    for (auto c : s) {
        switch (c) {
            case L'\\': out.append(L"\\\\"); break;
            case L'"':  out.append(L"\\\""); break;
            case L'\n': out.append(L"\\n");  break;
            case L'\r': out.append(L"\\r");  break;
            case L'\t': out.append(L"\\t");  break;
            case L'\b': out.append(L"\\b");  break;
            case L'\f': out.append(L"\\f");  break;
            default:
                if (c < 0x20) {
                    wchar_t buf[8];
                    swprintf_s(buf, L"\\u%04x", c);
                    out.append(buf);
                } else {
                    out.push_back(c);
                }
        }
    }
    return out;
}

} 

std::wstring GpuDetector::readDriverVersion() {
    HKEY h = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                      0, KEY_READ, &h) != ERROR_SUCCESS) return L"";
    WCHAR buf[128] = {};
    DWORD sz = sizeof(buf);
    DWORD type = 0;
    LSTATUS r = RegQueryValueExW(h, L"CurrentBuildNumber", nullptr, &type, (LPBYTE)buf, &sz);
    RegCloseKey(h);
    (void)r;
    return buf;
}

std::vector<GpuInfo> GpuDetector::detectAll() {
    std::vector<GpuInfo> out;
    IDXGIFactory1* factory = nullptr;
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        LOG_ERROR("CreateDXGIFactory1 失败");
        return out;
    }

    auto driver = readDriverVersion();

    for (UINT i = 0; ; ++i) {
        IDXGIAdapter1* adapter = nullptr;
        hr = factory->EnumAdapters1(i, &adapter);
        if (hr == DXGI_ERROR_NOT_FOUND) break;
        if (FAILED(hr)) continue;

        DXGI_ADAPTER_DESC1 desc{};
        if (FAILED(adapter->GetDesc1(&desc))) {
            adapter->Release();
            continue;
        }

        GpuInfo g;
        g.name = desc.Description;
        char vidBuf[8]; snprintf(vidBuf, sizeof(vidBuf), "0x%04X", desc.VendorId);
        char didBuf[8]; snprintf(didBuf, sizeof(didBuf), "0x%04X", desc.DeviceId);
        g.vendorId = std::wstring(vidBuf, vidBuf+strlen(vidBuf));
        g.deviceId = std::wstring(didBuf, didBuf+strlen(didBuf));
        g.vendor = vendorFromId(desc.VendorId);
        uint64_t mem = desc.DedicatedVideoMemory ? desc.DedicatedVideoMemory
                                                  : desc.DedicatedSystemMemory;
        g.vramBytes = mem;
        g.vramMB = (uint32_t)(mem / (1024 * 1024));
        g.driverVersion = driver;
        g.isVirtual = looksVirtual(g);
        g.isIntegrated = !g.isVirtual && isIntegrated(g);

        out.push_back(std::move(g));
        adapter->Release();
    }
    factory->Release();

    for (auto& g : out) {
        g.tier = g.isVirtual ? std::string("low") : tierName(computeTier(g));
    }

    auto score = [](const GpuInfo& g) -> int {
        if (g.isVirtual) return -1000;
        int s = 0;
        if (g.vendor == L"NVIDIA") s += 1000;
        if (g.vendor == L"AMD")    s += 800;
        if (g.vendor == L"Intel" && !g.isIntegrated) s += 500;
        if (g.isIntegrated)        s -= 500;
        if (g.tier == "ultra") s += 100;
        else if (g.tier == "high") s += 60;
        else if (g.tier == "mid")  s += 30;
        else if (g.tier == "low")  s += 10;
        s += (int)(g.vramMB / 256);
        return s;
    };

    std::vector<GpuInfo> real;
    for (auto& g : out) {
        if (g.isVirtual) {
            g.reason = L"\u865a\u62df GPU\uff0c\u5df2\u6392\u9664";
            continue;
        }
        real.push_back(g);
    }

    if (real.empty()) {
        if (!out.empty()) {
            out[0].isSelected = true;
            out[0].reason = L"\u672a\u627e\u5230\u72ec\u663e\uff08\u4ec5\u865a\u62df GPU\uff09";
        }
        return out;
    }

    auto best = std::max_element(real.begin(), real.end(),
                                 [&](const GpuInfo& a, const GpuInfo& b){ return score(a) < score(b); });
    if (best != real.end()) {
        for (auto& g : out) {
            if (g.name == best->name && g.vendorId == best->vendorId) {
                g.isSelected = true;
                if (g.isIntegrated) g.reason = L"\u672a\u68c0\u6d4b\u5230\u72ec\u663e\uff0c\u5df2\u9009\u62e9\u6700\u5f3a\u6838\u663e";
                else g.reason = L"\u6309\u5206\u6570\u9009\u62e9\uff08\u5382\u5546+\u5206\u7ea7+\u663e\u5b58\uff09";
                break;
            }
        }
    }
    return out;
}

GpuInfo GpuDetector::detectBest() {
    auto all = detectAll();
    for (auto& g : all) if (g.isSelected) return g;
    return all.empty() ? GpuInfo{} : all.front();
}

std::wstring GpuDetector::toJson(const GpuInfo& g) {
    std::wstring json = L"{";
    json += L"\"name\":\""          + escapeJsonW(g.name) + L"\",";
    json += L"\"vendor\":\""        + escapeJsonW(g.vendor) + L"\",";
    json += L"\"vendorId\":\""      + escapeJsonW(g.vendorId) + L"\",";
    json += L"\"deviceId\":\""      + escapeJsonW(g.deviceId) + L"\",";
    json += L"\"vramBytes\":"      + std::to_wstring(g.vramBytes) + L",";
    json += L"\"vramMB\":"          + std::to_wstring(g.vramMB) + L",";
    json += L"\"driverVersion\":\"" + escapeJsonW(g.driverVersion) + L"\",";
    auto tierW = std::wstring(g.tier.begin(), g.tier.end());
    json += L"\"tier\":\""          + escapeJsonW(tierW) + L"\",";
    json += L"\"isIntegrated\":"   + std::wstring(g.isIntegrated ? L"true" : L"false") + L",";
    json += L"\"isVirtual\":"       + std::wstring(g.isVirtual ? L"true" : L"false") + L",";
    json += L"\"isSelected\":"      + std::wstring(g.isSelected ? L"true" : L"false") + L",";
    json += L"\"reason\":\""        + escapeJsonW(g.reason) + L"\"";
    json += L"}";
    return json;
}

} 
