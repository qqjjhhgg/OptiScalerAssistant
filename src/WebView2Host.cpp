
#include "WebView2Host.h"
#include "Logger.h"
#include <Shlwapi.h>
#include <ShlObj.h>
#include <shellscalingapi.h>
#include <sstream>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "WebView2LoaderStatic.lib")

namespace OSA {

namespace {
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
} 

WebView2Host::WebView2Host() = default;
WebView2Host::~WebView2Host() { shutdown(); }

bool WebView2Host::init(HWND hwndParent, const std::wstring& browserDataDir,
                        const std::string& html, OnJsMessage onMsg,
                        OnNavigationCompleted onNavDone) {
    onMsg_ = std::move(onMsg);
    onNavDone_ = std::move(onNavDone);
    pendingHtml_ = html;
    hwndParent_ = hwndParent;

    applyDpiScale();

    std::wstring dataDir = browserDataDir;
    if (dataDir.empty()) {
        wchar_t path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, path))) {
            dataDir = std::wstring(path) + L"\\OptiScalerAssistant\\WebView2";
        }
    }

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, dataDir.empty() ? nullptr : dataDir.c_str(), nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(result)) {
                    LOG_ERROR("CreateCoreWebView2Environment 失败: 0x" + std::to_string((unsigned)result));
                    return result;
                }
                env->CreateCoreWebView2Controller(hwndParent_,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [this](HRESULT r2, ICoreWebView2Controller* c) -> HRESULT {
                            if (FAILED(r2)) {
                                LOG_ERROR("CreateCoreWebView2Controller 失败: 0x" + std::to_string((unsigned)r2));
                                return r2;
                            }
                            controller_ = c;
                            controller_->get_CoreWebView2(&webview_);
                            if (!webview_) return E_FAIL;

                            ICoreWebView2Settings* st = nullptr;
                            webview_->get_Settings(&st);
                            if (st) {
                                st->put_AreDevToolsEnabled(FALSE);
                                st->put_AreDefaultContextMenusEnabled(FALSE);
                                st->put_IsStatusBarEnabled(FALSE);
                                st->put_IsZoomControlEnabled(FALSE);
                                st->Release();
                            }

                            webview_->add_WebMessageReceived(
                                Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                    [this](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* a) -> HRESULT {
                                        onWebMessageReceived(nullptr, a);
                                        return S_OK;
                                    }).Get(),
                                &msgToken_);

                            webview_->add_NavigationCompleted(
                                Callback<ICoreWebView2NavigationCompletedEventHandler>(
                                    [this](ICoreWebView2* s, ICoreWebView2NavigationCompletedEventArgs* a) -> HRESULT {
                                        onNavigationCompletedHandler(s, a);
                                        return S_OK;
                                    }).Get(),
                                &navToken_);

                            RECT rc{};
                            if (GetClientRect(hwndParent_, &rc)) {
                                resize(rc);
                            }

                            auto w = utf8ToUtf16(pendingHtml_);
                            webview_->NavigateToString(w.c_str());
                            LOG_SUCCESS("WebView2 环境已创建，正在导航...");
                            return S_OK;
                        }).Get());
                return S_OK;
            }).Get());

    if (FAILED(hr)) {
        LOG_ERROR("CreateCoreWebView2EnvironmentWithOptions 失败: 0x" + std::to_string((unsigned)hr));
        return false;
    }
    return true;
}

void WebView2Host::shutdown() {
    if (webview_ && msgToken_.value) {
        webview_->remove_WebMessageReceived(msgToken_);
        msgToken_.value = 0;
    }
    if (webview_ && navToken_.value) {
        webview_->remove_NavigationCompleted(navToken_);
        navToken_.value = 0;
    }
    if (controller_) {
        controller_->Close();
        controller_.Reset();
    }
    webview_.Reset();
}

void WebView2Host::onWebMessageReceived(ICoreWebView2* , ICoreWebView2WebMessageReceivedEventArgs* args) {
    LPWSTR msg = nullptr;
    if (FAILED(args->TryGetWebMessageAsString(&msg)) || !msg) return;
    std::wstring w(msg);
    CoTaskMemFree(msg);
    std::string s = utf16ToUtf8(w);
    if (onMsg_) onMsg_(s);
}

void WebView2Host::onNavigationCompletedHandler(ICoreWebView2* , ICoreWebView2NavigationCompletedEventArgs* ) {
    LOG_SUCCESS("WebView2 导航完成");
    if (onNavDone_) onNavDone_();
}

void WebView2Host::postMessage(const std::string& json) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (!webview_) return;
    auto w = utf8ToUtf16(json);
    webview_->PostWebMessageAsString(w.c_str());
}

void WebView2Host::postJsonObject(const std::string& key, const std::string& json) {
    std::ostringstream os;
    os << "{\"" << key << "\":" << json << "}";
    postMessage(os.str());
}

void WebView2Host::resize(RECT rc) {
    if (controller_) {

        controller_->put_Bounds(rc);
    }
}

void WebView2Host::applyDpiScale() {
    UINT dpi = GetDpiForWindow(hwndParent_);
    if (dpi == 0) {
        HDC hdc = GetDC(hwndParent_);
        if (hdc) {
            dpi = GetDeviceCaps(hdc, LOGPIXELSX);
            ReleaseDC(hwndParent_, hdc);
        }
    }
    if (dpi == 0) dpi = 96;
    dpiScale_ = dpi / 96.0f;
}

} 
