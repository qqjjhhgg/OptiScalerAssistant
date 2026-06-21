
#include "MainWindow.h"
#include "Logger.h"
#include <windows.h>
#include <combaseapi.h>
#include <filesystem>
#include <cstdio>

#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace fs = std::filesystem;

namespace {

bool isAnotherInstanceRunning() {
    HANDLE m = CreateMutexW(nullptr, TRUE, L"Global\\OptiScalerAssistant_SingleInstance");
    if (!m) return false;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(m);
        return true;
    }
    
    static HANDLE g_singleInstanceMutex = m;
    (void)g_singleInstanceMutex;
    return false;
}

void bringExistingToFront() {
    HWND h = FindWindowW(L"OSA_MainWindow", nullptr);
    if (h) {
        ShowWindow(h, SW_RESTORE);
        SetForegroundWindow(h);
    }
}

} 

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR lpCmdLine, int nCmdShow) {
    (void)lpCmdLine; (void)nCmdShow;

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    if (isAnotherInstanceRunning()) {
        bringExistingToFront();
        return 0;
    }

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    char pidBuf[64];
    snprintf(pidBuf, sizeof(pidBuf), "PID=%u, MainThread=0x%X",
             GetCurrentProcessId(), GetCurrentThreadId());
    OSA::Logger::instance().info("===== OptiScaler 自动优化 v1.0 Started =====");
    OSA::Logger::instance().info(pidBuf);

    BOOL isAdmin = FALSE;
    PSID adminGroup = nullptr;
    SID_IDENTIFIER_AUTHORITY ntAuth = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuth, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
                                  0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(nullptr, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    OSA::Logger::instance().info(std::string("Admin: ") + (isAdmin ? "yes" : "no"));
    {
        FILE* f = nullptr;
        fopen_s(&f, "C:\\temp\\osa_debug.txt", "a");
        if (f) {
            fprintf(f, "admin check done, isAdmin=%d, pid=%u\n", isAdmin, GetCurrentProcessId());
            fclose(f);
        }
    }
    if (!isAdmin) {
        
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        SHELLEXECUTEINFOW sei{};
        sei.cbSize = sizeof(sei);
        sei.lpVerb = L"runas";
        sei.lpFile = path;
        sei.nShow = SW_SHOWNORMAL;
        if (ShellExecuteExW(&sei)) {
            return 0;  
        }
        MessageBoxW(nullptr, L"无法以管理员身份启动，部分功能可能不可用。\n（写入 Program Files 下的游戏目录需要管理员权限）",
                    L"OptiScaler 自动优化", MB_OK | MB_ICONWARNING);
    }

    OSA::MainWindow wnd;
    if (!wnd.create(hInst)) {
        MessageBoxW(nullptr, L"窗口创建失败。", L"OptiScaler 自动优化", MB_OK | MB_ICONERROR);
        return 1;
    }
    return wnd.run();
}
