#pragma once

#include <sstream>

#define LOG_BASE(msg, level)                                                   \
    {                                                                          \
        std::ostringstream logstream;                                          \
        logstream << msg;                                                      \
        hestia::Logger::get_instance().log_line(                               \
            level, logstream, __FILE__, __FUNCTION__, __LINE__);               \
    };

#define LOG_ERROR(msg) LOG_BASE(msg, hestia::Logger::Level::ERROR);
#define LOG_WARN(msg) LOG_BASE(msg, hestia::Logger::Level::WARN);
#define LOG_INFO(msg) LOG_BASE(msg, hestia::Logger::Level::INFO);
#define LOG_DEBUG(msg) LOG_BASE(msg, hestia::Logger::Level::DEBUG);

namespace hestia {

class Logger {
  public:
    enum class Level { DEBUG, INFO, WARN, ERROR };

    struct Config {
        std::string m_log_file_path;
        std::string m_log_prefix;
        bool m_active{true};
        bool m_assert{true};
        bool m_console_only{false};
        Level m_level{Level::INFO};
    };

    void initialize(const Config& config);

    static Logger& get_instance();

    void log_line(
        Level level,
        const std::ostringstream& line,
        const std::string& file_name     = "",
        const std::string& function_name = "",
        int line_number                  = -1);

  private:
    Config m_config;
};
}  // namespace hestia