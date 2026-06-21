
#include "ResourceExtractor.h"
#include "Logger.h"
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstdint>

namespace fs = std::filesystem;

namespace OSA {

namespace {

struct ResourceEntry {
    int id;
    const wchar_t* filename;
};

const ResourceEntry kBundle[] = {
    { RES_OPTISCALER_DLL,  L"OptiScaler.dll" },
    { RES_FAKENVAPI_DLL,   L"fakenvapi.dll" },
    { RES_FAKENVAPI_INI,   L"fakenvapi.ini" },
    { RES_LIBXELL,         L"libxell.dll" },
    { RES_FSR_DX12,        L"amd_fidelityfx_dx12.dll" },
    { RES_FSR_FG,          L"amd_fidelityfx_framegeneration_dx12.dll" },
    { RES_FSR_UPSCALER,    L"amd_fidelityfx_upscaler_dx12.dll" },
    { RES_FSR_VK,          L"amd_fidelityfx_vk.dll" },
    { RES_XESS,            L"libxess.dll" },
    { RES_XESS_DX11,       L"libxess_dx11.dll" },
    { RES_XESS_FG,         L"libxess_fg.dll" },
    { RES_DLSSG_FSR3,      L"dlssg_to_fsr3_amd_is_better.dll" },
};

} 

bool ExtractResource(HMODULE hModule, int resId, const std::wstring& destPath, bool overwrite) {
    HRSRC hRes = FindResourceW(hModule, MAKEINTRESOURCEW(resId), RT_RCDATA);
    if (!hRes) {
        LOG_WARN("资源未找到: id=" + std::to_string(resId));
        return false;
    }
    HGLOBAL hGlob = LoadResource(hModule, hRes);
    if (!hGlob) {
        LOG_WARN("加载资源失败: id=" + std::to_string(resId));
        return false;
    }
    void* p = LockResource(hGlob);
    DWORD size = SizeofResource(hModule, hRes);
    if (!p || size == 0) {
        LOG_WARN("资源锁定失败/为空: id=" + std::to_string(resId));
        return false;
    }

    try {
        fs::path p_(destPath);
        if (p_.has_parent_path()) fs::create_directories(p_.parent_path());
    } catch (...) {}

    if (fs::exists(destPath) && !overwrite) return true;

    std::ofstream f(destPath, std::ios::binary | std::ios::trunc);
    if (!f) {
        LOG_ERROR(std::string("无法写入: ") + w2s(destPath));
        return false;
    }
    f.write((const char*)p, size);
    f.close();
    return true;
}

ExtractResult ExtractOptiScalerBundle(HMODULE hModule, const std::wstring& gameDir) {
    ExtractResult r;
    try {
        if (!fs::exists(gameDir)) fs::create_directories(gameDir);
        for (auto& e : kBundle) {
            auto dest = fs::path(gameDir) / e.filename;
            if (ExtractResource(hModule, e.id, dest.wstring())) {
                r.filesWritten++;
                r.totalBytes += fs::file_size(dest);
            }
        }
        r.ok = true;
    } catch (const std::exception& ex) {
        r.error = std::wstring(ex.what(), ex.what() + strlen(ex.what()));
    }
    return r;
}

HtmlData LoadHtmlResource(HMODULE hModule, int resId) {
    HtmlData h;
    HRSRC hRes = FindResourceW(hModule, MAKEINTRESOURCEW(resId), RT_RCDATA);
    if (!hRes) return h;
    HGLOBAL hGlob = LoadResource(hModule, hRes);
    if (!hGlob) return h;
    h.ptr = LockResource(hGlob);
    h.size = SizeofResource(hModule, hRes);
    return h;
}

} 
