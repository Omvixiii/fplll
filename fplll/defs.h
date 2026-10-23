/* Copyright (C) 2005-2008 Damien Stehle.
   Copyright (C) 2007 David Cade.
   Copyright (C) 2011 Xavier Pujol.
   Copyright (C) 2013 Damien Stehle

   This file is part of fplll. fplll is free software: you
   can redistribute it and/or modify it under the terms of the GNU Lesser
   General Public License as published by the Free Software Foundation,
   either version 2.1 of the License, or (at your option) any later version.

   fplll is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with fplll. If not, see <http://www.gnu.org/licenses/>. */

#ifndef FPLLL_DEFS_H
#define FPLLL_DEFS_H

#if !defined(__CYGWIN__) && !(defined(__APPLE__) && defined(__aarch64__))
#define FPLLL_WITH_LONG_DOUBLE
#endif

#define FPLLL_WITH_ZLONG

#include <algorithm>
#include <array>
#include <atomic>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <functional>
#include <future>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <fplll/fplll_config.h>
#include <gmp.h>
#include <mpfr.h>

#if defined(__sun) || defined(__CYGWIN__)
#include <ieeefp.h>
extern "C" long double ldexpl(long double x, int exp);
#ifndef NAN
#define NAN __builtin_nanf("")
#endif
#endif

namespace fplll
{
/** Stream used for library diagnostics; discards by default. */
std::ostream &log_stream();
/** Redirects library diagnostics; nullptr restores the default discarding sink. */
void set_log_stream(std::ostream *stream);
}  // namespace fplll

#define FPLLL_INFO(x)                                                                              \
  {                                                                                                \
    ::fplll::log_stream() << x << '\n';                                                            \
  }
#define FPLLL_ABORT(x)                                                                             \
  {                                                                                                \
    cerr << "fplll: " << x << '\n';                                                                \
    abort();                                                                                       \
  }
#define FPLLL_CHECK(x, y)                                                                          \
  {                                                                                                \
    if (!(x))                                                                                      \
      FPLLL_ABORT(y);                                                                              \
  }

#ifdef DEBUG
#include <cassert>
extern int debug_depth;
#define FPLLL_TRACE(x) std::cerr << "TRACE: " << std::string(debug_depth * 2, ' ') << x << '\n'
struct DebugTracer
{
  DebugTracer(const char *f) : f(f) { debug_depth++; }
  ~DebugTracer() noexcept
  {
    debug_depth--;
    FPLLL_TRACE("</" << f << ">");
  }
  std::string f;
};
#define FPLLL_DEBUG_ABORT(x) FPLLL_ABORT(x)
#define FPLLL_DEBUG_CHECK(x) assert(x);
#define FPLLL_TRACE_IN(x)                                                                          \
  FPLLL_TRACE("<" << __func__ << " " << x << ">");                                                 \
  DebugTracer debugTracer(__func__);
#else
#define FPLLL_DEBUG_ABORT(x)
#define FPLLL_DEBUG_CHECK(x)
#define FPLLL_TRACE(x)
#define FPLLL_TRACE_IN(x)
#endif

#define FPLLL_BEGIN_NAMESPACE                                                                      \
  namespace fplll                                                                                  \
  {
#define FPLLL_END_NAMESPACE }

/** \namespace fplll
    The fplll namespace */
FPLLL_BEGIN_NAMESPACE

/* Explicitly re-export the standard-library names used by the public headers.
   This keeps the historical unqualified spelling working without importing all
   of namespace std.  A later pass qualifies the remaining uses. */
using std::abs;
using std::array;
using std::atomic;
using std::ceil;
using std::cerr;
using std::cin;
using std::copy;
using std::cos;
using std::cout;
using std::endl;
using std::exp;
using std::fabs;
using std::fill;
using std::find;
using std::floor;
using std::forward;
using std::function;
using std::future;
using std::get;
using std::getline;
using std::greater;
using std::ifstream;
using std::ios;
using std::ios_base;
using std::istream;
using std::istringstream;
using std::less;
using std::log;
using std::make_pair;
using std::make_unique;
using std::max;
using std::min;
using std::move;
using std::multimap;
using std::numeric_limits;
using std::ofstream;
using std::ostream;
using std::ostringstream;
using std::pair;
using std::pow;
using std::reverse;
using std::round;
using std::setfill;
using std::setprecision;
using std::setw;
using std::shared_ptr;
using std::sin;
using std::sort;
using std::sqrt;
using std::string;
using std::stringstream;
using std::swap;
using std::thread;
using std::to_string;
using std::tuple;
using std::unique;
using std::unique_ptr;
using std::vector;

/* this trick will not work on 16-bit machines*/
#if (LONG_MAX == 2147483647L)
inline constexpr double MAX_LONG_FAST = 1073741824.0;
#else
inline constexpr double MAX_LONG_FAST = 9007199254740992.0;
#endif

inline constexpr int MAX_EXP_DOUBLE = 1000;
inline constexpr int PREC_DOUBLE    = 53;
inline constexpr int PREC_DD        = 106;
inline constexpr int PREC_QD        = 212;

inline constexpr double LLL_DEF_DELTA        = 0.99;
inline constexpr double LLL_DEF_ETA          = 0.51;
inline constexpr double LLL_DEF_EPSILON      = 0.01;
inline constexpr int SIZE_RED_FAILURE_THRESH = 5;

// Constraint: 1/2 < eta - theta
inline constexpr double HLLL_DEF_THETA = 0.001;
// Constant for the size reduction.
inline constexpr double HLLL_DEF_C = 0.1;

enum RedStatus
{
  RED_SUCCESS = 0,
  // Skips value 1
  RED_GSO_FAILURE       = 2,
  RED_BABAI_FAILURE     = 3,
  RED_LLL_FAILURE       = 4,
  RED_ENUM_FAILURE      = 5,
  RED_BKZ_FAILURE       = 6,
  RED_BKZ_TIME_LIMIT    = 7,
  RED_BKZ_LOOPS_LIMIT   = 8,
  RED_HLLL_FAILURE      = 9,
  RED_HLLL_NORM_FAILURE = 10,
  RED_HLLL_SR_FAILURE   = 11,
  RED_URL_ERR           = 12,
  RED_STATUS_MAX        = 13
};

inline constexpr std::array<std::string_view, RED_STATUS_MAX> RED_STATUS_STR = {
    "success",
    "",
    "infinite number in GSO",
    "infinite loop in babai",
    "infinite loop in LLL",
    "error in SVP solver",
    "error in BKZ",
    "time limit exceeded in BKZ",
    "loops limit exceeded in BKZ",
    "error in HLLL",
    "increase of the norm",
    "error in weak size reduction",
    "Please see https://github.com/fplll/fplll/wiki/fplll-errors-FAQ for more information."};

enum class LLLMethod
{
  LM_WRAPPER   = 0,
  LM_PROVED    = 1,
  LM_HEURISTIC = 2,
  LM_FAST      = 3
};
using enum LLLMethod;

inline constexpr std::array<std::string_view, 4> LLL_METHOD_STR = {"wrapper", "proved", "heuristic",
                                                                   "fast"};

// LM_HEURISTIC is not (yet) an option for HLLL and cannot be called from the fplll binary, then
// we leave empty the third string.
inline constexpr std::array<std::string_view, 4> HLLL_METHOD_STR = {"wrapper", "proved", "",
                                                                    "fast"};

enum class IntType
{
  ZT_MPZ  = 0,
  ZT_LONG = 1
};
using enum IntType;

inline constexpr std::array<std::string_view, 2> INT_TYPE_STR = {"mpz", "long"};

enum class FloatType
{
  FT_DEFAULT     = 0,
  FT_DOUBLE      = 1,
  FT_LONG_DOUBLE = 2,
  FT_DD          = 3,
  FT_QD          = 4,
  FT_MPFR        = 5
};
using enum FloatType;

inline constexpr std::array<std::string_view, 6> FLOAT_TYPE_STR = {"",   "double", "long double",
                                                                   "dd", "qd",     "mpfr"};

/** Index an array with a scoped enum without implicit conversions. */
template <class Enum> constexpr std::size_t enum_index(Enum value)
{
  return static_cast<std::size_t>(value);
}

enum LLLFlags
{
  LLL_VERBOSE   = 1,
  LLL_EARLY_RED = 2,
  LLL_SIEGEL    = 4,
  LLL_DEFAULT   = 0
};

enum class SVPMethod
{
  SVPM_FAST   = 0,
  SVPM_PROVED = 2
};
using enum SVPMethod;

enum class CVPMethod
{
  CVPM_FAST   = 0,
  CVPM_PROVED = 2
};
using enum CVPMethod;

enum SVPFlags
{
  SVP_DEFAULT      = 0,
  SVP_VERBOSE      = 1,
  SVP_OVERRIDE_BND = 2,
  SVP_DUAL         = 4
};

enum CVPFlags
{
  CVP_DEFAULT = SVP_DEFAULT,
  CVP_VERBOSE = SVP_VERBOSE
};

inline constexpr double BKZ_DEF_AUTO_ABORT_SCALE        = 1.0;
inline constexpr int BKZ_DEF_AUTO_ABORT_MAX_NO_DEC      = 5;
inline constexpr double BKZ_DEF_GH_FACTOR               = 1.1;
inline constexpr double BKZ_DEF_MIN_SUCCESS_PROBABILITY = 0.5;
inline constexpr int BKZ_DEF_RERANDOMIZATION_DENSITY    = 3;

enum BKZFlags
{
  BKZ_DEFAULT     = 0,
  BKZ_VERBOSE     = 1,
  BKZ_NO_LLL      = 2,
  BKZ_MAX_LOOPS   = 4,
  BKZ_MAX_TIME    = 8,
  BKZ_BOUNDED_LLL = 0x10,
  BKZ_AUTO_ABORT  = 0x20,
  BKZ_DUMP_GSO    = 0x40,
  BKZ_GH_BND      = 0x80,
  BKZ_SD_VARIANT  = 0x100,
  BKZ_SLD_RED     = 0x200
};

enum HKZFlags
{
  HKZ_DEFAULT = 0,
  HKZ_VERBOSE = 1
};

#ifndef FPLLL_DEFAULT_STRATEGY_PATH
#define FPLLL_DEFAULT_STRATEGY_PATH ""
#endif

#ifndef FPLLL_DEFAULT_STRATEGY
#define FPLLL_DEFAULT_STRATEGY ""
#endif

enum class PrunerMetric
{
  PRUNER_METRIC_PROBABILITY_OF_SHORTEST = 0,
  PRUNER_METRIC_EXPECTED_SOLUTIONS      = 1
};
using enum PrunerMetric;

enum PrunerFlags
{
  PRUNER_CVP =
      0x1,  // Do not Halve the count of nodes, according to enumeration optimization from symmetry
  // Descent methods. If several activated, pruner will execute them in the order below.
  PRUNER_START_FROM_INPUT = 0x2,
  PRUNER_GRADIENT         = 0x4,  // Activate the gradient descent
  PRUNER_NELDER_MEAD      = 0x8,
  // Verbosity
  PRUNER_VERBOSE = 0x10,
  // Optimize w.r.t to half of the coefficients (those of even indices)
  // (by default this is not enabled)
  PRUNER_HALF = 0x20,
  // Optimize goal set to single enumeration cost while fixing the probability ~ target. Note that
  // flags PRUNER_HALF and PRUNER_SINGLE are mutually exclusive.
  PRUNER_SINGLE = 0x40
};

#define PRUNER_ZEALOUS (PRUNER_GRADIENT | PRUNER_NELDER_MEAD)

FPLLL_END_NAMESPACE

#endif
