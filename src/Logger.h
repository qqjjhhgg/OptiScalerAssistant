
#pragma once
#include <string>
#include <mutex>
#include <deque>
#include <functional>
#include <windows.h>

namespace OSA {

inline std::string w2s(const std::wstring& w) {
    if (w.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string out((size_t)len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), out.data(), len, nullptr, nullptr);
    return out;
}

enum class LogLevel { Info, Success, Warn, Error };

struct LogEntry {
    LogLevel level;
    std::string msg;
    SYSTEMTIME time;
};

class Logger {
public:
    using Sink = std::function<void(const LogEntry&)>;

    static Logger& instance();

    void setSink(Sink s);                     
    void log(LogLevel lvl, const std::string& msg);
    void info(const std::string& m)    { log(LogLevel::Info,    m); }
    void success(const std::string& m) { log(LogLevel::Success, m); }
    void warn(const std::string& m)    { log(LogLevel::Warn,    m); }
    void error(const std::string& m)   { log(LogLevel::Error,   m); }

    void file(const std::wstring& path);  
    void flushTo(Sink s);                 
private:
    Logger() = default;
    std::mutex mtx_;
    std::deque<LogEntry> history_;
    Sink sink_;
    std::wstring filePath_;
    HANDLE fileHandle_ = INVALID_HANDLE_VALUE;
};

#define LOG_INFO(m)    ::OSA::Logger::instance().info(m)
#define LOG_SUCCESS(m) ::OSA::Logger::instance().success(m)
#define LOG_WARN(m)    ::OSA::Logger::instance().warn(m)
#define LOG_ERROR(m)   ::OSA::Logger::instance().error(m)

} 
