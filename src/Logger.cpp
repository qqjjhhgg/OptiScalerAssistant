#include "Logger.h"
#include <fstream>

namespace OSA {

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

void Logger::setSink(Sink s) {
    std::lock_guard<std::mutex> lk(mtx_);
    sink_ = std::move(s);
    
    for (auto& e : history_) sink_(e);
}

void Logger::flushTo(Sink s) {
    std::lock_guard<std::mutex> lk(mtx_);
    for (auto& e : history_) s(e);
}

void Logger::log(LogLevel lvl, const std::string& msg) {
    LogEntry e{lvl, msg, {}};
    GetLocalTime(&e.time);
    {
        std::lock_guard<std::mutex> lk(mtx_);
        history_.push_back(e);
        if (history_.size() > 500) history_.pop_front();
        if (sink_) sink_(e);
    }
    if (fileHandle_ != INVALID_HANDLE_VALUE) {
        char buf[2048];
        int n = snprintf(buf, sizeof(buf), "[%04u-%02u-%02u %02u:%02u:%02u.%03u] %s\n",
                         e.time.wYear, e.time.wMonth, e.time.wDay,
                         e.time.wHour, e.time.wMinute, e.time.wSecond, e.time.wMilliseconds,
                         msg.c_str());
        if (n > 0) {
            DWORD written;
            WriteFile(fileHandle_, buf, (DWORD)n, &written, nullptr);
        }
    }
}

void Logger::file(const std::wstring& path) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (fileHandle_ != INVALID_HANDLE_VALUE) CloseHandle(fileHandle_);
    filePath_ = path;
    fileHandle_ = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (fileHandle_ == INVALID_HANDLE_VALUE) {
        
    }
}

} 
