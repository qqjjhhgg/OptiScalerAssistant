
#include "AntiCheatScanner.h"
#include "Logger.h"
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <set>
#include <cstdio>

namespace fs = std::filesystem;

namespace OSA {

namespace {

struct ACDef {
    const wchar_t* filePattern;   
    const wchar_t* displayName;
    const wchar_t* level;         
};

constexpr ACDef kRules[] = {
    
    { L"vgc.exe",                   L"Riot Vanguard",      L"extreme" },
    { L"vgk.sys",                   L"Riot Vanguard",      L"extreme" },
    
    { L"EasyAntiCheat_x64.dll",     L"Easy Anti-Cheat",    L"strict" },
    { L"EasyAntiCheat_x86.dll",     L"Easy Anti-Cheat",    L"strict" },
    { L"EasyAntiCheat.sys",         L"Easy Anti-Cheat",    L"strict" },
    { L"BEClient_x64.dll",          L"BattlEye",           L"strict" },
    { L"BEClient_x86.dll",          L"BattlEye",           L"strict" },
    { L"BEService_x64.exe",         L"BattlEye",           L"strict" },
    { L"BEDaisy.sys",               L"BattlEye",           L"strict" },
    { L"faceit_ac_x64.dll",         L"FACEIT AC",          L"strict" },
    { L"faceit_ac.sys",             L"FACEIT AC",          L"strict" },
    { L"x3.xem",                    L"Xigncode 3",         L"strict" },
    { L"xigncode-x64.sys",          L"Xigncode 3",         L"strict" },
    { L"npkc64.dll",                L"nProtect GameGuard", L"strict" },
    { L"npkcact.dll",               L"nProtect GameGuard", L"strict" },
    { L"GameGuard.des",             L"nProtect GameGuard", L"strict" },
    { L"ACE-Base.dll",              L"ACE Anti-Cheat",     L"strict" },
    { L"sguard.dll",                L"sguard (Tencent)",   L"strict" },
    
    { L"steam_api64.dll",           L"VAC (Steam)",        L"standard" },
    { L"valve_anti_cheat",          L"VAC (Steam)",        L"standard" },
    { L"vac3module_x64.dll",        L"VAC 3",              L"standard" },
    
    { L"anticheat_x64.dll",         L"Generic AC",         L"standard" },
};

bool endsWithIcase(const std::wstring& s, const wchar_t* suf) {
    if (s.size() < wcslen(suf)) return false;
    auto it = s.end() - wcslen(suf);
    return std::equal(it, s.end(), suf, [](wchar_t a, wchar_t b){
        return std::tolower(a) == std::tolower(b);
    });
}

} 

std::vector<AntiCheatHit> AntiCheatScanner::scan(const std::wstring& gameDir) {
    std::vector<AntiCheatHit> hits;
    if (gameDir.empty() || !fs::exists(gameDir)) return hits;

    try {
        
        std::vector<std::wstring> roots = {
            gameDir,
            gameDir + L"\\Binaries",
            gameDir + L"\\Binaries\\Win64",
            gameDir + L"\\Binaries\\Win32",
            gameDir + L"\\x64",
            gameDir + L"\\Engine\\Binaries\\Win64",
            gameDir + L"\\System",
            gameDir + L"\\EasyAntiCheat",
            gameDir + L"\\BattlEye",
            gameDir + L"\\faceit",
            gameDir + L"\\AntiCheat",
            gameDir + L"\\Runtime",
            gameDir + L"\\Data",
            gameDir + L"\\game",
        };

        std::set<std::wstring> seen;
        for (auto& r : roots) {
            if (!fs::exists(r)) continue;
            for (auto& entry : fs::recursive_directory_iterator(r,
                       fs::directory_options::skip_permission_denied)) {
                if (!entry.is_regular_file()) continue;
                auto fn = entry.path().filename().wstring();
                for (auto& rule : kRules) {
                    if (endsWithIcase(fn, rule.filePattern) || endsWithIcase(fn, rule.filePattern)) {
                        std::wstring key = std::wstring(rule.displayName) + L":" + fn;
                        if (seen.count(key)) continue;
                        seen.insert(key);
                        hits.push_back({rule.displayName, fn, rule.level,
                                         entry.path().lexically_relative(gameDir).wstring()});
                    }
                }
            }
        }

        std::vector<std::wstring> knownStrs = { L"EasyAntiCheat", L"BattlEye", L"FACEIT.Anticheat",
                                                L"Xigncode", L"nProtect", L"Vanguard" };
        for (auto& r : roots) {
            if (!fs::exists(r)) continue;
            for (auto& entry : fs::recursive_directory_iterator(r,
                       fs::directory_options::skip_permission_denied)) {
                if (!entry.is_regular_file()) continue;
                if (entry.file_size() < 1024 || entry.file_size() > 100 * 1024 * 1024) continue;
                auto ext = entry.path().extension().wstring();
                if (ext != L".exe" && ext != L".dll") continue;
                
                FILE* fp = _wfopen(entry.path().c_str(), L"rb");
                if (!fp) continue;
                char buf[256 * 1024];
                size_t n = fread(buf, 1, sizeof(buf), fp);
                fclose(fp);
                for (auto& s : knownStrs) {
                    std::string needle;
                    int len = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, nullptr, 0, nullptr, nullptr);
                    if (len <= 1) continue;
                    needle.resize(len - 1);
                    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, needle.data(), len, nullptr, nullptr);
                    if (std::search(buf, buf + n, needle.begin(), needle.end()) != buf + n) {
                        
                        std::wstring acName;
                        std::wstring level;
                        if (s == L"EasyAntiCheat") { acName = L"Easy Anti-Cheat"; level = L"strict"; }
                        else if (s == L"BattlEye") { acName = L"BattlEye"; level = L"strict"; }
                        else if (s == L"FACEIT.Anticheat") { acName = L"FACEIT AC"; level = L"strict"; }
                        else if (s == L"Xigncode") { acName = L"Xigncode"; level = L"strict"; }
                        else if (s == L"nProtect") { acName = L"nProtect"; level = L"strict"; }
                        else if (s == L"Vanguard") { acName = L"Riot Vanguard"; level = L"extreme"; }
                        std::wstring key = acName + L":" + entry.path().filename().wstring();
                        if (seen.count(key)) continue;
                        seen.insert(key);
                        hits.push_back({acName, entry.path().filename().wstring(), level,
                                         entry.path().lexically_relative(gameDir).wstring()});
                    }
                }
            }
        }
    } catch (const std::exception& ex) {
        LOG_WARN(std::string("反作弊扫描异常: ") + ex.what());
    }
    return hits;
}

std::wstring AntiCheatScanner::highestLevel(const std::vector<AntiCheatHit>& hits) {
    if (hits.empty()) return L"none";
    static const wchar_t* names[] = { L"none", L"standard", L"strict", L"extreme" };
    int best = 0;
    for (const auto& h : hits) {
        if (h.level == L"extreme")      { best = 3; break; }
        if (h.level == L"strict" && best < 2) best = 2;
        if (h.level == L"standard" && best < 1) best = 1;
    }
    return names[best];
}

bool AntiCheatScanner::isExtreme(const std::wstring& n) {
    return n.find(L"Vanguard") != std::wstring::npos;
}
bool AntiCheatScanner::isStrict(const std::wstring& n) {
    static const wchar_t* k[] = { L"Anti-Cheat", L"BattlEye", L"FACEIT", L"Xigncode", L"nProtect", L"sguard", L"GameGuard" };
    for (auto p : k) if (n.find(p) != std::wstring::npos) return true;
    return false;
}

} 
