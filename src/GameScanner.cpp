
#include "GameScanner.h"
#include "Logger.h"
#include <filesystem>
#include <algorithm>
#include <cstdio>

namespace fs = std::filesystem;

namespace OSA {

namespace {

bool isLikelyGameExe(const fs::path& p) {
    auto fn = p.filename().wstring();
    std::wstring lower_fn = fn;
    std::transform(lower_fn.begin(), lower_fn.end(), lower_fn.begin(), ::towlower);
    
    static const wchar_t* excludes[] = {
        L"unins", L"crash", L"setup", L"install", L"update",
        L"vc_redist", L"dxsetup", L"ue4prereq", L"easyanticheat", L"battleye",
        L"dotnet", L"vcredist", L"launcher", L"uninst"
    };
    for (auto e : excludes) {
        if (lower_fn.find(e) != std::wstring::npos) return false;
    }
    auto ext = p.extension().wstring();
    return _wcsicmp(ext.c_str(), L".exe") == 0;
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

std::wstring readPeArch(const std::wstring& path) {
    FILE* fp = _wfopen(path.c_str(), L"rb");
    if (!fp) return L"";
    IMAGE_DOS_HEADER dos{};
    if (fread(&dos, sizeof(dos), 1, fp) != 1) { fclose(fp); return L""; }
    if (dos.e_magic != IMAGE_DOS_SIGNATURE) { fclose(fp); return L""; }
    if (fseek(fp, dos.e_lfanew, SEEK_SET) != 0) { fclose(fp); return L""; }
    DWORD sig = 0;
    if (fread(&sig, 4, 1, fp) != 1) { fclose(fp); return L""; }
    if (sig != IMAGE_NT_SIGNATURE) { fclose(fp); return L""; }
    IMAGE_FILE_HEADER fh{};
    if (fread(&fh, sizeof(fh), 1, fp) != 1) { fclose(fp); return L""; }
    fclose(fp);
    return (fh.Machine == IMAGE_FILE_MACHINE_AMD64) ? L"x64" : L"x86";
}

std::wstring detectRhi(const std::wstring& gameDir, const std::wstring& exePath) {
    bool ue4 = false, ue5 = false, vulkan = false, dx11 = false, dx12 = false, unity = false;
    try {
        for (auto& e : fs::directory_iterator(gameDir)) {
            if (!e.is_regular_file()) continue;
            auto fn = e.path().filename().wstring();
            std::wstring lfn = fn;
            std::transform(lfn.begin(), lfn.end(), lfn.begin(), ::towlower);
            if (lfn.find(L"ue5") != std::wstring::npos || lfn.find(L"unrealengine") != std::wstring::npos) ue5 = true;
            if (lfn.find(L"ue4") != std::wstring::npos) ue4 = true;
            if (lfn.find(L"vulkan") != std::wstring::npos) vulkan = true;
            if (lfn.find(L"unity") != std::wstring::npos) unity = true;
            if (lfn == L"d3d11.dll" || lfn == L"d3dcompiler_47.dll") dx11 = true;
            if (lfn == L"d3d12.dll" || lfn == L"d3d12core.dll") dx12 = true;
        }
    } catch (...) {}
    if (ue5 || ue4) return L"d3d12";
    if (dx12) return L"d3d12";
    if (unity) return L"d3d11";
    if (dx11) return L"d3d11";
    if (vulkan) return L"vulkan";
    return L"other";
}

} 

GameInfo GameScanner::scan(const std::wstring& dirOrExe) {
    GameInfo g;
    if (dirOrExe.empty()) return g;

    std::wstring dir = dirOrExe;
    std::wstring explicitExe;
    try {
        if (fs::is_regular_file(dirOrExe)) {
            explicitExe = dirOrExe;
            dir = fs::path(dirOrExe).parent_path().wstring();
        }
    } catch (...) {}

    g.gameDir = dir;
    if (!fs::exists(dir)) return g;

    std::vector<fs::path> exes;
    try {
        for (auto& e : fs::recursive_directory_iterator(dir,
                   fs::directory_options::skip_permission_denied)) {
            if (!e.is_regular_file()) continue;
            if (!isLikelyGameExe(e.path())) continue;
            exes.push_back(e.path());
        }
    } catch (const std::exception& ex) {
        LOG_WARN(std::string("扫描游戏目录异常: ") + ex.what());
    }

    if (exes.empty()) {
        LOG_WARN("未找到 game.exe");
        return g;
    }

    std::sort(exes.begin(), exes.end(), [&](const fs::path& a, const fs::path& b){
        bool ra = (a.parent_path() == fs::path(dir));
        bool rb = (b.parent_path() == fs::path(dir));
        if (ra != rb) return ra > rb;
        try {
            return fs::file_size(a) > fs::file_size(b);
        } catch (...) { return false; }
    });

    auto best = exes.front();
    
    if (!explicitExe.empty()) {
        try {
            fs::path e(explicitExe);
            if (fs::is_regular_file(e)) best = e;
        } catch (...) {}
    }
    g.exePath = best.wstring();
    g.exeName = best.filename().wstring();
    try { g.exeSize = fs::file_size(best); } catch (...) {}

    g.arch = readPeArch(best.wstring());
    g.gameType = detectRhi(dir, best.wstring());

    g.acHits = AntiCheatScanner::scan(dir);
    g.acLevel = AntiCheatScanner::highestLevel(g.acHits);
    
    std::wstring topName;
    auto lvlRank = [](const std::wstring& l) {
        if (l == L"extreme") return 3;
        if (l == L"strict") return 2;
        if (l == L"standard") return 1;
        return 0;
    };
    int top = 0;
    for (auto& h : g.acHits) {
        if (lvlRank(h.level) > top) { top = lvlRank(h.level); topName = h.name; }
    }
    g.antiCheat = topName.empty() ? L"无" : topName;

    return g;
}

std::wstring GameScanner::detectType(const std::wstring& exePath) {
    return detectRhi(fs::path(exePath).parent_path().wstring(), exePath);
}

std::wstring GameScanner::toJson(const GameInfo& g) {
    std::wstring json = L"{";
    json += L"\"gameDir\":\""    + escapeJsonW(g.gameDir) + L"\",";
    json += L"\"exeName\":\""    + escapeJsonW(g.exeName) + L"\",";
    json += L"\"exePath\":\""    + escapeJsonW(g.exePath) + L"\",";
    json += L"\"gameType\":\""   + escapeJsonW(g.gameType) + L"\",";
    json += L"\"arch\":\""       + escapeJsonW(g.arch) + L"\",";
    json += L"\"exeSize\":"      + std::to_wstring(g.exeSize) + L",";
    json += L"\"antiCheat\":\""  + escapeJsonW(g.antiCheat) + L"\",";
    json += L"\"acLevel\":\""    + escapeJsonW(g.acLevel) + L"\"";
    json += L"}";
    return json;
}

} 
