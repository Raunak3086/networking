#pragma once

#include <mutex>
#include <cstdio>
#include <cstdarg>
#include <chrono>

enum class LogLevel { QUIET = 0, NORMAL = 1, VERBOSE = 2 };

class Logger {
public:
    static Logger& instance() {
        static Logger logger;
        return logger;
    }
    
    static void set_level(LogLevel level) {
        instance().set_level_impl(level);
    }
    
    void set_level_impl(LogLevel level) {
        std::lock_guard<std::mutex> lock(mtx_);
        level_ = level;
    }
    
    LogLevel get_level() const {
        return level_;
    }
    
    void info(const char* fmt, ...) {
        if (level_ < LogLevel::NORMAL) return;
        std::lock_guard<std::mutex> lock(mtx_);
        print_time();
        va_list args;
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
        fprintf(stderr, "\n");
    }
    
    void verbose(const char* fmt, ...) {
        if (level_ < LogLevel::VERBOSE) return;
        std::lock_guard<std::mutex> lock(mtx_);
        print_time();
        va_list args;
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
        fprintf(stderr, "\n");
    }
    
    void error(const char* fmt, ...) {
        std::lock_guard<std::mutex> lock(mtx_);
        print_time();
        va_list args;
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
        fprintf(stderr, "\n");
    }
    
private:
    Logger() = default;
    LogLevel level_ = LogLevel::NORMAL;
    std::mutex mtx_;
    
    void print_time() {
        auto now = std::chrono::system_clock::now();
        time_t t = std::chrono::system_clock::to_time_t(now);
        struct tm tm_info;
#ifdef _WIN32
        localtime_s(&tm_info, &t);
#else
        localtime_r(&t, &tm_info);
#endif
        fprintf(stderr, "[%02d:%02d:%02d] ", tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec);
    }
};

#define LOG_INFO(...) Logger::instance().info(__VA_ARGS__)
#define LOG_VERBOSE(...) Logger::instance().verbose(__VA_ARGS__)
#define LOG_ERROR(...) Logger::instance().error(__VA_ARGS__)
