#include "Logger.h"

namespace winuxlink {

// 静态成员初始化
LogLevel Logger::s_level = LogLevel::Info;
std::mutex Logger::s_mutex;
std::ofstream Logger::s_file;
bool Logger::s_ansiEnabled = false;

// ANSI 颜色码 (跨平台)
#define COLOR_RESET   "\033[0m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BRIGHT_YELLOW "\033[93m"
#define COLOR_BRIGHT_RED    "\033[91m"

void Logger::enableAnsiColors() {
#ifdef _WIN32
    // Windows 10+ 控制台启用虚拟终端序列
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
            s_ansiEnabled = true;
        }
    }
#else
    // Linux/macOS 默认支持
    s_ansiEnabled = true;
#endif
}

void Logger::setLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_level = level;
}

void Logger::setLogFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_file.is_open()) {
        s_file.close();
    }
    s_file.open(filename, std::ios::out | std::ios::app);
}

void Logger::debug(const std::string& msg)   { log(LogLevel::Debug, msg); }
void Logger::info(const std::string& msg)    { log(LogLevel::Info, msg); }
void Logger::warning(const std::string& msg) { log(LogLevel::Warning, msg); }
void Logger::error(const std::string& msg)   { log(LogLevel::Error, msg); }

void Logger::log(LogLevel level, const std::string& msg) {
    // 等级过滤
    if (level < s_level) return;

    // 启用 ANSI (延迟到第一次调用日志时，避免静态初始化顺序问题)
    static std::once_flag initFlag;
    std::call_once(initFlag, []() { enableAnsiColors(); });

    std::lock_guard<std::mutex> lock(s_mutex);
    std::string timestamp = getTimestamp();
    std::string levelStr = levelToString(level);
    std::string entry = timestamp + " [" + levelStr + "] " + msg;

    // 控制台输出 (带颜色)
    const char* color = COLOR_RESET;
    if (s_ansiEnabled) {
        switch (level) {
            case LogLevel::Debug:   color = COLOR_CYAN;          break;
            case LogLevel::Info:    color = COLOR_GREEN;         break;
            case LogLevel::Warning: color = COLOR_BRIGHT_YELLOW; break;
            case LogLevel::Error:   color = COLOR_BRIGHT_RED;    break;
        }
    }

    std::ostream& console = (level >= LogLevel::Warning) ? std::cerr : std::cout;
    // 控制台输出 (带颜色)，并且颜色只影响当前输出，后续为无颜色
    console << color << entry << COLOR_RESET << std::endl;

    // 文件输出 (无颜色)
    if (s_file.is_open()) {
        s_file << entry << std::endl;
        s_file.flush(); // 确保立即写入，便于崩溃时保留日志
    }
}

std::string Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Debug:   return "DEBUG";
        case LogLevel::Info:    return "INFO";
        case LogLevel::Warning: return "WARN";
        case LogLevel::Error:   return "ERROR";
    }
    return "UNKNOWN";
}

std::string Logger::getTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) % 1000;

    std::tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &time_t);
#else
    localtime_r(&time_t, &tm_buf);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}


#ifdef _WIN32
    std::string Logger::getLastErrorString() {
        DWORD err = WSAGetLastError();
        if (err == 0) return "No error";
        char* msg = nullptr;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&msg, 0, nullptr);
        std::string result(msg ? msg : "Unknown error");
        LocalFree(msg);
        return result + " (code: " + std::to_string(err) + ")";
    }
#else
    std::string Logger::getLastErrorString() {
        int err = errno;
        return std::string(strerror(err)) + " (code: " + std::to_string(err) + ")";
    }
#endif

} // namespace winuxlink