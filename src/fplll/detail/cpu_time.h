#ifndef FPLLL_DETAIL_CPU_TIME_H
#define FPLLL_DETAIL_CPU_TIME_H

#include <chrono>
#include <cstdint>
#include <ctime>

#if defined(__linux__) || defined(__APPLE__)
#include <time.h>
#endif

namespace fplll
{
namespace detail
{

/** Process CPU time in milliseconds.  This replaces the POSIX-only cputime(). */
inline std::int64_t cpu_time_milliseconds()
{
#if defined(__linux__) || defined(__APPLE__)
  timespec time_value{};
  if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time_value) == 0)
  {
    return static_cast<std::int64_t>(time_value.tv_sec) * 1000 +
           static_cast<std::int64_t>(time_value.tv_nsec) / 1000000;
  }
#endif
  return static_cast<std::int64_t>(std::clock()) * 1000 / CLOCKS_PER_SEC;
}

inline double cpu_time_seconds() { return static_cast<double>(cpu_time_milliseconds()) / 1000.0; }

}  // namespace detail
}  // namespace fplll

#endif
