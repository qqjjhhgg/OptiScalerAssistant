
#pragma once
#include "WebView2Host.h"
#include "GpuDetector.h"
#include "GameScanner.h"
#include "ProfileAdvisor.h"
#include <windows.h>
#include <string>
#include <functional>
#include <ShlObj.h>

namespace OSA {

class MainWindow {
public:
    MainWindow();
    ~MainWindow();
    bool create(HINSTANCE hInst);
    int  run();

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT onMessage(HWND, UINT, WPARAM, LPARAM);

    void onCreate(HWND);
    void onSize(HWND);
    void onClose(HWND);
    void onWebViewReady();

    void onJsMessage(const std::string& json);
    void sendToJs(const std::string& op, const std::string& payloadJson);
    void sendToJsWrapped(const std::string& op, const std::string& key, const std::string& payloadJson);
    void sendLog(const std::string& level, const std::string& msg);
    void sendError(const std::string& op, const std::wstring& msg);
    void sendRecommendedIni();
    void sendScreenResolution();

    void doDetectGpu();
    void doBrowseFolder();
    void doScanGame(const std::wstring& path);
    void doOptimize(const std::string& json);
    void doUninstall(const std::wstring& path);
    void doOpenGame(const std::wstring& path);

    HWND hwnd_ = nullptr;
    HINSTANCE hInst_ = nullptr;
    std::unique_ptr<WebView2Host> webview_;

    GpuInfo gpu_;
    GameInfo game_;
    bool gpuDetected_ = false;
};

} 
