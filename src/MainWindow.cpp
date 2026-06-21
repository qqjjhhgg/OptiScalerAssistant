
#define NOMINMAX
#include "MainWindow.h"
#include "Resource.h"
#include "ResourceExtractor.h"
#include "GpuDetector.h"
#include "GameScanner.h"
#include "ProfileAdvisor.h"
#include "Installer.h"
#include "Logger.h"
#include <CommCtrl.h>
#include <ShlObj.h>
#include <filesystem>
#include <regex>
#include <sstream>
#include <cstdio>
#include <algorithm>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "ole32.lib")

namespace fs = std::filesystem;

namespace OSA {

namespace {

std::string jsonGet(const std::string& json, const std::string& key, const std::string& def = "") {
    
    auto pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return def;
    pos += key.size() + 2;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' ||
                                  json[pos] == '\n' || json[pos] == '\r')) ++pos;
    if (pos >= json.size() || json[pos] != ':') return def;
    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' ||
                                  json[pos] == '\n' || json[pos] == '\r')) ++pos;
    if (pos >= json.size()) return def;

    if (json[pos] == '"') {
        std::string out;
        out.reserve(32);
        for (size_t i = pos + 1; i < json.size(); ++i) {
            char c = json[i];
            if (c == '"') return out;
            if (c == '\\' && i + 1 < json.size()) {
                char n = json[i + 1];
                if (n == '"') out.push_back('"');
                else if (n == '\\') out.push_back('\\');
                else if (n == 'n') out.push_back('\n');
                else if (n == 'r') out.push_back('\r');
                else if (n == 't') out.push_back('\t');
                else { out.push_back(c); out.push_back(n); }
                ++i;
            } else {
                out.push_back(c);
            }
        }
        return out;
    }

    auto e = json.find_first_of(",} ]\t\n\r", pos);
    if (e == std::string::npos) e = json.size();
    return json.substr(pos, e - pos);
}

std::wstring utf8ToUtf16(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w; w.resize(n);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}
std::string utf16ToUtf8(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s; s.resize(n);
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}

std::string escapeForJson(const std::wstring& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (auto c : s) {
        if (c == L'"' || c == L'\\') { out.push_back('\\'); out.push_back((char)c); }
        else if (c == L'\n') out.append("\\n");
        else if (c == L'\r') out.append("\\r");
        else if (c == L'\t') out.append("\\t");
        else if (c < 0x80) out.push_back((char)c);
        else {
            char buf[8];
            int n = WideCharToMultiByte(CP_UTF8, 0, &c, 1, buf, sizeof(buf), nullptr, nullptr);
            out.append(buf, n);
        }
    }
    return out;
}

} 

MainWindow::MainWindow() = default;
MainWindow::~MainWindow() = default;

bool MainWindow::create(HINSTANCE hInst) {
    hInst_ = hInst;
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"OSA_MainWindow";
    if (!RegisterClassExW(&wc)) {
        DWORD e = GetLastError();
        if (e != ERROR_CLASS_ALREADY_EXISTS) {
            LOG_ERROR("RegisterClass failed: " + std::to_string(e));
            return false;
        }
    }

    
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int shortSide = (sw < sh) ? sw : sh;
    
    int w, h;
    if (shortSide >= 2160) {
        w = 1235; h = 1680;
    } else {
        float k = shortSide / 2160.0f;
        
        w = std::max(800, (int)(1235.0f * k));
        h = std::max(1000, (int)(1680.0f * k));
    }
    
    int shFull = GetSystemMetrics(SM_CYFULLSCREEN);
    if (w > sw) w = sw;
    if (h > shFull) h = shFull;
    int x = (sw - w) / 2, y = (shFull - h) / 2;
    if (y < 0) y = 0;

    hwnd_ = CreateWindowExW(
        0,
        L"OSA_MainWindow",
        L"OptiScaler 自动优化 v1.0",
        WS_OVERLAPPEDWINDOW,
        x, y, w, h,
        nullptr, nullptr, hInst, this);
    if (!hwnd_) {
        LOG_ERROR("CreateWindow failed: " + std::to_string(GetLastError()));
        return false;
    }

    {
        HICON hIconBig   = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_MAINICON),
                                             IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
        HICON hIconSmall = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_MAINICON),
                                             IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
        if (hIconBig)   SendMessageW(hwnd_, WM_SETICON, ICON_BIG,   (LPARAM)hIconBig);
        if (hIconSmall) SendMessageW(hwnd_, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);
    }

    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);

    HMODULE hMod = GetModuleHandleW(nullptr);
    auto html = LoadHtmlResource(hMod, RES_HTML);
    std::string htmlStr;
    if (html.ptr && html.size) {
        htmlStr.assign((const char*)html.ptr, html.size);
    } else {
        htmlStr = R"(<!doctype html><meta charset=utf-8><body style="background:#0e141b;color:#e6edf3;font-family:Segoe UI;padding:40px"><h1>Resource load failed</h1><p>app.html not embedded in exe. Please check rc resources.</p></body>)";
    }

    std::wstring dataDir;
    wchar_t* appdata = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appdata))) {
        dataDir = (fs::path(appdata) / L"OptiScalerAssistant" / L"WebView2").wstring();
        CoTaskMemFree(appdata);
    }

    webview_ = std::make_unique<WebView2Host>();
    if (!webview_->init(hwnd_, dataDir, htmlStr,
                        [this](const std::string& j){ onJsMessage(j); },
                        [this](){ onWebViewReady(); })) {
        LOG_ERROR("WebView2 init failed");
    }
    return true;
}

LRESULT CALLBACK MainWindow::WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    MainWindow* self = nullptr;
    if (m == WM_NCCREATE) {
        auto cs = reinterpret_cast<CREATESTRUCTW*>(l);
        self = reinterpret_cast<MainWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    }
    if (self) return self->onMessage(h, m, w, l);
    return DefWindowProcW(h, m, w, l);
}

LRESULT MainWindow::onMessage(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_CREATE: onCreate(h); return 0;
    case WM_SIZE:   onSize(h);   return 0;
    case WM_GETMINMAXINFO: {
        auto* mmi = reinterpret_cast<MINMAXINFO*>(l);
        mmi->ptMinTrackSize.x = 640;
        mmi->ptMinTrackSize.y = 520;
        return 0;
    }
    case WM_CLOSE:  onClose(h);  return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

void MainWindow::onCreate(HWND) {
    wchar_t* appdata = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appdata))) {
        fs::path p = fs::path(appdata) / L"OptiScalerAssistant";
        fs::create_directories(p);
        Logger::instance().file((p / L"app.log").wstring());
        CoTaskMemFree(appdata);
    }
    
    Logger::instance().setSink([this](const LogEntry& e){
        const char* lvl = "info";
        switch (e.level) {
        case LogLevel::Info:    lvl = "info"; break;
        case LogLevel::Success: lvl = "success"; break;
        case LogLevel::Warn:    lvl = "warn"; break;
        case LogLevel::Error:   lvl = "error"; break;
        }
        std::ostringstream os;
        os << "{\"op\":\"log\",\"level\":\"" << lvl << "\",\"msg\":\""
           << escapeForJson(utf8ToUtf16(e.msg)) << "\"}";
        if (webview_) webview_->postMessage(os.str());
    });

    doDetectGpu();
}

void MainWindow::onWebViewReady() {
    
    sendLog("success", "WebView2 导航完成");
    webview_->postMessage("{\"op\":\"app-ready\"}");

    sendScreenResolution();
    if (gpuDetected_ && !gpu_.name.empty()) {
        auto json = GpuDetector::toJson(gpu_);
        sendToJsWrapped("gpu-info", "gpu", utf16ToUtf8(json));
    }
}

void MainWindow::onSize(HWND) {
    if (webview_ && hwnd_) {
        RECT rc; GetClientRect(hwnd_, &rc);
        webview_->resize(rc);
    }
}

void MainWindow::onClose(HWND) {
    if (webview_) webview_->shutdown();
    DestroyWindow(hwnd_);
}

int MainWindow::run() {
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}

void MainWindow::sendToJs(const std::string& op, const std::string& payloadJson) {
    if (!webview_) return;
    std::string body = payloadJson.empty() ? "{}" : payloadJson;
    if (!body.empty() && body.front() == '{') body = body.substr(1);
    if (!body.empty() && body.back() == '}') body.pop_back();
    std::ostringstream os;
    os << "{\"op\":\"" << op << "\"," << body << "}";
    webview_->postMessage(os.str());
}

void MainWindow::sendToJsWrapped(const std::string& op, const std::string& key, const std::string& payloadJson) {
    if (!webview_) return;
    std::ostringstream os;
    os << "{\"op\":\"" << op << "\",\"" << key << "\":" << (payloadJson.empty() ? "{}" : payloadJson) << "}";
    webview_->postMessage(os.str());
}

void MainWindow::sendLog(const std::string& level, const std::string& msg) {
    if (!webview_) return;
    std::ostringstream os;
    os << "{\"op\":\"log\",\"level\":\"" << level << "\",\"msg\":\""
       << escapeForJson(utf8ToUtf16(msg)) << "\"}";
    webview_->postMessage(os.str());
}

void MainWindow::sendError(const std::string& op, const std::wstring& msg) {
    std::string payload = "{\"error\":\"" + escapeForJson(msg) + "\"}";
    sendToJs(op + "-fail", payload);
}

void MainWindow::sendScreenResolution() {
    if (!webview_) return;
    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);
    std::ostringstream os;
    os << "{\"width\":" << w << ",\"height\":" << h << "}";
    sendToJsWrapped("screen-resolution", "res", os.str());
}

void MainWindow::sendRecommendedIni() {
    if (!gpuDetected_ || gpu_.name.empty()) return;
    InstallOptions dummy;
    dummy.gpu = gpu_;
    dummy.resolution = L"2560x1440";
    dummy.injection = L"winmm";
    dummy.upscaler = L"auto";
    dummy.fg = true;
    dummy.sharpen = true;
    dummy.hotkeyMenu = L"Insert";
    auto dec = ProfileAdvisor::advise(dummy);
    std::string ini = dec.iniContent;
    
    std::string payload = "{\"ini\":\"" + escapeForJson(utf8ToUtf16(ini)) + "\"}";
    sendToJs("recommended-ini", payload);
}

void MainWindow::onJsMessage(const std::string& json) {
    LOG_INFO(std::string("[JS recv] ") + json);
    auto op = jsonGet(json, "op");
    if (op == "ready") {
        sendScreenResolution();
        if (gpuDetected_ && !gpu_.name.empty()) {
            auto j = GpuDetector::toJson(gpu_);
            sendToJsWrapped("gpu-info", "gpu", utf16ToUtf8(j));
        }
    } else if (op == "detect-gpu") {
        doDetectGpu();
    } else if (op == "browse-folder") {
        doBrowseFolder();
    } else if (op == "scan-game") {
        auto path = utf8ToUtf16(jsonGet(json, "path"));
        doScanGame(path);
    } else if (op == "optimize") {
        doOptimize(json);
    } else if (op == "uninstall") {
        auto path = utf8ToUtf16(jsonGet(json, "path"));
        doUninstall(path);
    } else if (op == "open-game") {
        auto path = utf8ToUtf16(jsonGet(json, "path"));
        doOpenGame(path);
    } else if (op == "save-config") {
        
    } else {
        LOG_WARN("Unknown op: " + op);
    }
}

void MainWindow::doDetectGpu() {
    try {
        auto all = GpuDetector::detectAll();
        if (all.empty()) {
            gpuDetected_ = false;
            sendError("gpu", L"未检测到任何 GPU");
            return;
        }
        GpuInfo best;
        for (auto& g : all) if (g.isSelected) { best = g; break; }
        if (best.name.empty()) best = all.front();
        gpu_ = best;
        gpuDetected_ = true;

        auto json = GpuDetector::toJson(best);
        sendScreenResolution();
        sendToJsWrapped("gpu-info", "gpu", utf16ToUtf8(json));

        for (auto& g : all) {
            std::ostringstream os;
            os << "  - " << utf16ToUtf8(g.name) << " | " << utf16ToUtf8(g.vendor)
               << " | VRAM=" << g.vramMB << "MB"
               << " | tier=" << g.tier
               << (g.isVirtual ? " | VIRTUAL" : "")
               << (g.isIntegrated ? " | iGPU" : "")
               << (g.isSelected ? " | <-- SELECTED" : "")
               << " | " << utf16ToUtf8(g.reason);
            LOG_INFO(os.str());
        }
    } catch (const std::exception& ex) {
        gpuDetected_ = false;
        sendError("gpu", utf8ToUtf16(ex.what()));
    }
}

void MainWindow::doBrowseFolder() {
    IFileOpenDialog* dlg = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&dlg));
    if (FAILED(hr) || !dlg) {
        if (dlg) dlg->Release();
        return;
    }
    DWORD opts;
    dlg->GetOptions(&opts);
    
    dlg->SetOptions(opts | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST);
    COMDLG_FILTERSPEC spec[] = {
        { L"游戏可执行文件", L"*.exe" },
        { L"所有文件",       L"*.*" }
    };
    dlg->SetFileTypes(ARRAYSIZE(spec), spec);
    dlg->SetFileTypeIndex(1);
    dlg->SetTitle(L"选择游戏启动程序 (game.exe)");
    if (SUCCEEDED(dlg->Show(hwnd_))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item))) {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                std::wstring exePath(path);
                CoTaskMemFree(path);
                
                std::ostringstream os;
                os << "{\"op\":\"path-pick\",\"path\":\""
                   << escapeForJson(exePath) << "\",\"isExe\":true}";
                if (webview_) webview_->postMessage(os.str());
            }
            item->Release();
        }
    }
    dlg->Release();
}

void MainWindow::doScanGame(const std::wstring& path) {
    try {
        LOG_INFO(std::string("[doScanGame] path=") + utf16ToUtf8(path));
        auto info = GameScanner::scan(path);
        game_ = info;
        LOG_INFO(std::string("[doScanGame] exeName=") + utf16ToUtf8(info.exeName) +
                 " gameType=" + utf16ToUtf8(info.gameType) +
                 " arch=" + utf16ToUtf8(info.arch) +
                 " acLevel=" + utf16ToUtf8(info.acLevel));
        if (info.exeName.empty()) {
            sendError("game", L"未找到 game.exe");
            return;
        }
        auto json = GameScanner::toJson(info);
        LOG_INFO(std::string("[doScanGame] json=") + utf16ToUtf8(json));
        sendToJsWrapped("game-info", "info", utf16ToUtf8(json));
        
        LOG_INFO(std::string("[doScanGame] sent game-info to JS"));
    } catch (const std::exception& ex) {
        sendError("game", utf8ToUtf16(ex.what()));
    }
}

void MainWindow::doOptimize(const std::string& json) {
    InstallOptions opt;
    opt.gamePath    = utf8ToUtf16(jsonGet(json, "gamePath"));
    opt.resolution  = utf8ToUtf16(jsonGet(json, "resolution", "2560x1440"));
    opt.injection   = utf8ToUtf16(jsonGet(json, "injection", "winmm"));
    opt.upscaler    = utf8ToUtf16(jsonGet(json, "upscaler", "auto"));
    opt.fg          = jsonGet(json, "fg", "true") == "true";
    opt.sharpen     = jsonGet(json, "sharpen", "true") == "true";
    opt.hotkeyMenu  = utf8ToUtf16(jsonGet(json, "hotkeyMenu", "Insert"));
    opt.gpu = gpuDetected_ ? gpu_ : GpuDetector::detectBest();
    game_ = opt.gamePath.empty() ? GameInfo{} : GameScanner::scan(opt.gamePath);
    opt.game = game_;

    std::wstring err;
    if (!Installer::optimize(opt, err)) {
        sendError("optimize", err);
        return;
    }
    sendToJs("optimize-done", "{\"msg\":\"安装完成，可启动游戏测试\"}");
}

void MainWindow::doUninstall(const std::wstring& path) {
    std::wstring err;
    if (!Installer::uninstall(path, err)) {
        sendError("uninstall", err);
        return;
    }
    sendToJs("uninstall-done", "{\"msg\":\"已卸载 OptiScaler\"}");
}

void MainWindow::doOpenGame(const std::wstring& path) {
    if (!fs::exists(path)) {
        sendError("open-game", L"目录不存在");
        return;
    }
    game_ = GameScanner::scan(path);
    if (game_.exePath.empty()) {
        sendError("open-game", L"未找到 game.exe");
        return;
    }
    std::wstring err;
    if (!Installer::launchGame(game_.exePath, err)) {
        sendError("open-game", err);
        return;
    }
    sendLog("success", "游戏已启动: " + utf16ToUtf8(game_.exeName));
}

} 
