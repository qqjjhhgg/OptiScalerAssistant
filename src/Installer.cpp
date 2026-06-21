
#include "Installer.h"
#include "ResourceExtractor.h"
#include "Logger.h"
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <windows.h>
#include <shlobj.h>
#include <sddl.h>

namespace fs = std::filesystem;

namespace OSA {

namespace {

constexpr const char* kOSAMarker = "OSA1.0_OPTISCALER_INSTALLED";
void markOurs(const std::wstring& path) {
    if (!fs::exists(path)) return;
    HANDLE h = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    char tag[64] = {};
    int n = snprintf(tag, sizeof(tag), "\n;%s\n", kOSAMarker);
    DWORD w;
    WriteFile(h, tag, (DWORD)n, &w, nullptr);
    CloseHandle(h);
}

bool hasMarker(const std::wstring& path) {
    if (!fs::exists(path)) return false;
    auto sz = fs::file_size(path);
    if (sz < 64) return false;
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER li; li.QuadPart = sz - 64;
    SetFilePointerEx(h, li, nullptr, FILE_BEGIN);
    char buf[64] = {};
    DWORD r;
    ReadFile(h, buf, 64, &r, nullptr);
    CloseHandle(h);
    return strstr(buf, kOSAMarker) != nullptr;
}

void backupOriginal(const std::wstring& path) {
    if (!fs::exists(path)) return;
    if (hasMarker(path)) return;  
    auto bak = path + L".osa_bak";
    if (fs::exists(bak)) return;
    try {
        fs::copy_file(path, bak, fs::copy_options::overwrite_existing);
        LOG_INFO(std::string("已备份: ") + w2s(path));
    } catch (...) {}
}

void rollbackIfBackedUp(const std::wstring& path) {
    if (!fs::exists(path)) return;
    auto bak = path + L".osa_bak";
    if (fs::exists(bak)) {
        try {
            fs::remove(path);
            fs::rename(bak, path);
            LOG_INFO(std::string("已恢复备份: ") + w2s(path));
        } catch (...) {}
    } else {
        
        try { fs::remove(path); } catch (...) {}
    }
}

} 

bool Installer::isOurs(const std::wstring& path) {
    return hasMarker(path);
}

bool Installer::safeRemove(const std::wstring& path) {
    try {
        if (!fs::exists(path)) return true;
        if (hasMarker(path)) { fs::remove(path); return true; }
        rollbackIfBackedUp(path);
        return true;
    } catch (...) { return false; }
}

bool Installer::copyInjectionDll(const std::wstring& gameDir, const std::wstring& injectName) {
    HMODULE h = GetModuleHandleW(nullptr);
    
    auto src = fs::path(gameDir) / L"OptiScaler.dll";
    auto dst = fs::path(gameDir) / injectName;
    
    backupOriginal(dst.wstring());
    try {
        fs::copy_file(src, dst, fs::copy_options::overwrite_existing);
        markOurs(dst.wstring());
        LOG_SUCCESS(std::string("注入 DLL: ") + w2s(dst.wstring()));
        return true;
    } catch (const std::exception& ex) {
        LOG_ERROR(std::string("复制注入 DLL 失败: ") + ex.what());
        return false;
    }
}

bool Installer::optimize(const InstallOptions& opt, std::wstring& errOut) {
    HMODULE h = GetModuleHandleW(nullptr);
    
    auto xr = ExtractOptiScalerBundle(h, opt.gamePath);
    if (!xr.ok) { errOut = L"释放资源失败: " + xr.error; return false; }
    LOG_SUCCESS(std::string("释放 ") + std::to_string(xr.filesWritten) + " 个文件 ("
                + std::to_string(xr.totalBytes / 1024) + " KB)");

    ProfileDecision dec = ProfileAdvisor::advise(opt);
    auto iniPath = fs::path(opt.gamePath) / L"OptiScaler.ini";
    {
        
        if (fs::exists(iniPath) && !hasMarker(iniPath.wstring())) {
            try { fs::copy_file(iniPath, iniPath.wstring() + L".osa_bak", fs::copy_options::overwrite_existing); } catch (...) {}
        }
        std::ofstream f(iniPath, std::ios::binary | std::ios::trunc);
        if (!f) { errOut = L"无法写入 OptiScaler.ini"; return false; }
        f.write(dec.iniContent.data(), dec.iniContent.size());
        f.close();
        markOurs(iniPath.wstring());
    }
    LOG_SUCCESS(std::string("OptiScaler.ini 已生成 (") + w2s(opt.gamePath) + ")");

    if (!copyInjectionDll(opt.gamePath, opt.injection + L".dll")) {
        errOut = L"注入 DLL 失败";
        return false;
    }

    wchar_t* appdata = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appdata))) {
        fs::path logDir = fs::path(appdata) / L"OptiScalerAssistant";
        fs::create_directories(logDir);
        auto logPath = logDir / L"install.log";
        std::ofstream lg(logPath, std::ios::app);
        if (lg) {
            SYSTEMTIME st; GetLocalTime(&st);
            lg << "[" << st.wYear << "-" << st.wMonth << "-" << st.wDay
               << " " << st.wHour << ":" << st.wMinute << ":" << st.wSecond << "] "
               << "Optimize: " << w2s(opt.gamePath)
               << " | GPU: " << w2s(opt.gpu.name)
               << " | Inject: " << w2s(opt.injection)
               << " | Up: " << w2s(dec.upscalerFinal)
               << " | Preset: " << w2s(dec.qualityPreset)
               << " | FG: " << (dec.enableFG ? "yes" : "no")
               << "\n";
        }
        CoTaskMemFree(appdata);
    }

    return true;
}

bool Installer::uninstall(const std::wstring& gameDir, std::wstring& errOut) {
    if (!fs::exists(gameDir)) { errOut = L"目录不存在"; return false; }
    
    static const wchar_t* injects[] = {
        L"winmm.dll", L"version.dll", L"dxgi.dll",
        L"d3d12.dll", L"dbghelp.dll", L"wininet.dll", L"winhttp.dll"
    };
    for (auto name : injects) {
        auto p = fs::path(gameDir) / name;
        if (fs::exists(p)) {
            if (hasMarker(p.wstring())) {
                fs::remove(p);
                LOG_INFO(std::string("已移除: ") + w2s(p.wstring()));
            } else if (fs::exists(p.wstring() + L".osa_bak")) {
                rollbackIfBackedUp(p.wstring());
            }
        }
    }
    
    static const wchar_t* ours[] = {
        L"OptiScaler.dll", L"nvngx_orig.dll",
        L"fakenvapi.dll", L"libxell.dll",
        L"amd_fidelityfx_dx12.dll", L"amd_fidelityfx_framegeneration_dx12.dll",
        L"amd_fidelityfx_upscaler_dx12.dll", L"amd_fidelityfx_vk.dll",
        L"libxess.dll", L"libxess_dx11.dll", L"libxess_fg.dll",
        L"dlssg_to_fsr3_amd_is_better.dll"
    };
    for (auto name : ours) {
        auto p = fs::path(gameDir) / name;
        if (fs::exists(p)) {
            try { fs::remove(p); LOG_INFO(std::string("已移除: ") + w2s(p.wstring())); } catch (...) {}
        }
    }
    return true;
}

bool Installer::launchGame(const std::wstring& exePath, std::wstring& errOut) {
    if (!fs::exists(exePath)) { errOut = L"游戏可执行文件不存在"; return false; }
    auto dir = fs::path(exePath).parent_path().wstring();
    STARTUPINFOW si{}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::wstring cmd = L"\"" + exePath + L"\"";
    if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr,
                        dir.c_str(), &si, &pi)) {
        errOut = L"CreateProcess 失败: " + std::to_wstring(GetLastError());
        return false;
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
}

} 
