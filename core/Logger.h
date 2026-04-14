#pragma once

#include <string>
#include <mutex>
#include <fstream>
#include <sstream>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <cstring>


namespace winuxlink {

#ifdef _WIN32
    #include <windows.h>
#endif



enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

class Logger {
public:
    // 设置全局日志等级 (低于此等级的日志将被忽略)
    static void setLevel(LogLevel level);

    // 设置日志输出文件 (不调用则仅输出到控制台)
    static void setLogFile(const std::string& filename);

    // 日志接口
    static void debug(const std::string& msg);
    static void info(const std::string& msg);
    static void warning(const std::string& msg);
    static void error(const std::string& msg);

    static std::string getLastErrorString();

private:
    static void log(LogLevel level, const std::string& msg);
    static std::string levelToString(LogLevel level);
    static std::string getTimestamp();
    static void enableAnsiColors();

    static LogLevel s_level;
    static std::mutex s_mutex;
    static std::ofstream s_file;
    static bool s_ansiEnabled;
};

// 便捷宏 (可选)
#ifdef _NW_DEBUG
    #define LOG_DEBUG(msg)   winuxlink::Logger::debug(msg + winuxlink::Logger::getLastErrorString())
#else
    #define LOG_DEBUG(msg)   ((void)0)
#endif
#define LOG_INFO(msg)    winuxlink::Logger::info(msg)
#define LOG_WARNING(msg) winuxlink::Logger::warning(msg)
#define LOG_ERROR(msg)   winuxlink::Logger::error(msg + winuxlink::Logger::getLastErrorString())

} // namespace winuxlink