#include "test_framework.h"
#include "core/logger.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <vector>

using namespace minigit::core;

TEST_CASE(Logger, LogLevelToStringAndParsing)
{
    ASSERT_EQ(to_string(LogLevel::TRACE), "TRACE");
    ASSERT_EQ(to_string(LogLevel::DEBUG), "DEBUG");
    ASSERT_EQ(to_string(LogLevel::INFO), "INFO");
    ASSERT_EQ(to_string(LogLevel::WARN), "WARN");
    ASSERT_EQ(to_string(LogLevel::ERROR), "ERROR");
    ASSERT_EQ(to_string(LogLevel::OFF), "OFF");

    ASSERT_TRUE(log_level_from_string("trace") == LogLevel::TRACE);
    ASSERT_TRUE(log_level_from_string("TRACE") == LogLevel::TRACE);
    ASSERT_TRUE(log_level_from_string("debug") == LogLevel::DEBUG);
    ASSERT_TRUE(log_level_from_string("info") == LogLevel::INFO);
    ASSERT_TRUE(log_level_from_string("warn") == LogLevel::WARN);
    ASSERT_TRUE(log_level_from_string("warning") == LogLevel::WARN);
    ASSERT_TRUE(log_level_from_string("error") == LogLevel::ERROR);
    ASSERT_TRUE(log_level_from_string("off") == LogLevel::OFF);
    ASSERT_TRUE(log_level_from_string("unknown_level") == LogLevel::OFF);
}

TEST_CASE(Logger, DefaultStateIsOff)
{
    auto& logger = Logger::instance();
    logger.reset();

    ASSERT_TRUE(logger.level() == LogLevel::OFF);
    ASSERT_FALSE(logger.is_enabled(LogLevel::TRACE));
    ASSERT_FALSE(logger.is_enabled(LogLevel::DEBUG));
    ASSERT_FALSE(logger.is_enabled(LogLevel::INFO));
    ASSERT_FALSE(logger.is_enabled(LogLevel::WARN));
    ASSERT_FALSE(logger.is_enabled(LogLevel::ERROR));

    std::ostringstream ss;
    logger.set_output_stream(&ss);
    logger.log(LogLevel::ERROR, "test", "should not appear");
    ASSERT_TRUE(ss.str().empty());
}

TEST_CASE(Logger, LevelFiltering)
{
    auto& logger = Logger::instance();
    logger.reset();

    std::ostringstream ss;
    logger.set_output_stream(&ss);
    logger.set_level(LogLevel::WARN);

    ASSERT_FALSE(logger.is_enabled(LogLevel::TRACE));
    ASSERT_FALSE(logger.is_enabled(LogLevel::DEBUG));
    ASSERT_FALSE(logger.is_enabled(LogLevel::INFO));
    ASSERT_TRUE(logger.is_enabled(LogLevel::WARN));
    ASSERT_TRUE(logger.is_enabled(LogLevel::ERROR));

    LOG_TRACE("core", "trace message");
    LOG_DEBUG("core", "debug message");
    LOG_INFO("core", "info message");
    ASSERT_TRUE(ss.str().empty());

    LOG_WARN("core", "warn message");
    LOG_ERROR("core", "error message");

    std::string out = ss.str();
    ASSERT_TRUE(out.find("[WARN]") != std::string::npos);
    ASSERT_TRUE(out.find("warn message") != std::string::npos);
    ASSERT_TRUE(out.find("[ERROR]") != std::string::npos);
    ASSERT_TRUE(out.find("error message") != std::string::npos);
    ASSERT_TRUE(out.find("trace message") == std::string::npos);
    ASSERT_TRUE(out.find("debug message") == std::string::npos);
    ASSERT_TRUE(out.find("info message") == std::string::npos);

    logger.reset();
}

TEST_CASE(Logger, MessageFormatting)
{
    auto& logger = Logger::instance();
    logger.reset();

    std::ostringstream ss;
    logger.set_output_stream(&ss);
    logger.set_level(LogLevel::DEBUG);

    LOG_DEBUG("storage", "reading CAS object 4a8b2c");

    std::string out = ss.str();
    ASSERT_FALSE(out.empty());
    ASSERT_TRUE(out.front() == '[');
    ASSERT_TRUE(out.find("[DEBUG]") != std::string::npos);
    ASSERT_TRUE(out.find("[storage]") != std::string::npos);
    ASSERT_TRUE(out.find("reading CAS object 4a8b2c\n") != std::string::npos);

    logger.reset();
}

TEST_CASE(Logger, FastPathZeroCostWhenDisabled)
{
    auto& logger = Logger::instance();
    logger.reset(); // OFF

    int side_effect = 0;
    LOG_TRACE("bench", (side_effect++, "evaluated"));
    LOG_DEBUG("bench", (side_effect++, "evaluated"));
    LOG_INFO("bench", (side_effect++, "evaluated"));
    LOG_WARN("bench", (side_effect++, "evaluated"));
    LOG_ERROR("bench", (side_effect++, "evaluated"));

    ASSERT_EQ(side_effect, 0);
}

TEST_CASE(Logger, FileLogging)
{
    auto& logger = Logger::instance();
    logger.reset();

    std::filesystem::path log_path = std::filesystem::temp_directory_path() / "minigit_unit_test.log";
    if (std::filesystem::exists(log_path))
    {
        std::filesystem::remove(log_path);
    }

    bool opened = logger.set_output_file(log_path.string());
    ASSERT_TRUE(opened);
    ASSERT_EQ(logger.output_file(), log_path.string());

    logger.set_level(LogLevel::INFO);
    LOG_INFO("network", "HTTP request initiated");
    LOG_WARN("network", "Retrying after 503 response");

    logger.reset(); // Closes file handle

    ASSERT_TRUE(std::filesystem::exists(log_path));
    std::ifstream file(log_path);
    ASSERT_TRUE(file.is_open());
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    ASSERT_TRUE(content.find("[INFO]") != std::string::npos);
    ASSERT_TRUE(content.find("[network]") != std::string::npos);
    ASSERT_TRUE(content.find("HTTP request initiated") != std::string::npos);
    ASSERT_TRUE(content.find("[WARN]") != std::string::npos);
    ASSERT_TRUE(content.find("Retrying after 503 response") != std::string::npos);

    file.close();
    std::filesystem::remove(log_path);
}

TEST_CASE(Logger, ThreadSafety)
{
    auto& logger = Logger::instance();
    logger.reset();

    std::ostringstream ss;
    logger.set_output_stream(&ss);
    logger.set_level(LogLevel::TRACE);

    constexpr int kThreads = 8;
    constexpr int kLogsPerThread = 50;
    std::vector<std::thread> threads;
    threads.reserve(kThreads);

    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back([t]() {
            for (int i = 0; i < kLogsPerThread; ++i)
            {
                LOG_TRACE("thread_test", "thread " << t << " iteration " << i);
            }
        });
    }

    for (auto& th : threads)
    {
        th.join();
    }

    std::string out = ss.str();
    int count = 0;
    size_t pos = 0;
    while ((pos = out.find("[thread_test]", pos)) != std::string::npos)
    {
        count++;
        pos += 13;
    }

    ASSERT_EQ(count, kThreads * kLogsPerThread);
    logger.reset();
}

TEST_CASE(Logger, CurrentTimestampFormat)
{
    std::string ts = Logger::current_timestamp();
    // Expected format: YYYY-MM-DD HH:MM:SS.mmm (23 chars)
    ASSERT_EQ(ts.size(), 23);
    ASSERT_EQ(ts[4], '-');
    ASSERT_EQ(ts[7], '-');
    ASSERT_EQ(ts[10], ' ');
    ASSERT_EQ(ts[13], ':');
    ASSERT_EQ(ts[16], ':');
    ASSERT_EQ(ts[19], '.');
}
