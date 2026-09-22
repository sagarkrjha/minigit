#include "logger.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <iomanip>

namespace minigit::core {

namespace {

std::string to_lower_copy(std::string_view s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s)
    {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

const char* get_env_var(const char* name)
{
#if defined(_MSC_VER)
    char* buf = nullptr;
    size_t sz = 0;
    if (_dupenv_s(&buf, &sz, name) == 0 && buf != nullptr)
    {
        static thread_local std::string s_val;
        s_val = buf;
        free(buf);
        return s_val.c_str();
    }
    return nullptr;
#else
    return std::getenv(name);
#endif
}

} // namespace

std::string_view to_string(LogLevel level)
{
    switch (level)
    {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::OFF:   return "OFF";
    }
    return "UNKNOWN";
}

LogLevel log_level_from_string(std::string_view str)
{
    std::string lower = to_lower_copy(str);
    if (lower == "trace")
        return LogLevel::TRACE;
    if (lower == "debug")
        return LogLevel::DEBUG;
    if (lower == "info")
        return LogLevel::INFO;
    if (lower == "warn" || lower == "warning")
        return LogLevel::WARN;
    if (lower == "error")
        return LogLevel::ERROR;
    if (lower == "off")
        return LogLevel::OFF;
    return LogLevel::OFF;
}

Logger::Logger()
    : level_(LogLevel::OFF),
      out_stream_(&std::cerr),
      file_stream_(nullptr),
      file_path_()
{
}

Logger::~Logger() = default;

Logger& Logger::instance()
{
    static Logger s_instance;
    return s_instance;
}

void Logger::set_level(LogLevel level)
{
    std::lock_guard<std::mutex> lock(mutex_);
    level_ = level;
}

LogLevel Logger::level() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return level_;
}

bool Logger::is_enabled(LogLevel level) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (level_ == LogLevel::OFF)
        return false;
    return static_cast<uint8_t>(level) >= static_cast<uint8_t>(level_);
}

void Logger::set_output_stream(std::ostream* os)
{
    std::lock_guard<std::mutex> lock(mutex_);
    file_stream_.reset();
    file_path_.clear();
    out_stream_ = os ? os : &std::cerr;
}

bool Logger::set_output_file(const std::string& path)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto new_file = std::make_unique<std::ofstream>(path, std::ios::out | std::ios::app);
    if (!new_file->is_open())
    {
        return false;
    }
    file_path_ = path;
    file_stream_ = std::move(new_file);
    out_stream_ = file_stream_.get();
    return true;
}

std::string Logger::output_file() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return file_path_;
}

void Logger::init_from_env()
{
    const char* log_file_env = get_env_var("MINIGIT_LOG_FILE");
    if (log_file_env && *log_file_env != '\0')
    {
        set_output_file(log_file_env);
    }

    const char* log_level_env = get_env_var("MINIGIT_LOG_LEVEL");
    bool explicit_level = false;
    if (log_level_env && *log_level_env != '\0')
    {
        set_level(log_level_from_string(log_level_env));
        explicit_level = true;
    }

    const char* trace_env = get_env_var("MINIGIT_TRACE");
    if (trace_env && *trace_env != '\0')
    {
        std::string val = to_lower_copy(trace_env);
        if (val == "1" || val == "true" || val == "stderr")
        {
            if (!explicit_level)
                set_level(LogLevel::DEBUG);
            if (!log_file_env)
                set_output_stream(&std::cerr);
        }
        else if (val == "2")
        {
            if (!explicit_level)
                set_level(LogLevel::TRACE);
            if (!log_file_env)
                set_output_stream(&std::cerr);
        }
        else if (val == "stdout")
        {
            if (!explicit_level)
                set_level(LogLevel::DEBUG);
            if (!log_file_env)
                set_output_stream(&std::cout);
        }
        else if (val != "0" && val != "false" && val != "off")
        {
            // Treat as file path
            if (set_output_file(trace_env))
            {
                if (!explicit_level)
                    set_level(LogLevel::DEBUG);
            }
        }
    }
}

void Logger::reset()
{
    std::lock_guard<std::mutex> lock(mutex_);
    level_ = LogLevel::OFF;
    file_stream_.reset();
    file_path_.clear();
    out_stream_ = &std::cerr;
}

std::string Logger::current_timestamp()
{
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    auto timer = std::chrono::system_clock::to_time_t(now);

    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &timer);
#else
    localtime_r(&timer, &tm_buf);
#endif

    char buf[64];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
                  tm_buf.tm_year + 1900,
                  tm_buf.tm_mon + 1,
                  tm_buf.tm_mday,
                  tm_buf.tm_hour,
                  tm_buf.tm_min,
                  tm_buf.tm_sec,
                  static_cast<int>(ms.count()));
    return std::string(buf);
}

void Logger::log(LogLevel level, std::string_view subsystem, std::string_view message)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (level_ == LogLevel::OFF || static_cast<uint8_t>(level) < static_cast<uint8_t>(level_))
    {
        return;
    }

    if (!out_stream_)
    {
        return;
    }

    *out_stream_ << "[" << current_timestamp() << "] "
                 << "[" << to_string(level) << "] "
                 << "[" << subsystem << "] "
                 << message << "\n";
    out_stream_->flush();
}

} // namespace minigit::core
