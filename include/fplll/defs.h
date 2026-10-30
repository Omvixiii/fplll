// SPDX-FileCopyrightText: 2005-2008 Damien Stehle
// SPDX-FileCopyrightText: 2007 David Cade
// SPDX-FileCopyrightText: 2011 Xavier Pujol
// SPDX-FileCopyrightText: 2013 Damien Stehle
// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef FPLLL_DEFS_H
#define FPLLL_DEFS_H

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <iosfwd>
#include <optional>
#include <source_location>
#include <stdexcept>
#include <string_view>
#include <utility>

#include <fplll/fplll_config.h>

namespace fplll
{

/** Error raised when a precondition or a runtime invariant of the library is
    violated.  All library errors derive from std::runtime_error, so callers
    that do not care about the concrete type can catch std::exception. */
class Error : public std::runtime_error
{
public:
  using std::runtime_error::runtime_error;
};

namespace detail
{

/** Reports a violated precondition and throws fplll::Error.  The location is
    passed first by the FPLLL_CHECK/FPLLL_ABORT macros, which capture the
    caller's source location. */
template <class... Args>
[[noreturn]] inline void fail(const std::source_location &location,
                              std::format_string<Args...> format, Args &&...args)
{
  throw Error(std::format("fplll: {} ({}:{})", std::format(format, std::forward<Args>(args)...),
                          location.file_name(), location.line()));
}

/** Reports a failed internal debug check and aborts: a failing debug check is
    a bug in fplll, not a recoverable error. */
[[noreturn]] inline void debug_check_failed(const char *expression,
                                            const std::source_location &location)
{
  // The abort helper stays on <cstdio>; <print> is not universally available.
  // NOLINTNEXTLINE(modernize-use-std-print)
  std::fprintf(stderr, "fplll: internal debug check failed: %s\n  at %s:%u in %s\n", expression,
               location.file_name(), location.line(), location.function_name());
  std::abort();
}

}  // namespace detail

/** Logging stream for library diagnostics: discards by default. */
std::ostream &log_stream();
/** Redirects library diagnostics; nullptr restores the default discarding sink. */
void set_log_stream(std::ostream *stream);

/** Default floating-point type used inside the enumeration loop. */
using enumf = double;

/* Numeric limits shared by the reductions. */
inline constexpr double MAX_LONG_FAST = 9007199254740992.0;
inline constexpr int MAX_EXP_DOUBLE   = 1000;
inline constexpr int PREC_DOUBLE      = 53;
inline constexpr int PREC_DD          = 106;
inline constexpr int PREC_QD          = 212;

/* Default parameters of the reductions. */
inline constexpr double LLL_DEF_DELTA        = 0.99;
inline constexpr double LLL_DEF_ETA          = 0.51;
inline constexpr double LLL_DEF_EPSILON      = 0.01;
inline constexpr int SIZE_RED_FAILURE_THRESH = 5;

// Constraint: 1/2 < eta - theta
inline constexpr double HLLL_DEF_THETA = 0.001;
// Constant for the size reduction.
inline constexpr double HLLL_DEF_C = 0.1;

inline constexpr double BKZ_DEF_AUTO_ABORT_SCALE        = 1.0;
inline constexpr int BKZ_DEF_AUTO_ABORT_MAX_NO_DEC      = 5;
inline constexpr double BKZ_DEF_GH_FACTOR               = 1.1;
inline constexpr double BKZ_DEF_MIN_SUCCESS_PROBABILITY = 0.5;
inline constexpr int BKZ_DEF_RERANDOMIZATION_DENSITY    = 3;

/** Outcome of a reduction or of a solver call. */
enum class RedStatus : std::uint8_t
{
  success = 0,
  gso_failure,
  babai_failure,
  lll_failure,
  enum_failure,
  bkz_failure,
  bkz_time_limit,
  bkz_loops_limit,
  hlll_failure,
  hlll_norm_failure,
  hlll_sr_failure,
};

[[nodiscard]] constexpr bool succeeded(RedStatus status) noexcept
{
  return status == RedStatus::success;
}

constexpr std::string_view to_string(RedStatus status) noexcept
{
  switch (status)
  {
  case RedStatus::success:
    return "success";
  case RedStatus::gso_failure:
    return "infinite number in GSO";
  case RedStatus::babai_failure:
    return "infinite loop in babai";
  case RedStatus::lll_failure:
    return "infinite loop in LLL";
  case RedStatus::enum_failure:
    return "error in SVP solver";
  case RedStatus::bkz_failure:
    return "error in BKZ";
  case RedStatus::bkz_time_limit:
    return "time limit exceeded in BKZ";
  case RedStatus::bkz_loops_limit:
    return "loops limit exceeded in BKZ";
  case RedStatus::hlll_failure:
    return "error in HLLL";
  case RedStatus::hlll_norm_failure:
    return "increase of the norm";
  case RedStatus::hlll_sr_failure:
    return "error in weak size reduction";
  }
  return "unknown status";
}

/** LLL and HLLL variants. */
enum class LLLMethod : std::uint8_t
{
  wrapper = 0,
  proved,
  heuristic,
  fast,
};

constexpr std::string_view to_string(LLLMethod method) noexcept
{
  switch (method)
  {
  case LLLMethod::wrapper:
    return "wrapper";
  case LLLMethod::proved:
    return "proved";
  case LLLMethod::heuristic:
    return "heuristic";
  case LLLMethod::fast:
    return "fast";
  }
  return "";
}

/** Floating-point backend used for the Gram-Schmidt computations. */
enum class FloatType : std::uint8_t
{
  automatic = 0,
  double_precision,
  dd,
  qd,
  mpfr,
};

constexpr std::string_view to_string(FloatType type) noexcept
{
  switch (type)
  {
  case FloatType::automatic:
    return "automatic";
  case FloatType::double_precision:
    return "double";
  case FloatType::dd:
    return "dd";
  case FloatType::qd:
    return "qd";
  case FloatType::mpfr:
    return "mpfr";
  }
  return "";
}

/** True when the build supports the requested backend. */
constexpr bool is_supported([[maybe_unused]] FloatType type) noexcept
{
  switch (type)
  {
  case FloatType::qd:
  case FloatType::dd:
#ifdef FPLLL_WITH_QD
    return true;
#else
    return false;
#endif
  default:
    return true;
  }
}

/**
   Detailed outcome of a reduction.

   A reduction may stop early (time/loop limits) or fail numerically; the
   report keeps the reason (`status`), the configuration that was actually used
   (`float_type`, `precision`), whether the produced basis carries the
   mathematical guarantee of the requested proved method (`guaranteed`) and the
   statistics the specific reduction can report (`tours`, `nodes`).

   When a reduction returns normally, the transformation matrices (if any) and
   the reduced basis always satisfy `B_out = U B_in`, even when `status` is not
   `RedStatus::success`; a failure means the basis may not be fully reduced,
   not that the reported transformation is inconsistent.
*/
struct ReductionReport
{
  /** Outcome of the reduction. */
  RedStatus status = RedStatus::success;
  /** Floating-point backend actually used.  `FloatType::automatic` is only
      reported when no backend was needed, e.g. for an empty basis. */
  FloatType float_type = FloatType::automatic;
  /** Precision the backend actually ran at; 0 when no backend was used. */
  int precision = 0;
  /** True when the returned basis carries the mathematical guarantee of the
      requested proved method at sufficient precision.  BKZ and HKZ always
      report false: they do not provide a certificate. */
  bool guaranteed = false;
  /** Number of completed tours; only BKZ and HKZ report it. */
  std::optional<long> tours;
  /** Number of enumeration nodes visited; only BKZ and HKZ report it. */
  std::optional<long> nodes;
};

/** True when the reduction in `report` succeeded. */
[[nodiscard]] constexpr bool succeeded(const ReductionReport &report) noexcept
{
  return succeeded(report.status);
}

/** Human-readable name of the status in `report`. */
constexpr std::string_view to_string(const ReductionReport &report) noexcept
{
  return to_string(report.status);
}

/** SVP solver variants. */
enum class SVPMethod : std::uint8_t
{
  fast = 0,
  proved,
};

/** CVP solver variants. */
enum class CVPMethod : std::uint8_t
{
  fast = 0,
  proved,
};

/** Objective of the pruning-parameter optimizer. */
enum class PrunerMetric : std::uint8_t
{
  probability_of_shortest = 0,
  expected_solutions,
};

}  // namespace fplll

/** Checks a precondition; on failure throws fplll::Error with the given
    std::format message. */
#define FPLLL_CHECK(condition, ...)                                                                \
  do                                                                                               \
  {                                                                                                \
    if (!(condition))                                                                              \
      ::fplll::detail::fail(std::source_location::current(), __VA_ARGS__);                         \
  } while (false)

/** Unconditional precondition failure; throws fplll::Error. */
#define FPLLL_ABORT(...) ::fplll::detail::fail(std::source_location::current(), __VA_ARGS__)

/** Checks a compiled-in internal invariant; on failure aborts. */
#ifdef FPLLL_DEBUG_CHECKS
#define FPLLL_DEBUG_CHECK(expression)                                                              \
  do                                                                                               \
  {                                                                                                \
    if (!(expression))                                                                             \
      ::fplll::detail::debug_check_failed(#expression, std::source_location::current());           \
  } while (false)
#else
#define FPLLL_DEBUG_CHECK(expression) ((void)0)
#endif

#endif
