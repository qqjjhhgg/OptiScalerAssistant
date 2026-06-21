
#pragma once
#include <windows.h>
#include <wrl.h>
#include <WebView2.h>
#include <functional>
#include <string>
#include <mutex>

namespace OSA {

using namespace Microsoft::WRL;

class WebView2Host {
public:
    using OnJsMessage = std::function<void(const std::string& json)>;
    using OnNavigationCompleted = std::function<void()>;

    WebView2Host();
    ~WebView2Host();

    bool init(HWND hwndParent, const std::wstring& browserDataDir,
              const std::string& html, OnJsMessage onMsg,
              OnNavigationCompleted onNavDone = nullptr);
    void shutdown();

    void postMessage(const std::string& json);
    void postJsonObject(const std::string& key, const std::string& json);

    void resize(RECT rc);
    bool isReady() const { return webview_ != nullptr; }

private:
    void onWebMessageReceived(ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args);
    void onNavigationCompletedHandler(ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args);
    void applyDpiScale();

    ComPtr<ICoreWebView2Controller> controller_;
    ComPtr<ICoreWebView2> webview_;
    OnJsMessage onMsg_;
    OnNavigationCompleted onNavDone_;
    std::string pendingHtml_;
    std::mutex mtx_;
    EventRegistrationToken msgToken_{};
    EventRegistrationToken navToken_{};
    HWND hwndParent_ = nullptr;
    float dpiScale_ = 1.0f;
};

} 
