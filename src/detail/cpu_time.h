// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef FPLLL_DETAIL_CPU_TIME_H
#define FPLLL_DETAIL_CPU_TIME_H

#include <chrono>
#include <cstdint>
#include <ctime>

// clock_gettime()/CLOCK_PROCESS_CPUTIME_ID are POSIX; on Apple platforms they
// are declared in the SDK's <_time.h> (which <time.h> only re-exports).
#if defined(__APPLE__)
#include <_time.h>
#elif defined(__linux__)
#include <time.h>
#endif

namespace fplll::detail
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
  const auto clocks     = static_cast<std::int64_t>(std::clock());
  const auto per_second = static_cast<std::int64_t>(CLOCKS_PER_SEC);
  return clocks * 1000 / per_second;
}

/** Process CPU time as a chrono duration, so callers can compare it against a
    `std::chrono::milliseconds` budget without repeating the unit conversion.
    CPU time is used (rather than wall-clock time) so that a run is charged for
    the parallel workers it keeps busy. */
inline std::chrono::milliseconds cpu_time()
{
  return std::chrono::milliseconds(cpu_time_milliseconds());
}

inline double cpu_time_seconds() { return static_cast<double>(cpu_time_milliseconds()) / 1000.0; }

}  // namespace fplll::detail

#endif
