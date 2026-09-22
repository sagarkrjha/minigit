#pragma once

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>

namespace minigit::core {

enum class LogLevel : uint8_t {
    TRACE = 0,
    DEBUG = 1,
    INFO  = 2,
    WARN  = 3,
    ERROR = 4,
    OFF   = 5
};

std::string_view to_string(LogLevel level);
LogLevel log_level_from_string(std::string_view str);

class Logger {
public:
    static Logger& instance();

    // Log level configuration
    void set_level(LogLevel level);
    LogLevel level() const;
    bool is_enabled(LogLevel level) const;

    // Output target configuration
    void set_output_stream(std::ostream* os);
    bool set_output_file(const std::string& path);
    std::string output_file() const;

    // Initialize from environment variables:
    // - MINIGIT_TRACE: "1"/"true"/"stderr" (DEBUG), "2" (TRACE), or file path
    // - MINIGIT_LOG_LEVEL: "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "OFF"
    // - MINIGIT_LOG_FILE: destination log file path
    void init_from_env();

    // Reset logger state to default (OFF, std::cerr, no file)
    void reset();

    // Log message formatted with timestamp, level, and subsystem tag
    void log(LogLevel level, std::string_view subsystem, std::string_view message);

    // Current formatted timestamp [YYYY-MM-DD HH:MM:SS.mmm]
    static std::string current_timestamp();

private:
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    LogLevel level_{LogLevel::OFF};
    std::ostream* out_stream_{&std::cerr};
    std::unique_ptr<std::ofstream> file_stream_{nullptr};
    std::string file_path_{};
    mutable std::mutex mutex_{};
};

} // namespace minigit::core

#define LOG_ENABLED(level) (::minigit::core::Logger::instance().is_enabled(level))

#define MINIGIT_LOG(level, subsystem, expr)                                \
    do {                                                                   \
        if (LOG_ENABLED(level)) {                                          \
            std::ostringstream _minigit_log_ss;                            \
            _minigit_log_ss << expr;                                       \
            ::minigit::core::Logger::instance().log(                       \
                level, subsystem, _minigit_log_ss.str());                  \
        }                                                                  \
    } while (false)

#define LOG_TRACE(subsystem, expr) MINIGIT_LOG(::minigit::core::LogLevel::TRACE, subsystem, expr)
#define LOG_DEBUG(subsystem, expr) MINIGIT_LOG(::minigit::core::LogLevel::DEBUG, subsystem, expr)
#define LOG_INFO(subsystem, expr)  MINIGIT_LOG(::minigit::core::LogLevel::INFO,  subsystem, expr)
#define LOG_WARN(subsystem, expr)  MINIGIT_LOG(::minigit::core::LogLevel::WARN,  subsystem, expr)
#define LOG_ERROR(subsystem, expr) MINIGIT_LOG(::minigit::core::LogLevel::ERROR, subsystem, expr)
