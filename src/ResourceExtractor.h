
#pragma once
#include <windows.h>
#include <string>

namespace OSA {

enum EmbeddedRes : int {
    RES_HTML = 100,
    RES_INI_DEFAULT = 101,
    RES_OPTISCALER_DLL = 200,
    RES_NVNGX = 201,
    RES_FAKENVAPI_DLL = 202,
    RES_FAKENVAPI_INI = 203,
    RES_LIBXELL = 204,
    
    RES_FSR_DX12 = 220,
    RES_FSR_FG = 221,
    RES_FSR_UPSCALER = 222,
    RES_FSR_VK = 223,
    RES_XESS = 230,
    RES_XESS_DX11 = 231,
    RES_XESS_FG = 232,
    RES_DLSSG_FSR3 = 240,
};

bool ExtractResource(HMODULE hModule, int resId, const std::wstring& destPath, bool overwrite = true);

struct ExtractResult {
    bool ok = false;
    int filesWritten = 0;
    int64_t totalBytes = 0;
    std::wstring error;
};
ExtractResult ExtractOptiScalerBundle(HMODULE hModule, const std::wstring& gameDir);

struct HtmlData {
    const void* ptr = nullptr;
    size_t size = 0;
};
HtmlData LoadHtmlResource(HMODULE hModule, int resId = RES_HTML);

} 
