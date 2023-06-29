#pragma once

#include <ctime>
#include <string>

namespace hestia {
class TimeUtils {
  public:
    static std::time_t get_current_time();
    static std::string get_current_time_hr();
};
}  // namespace hestia
