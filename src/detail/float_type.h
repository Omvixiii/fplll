// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef FPLLL_DETAIL_FLOAT_TYPE_H
#define FPLLL_DETAIL_FLOAT_TYPE_H

#include <fplll/defs.h>
#include <fplll/nr/nr.h>

#include <utility>

namespace fplll
{

/** Restores the previous MPFR default precision when leaving a scope. */
class ScopedPrecision
{
public:
  explicit ScopedPrecision(unsigned int precision)
      : old_precision_(FP_NR<mpfr_t>::set_prec(precision))
  {
  }

  ~ScopedPrecision() noexcept { FP_NR<mpfr_t>::set_prec(old_precision_); }

  ScopedPrecision(const ScopedPrecision &)            = delete;
  ScopedPrecision &operator=(const ScopedPrecision &) = delete;

private:
  unsigned int old_precision_;
};

namespace detail
{

/** The floating-point backend and precision a reduction will run at. */
struct FloatTypeResolution
{
  /** Backend to use; never `FloatType::automatic` in a resolved value. */
  FloatType type = FloatType::automatic;
  /** Precision of the backend; 0 for the fixed-precision backends is resolved
      to their native precision. */
  int precision = 0;
};

/** Native precision of a fixed-precision backend; 0 for the other values. */
constexpr int native_precision(FloatType type) noexcept
{
  switch (type)
  {
  case FloatType::double_precision:
    return PREC_DOUBLE;
  case FloatType::dd:
    return PREC_DD;
  case FloatType::qd:
    return PREC_QD;
  case FloatType::automatic:
  case FloatType::mpfr:
    return 0;
  }
  return 0;
}

/**
   MPFR precision to assume when a BKZ/HKZ caller did not specify one.

   `BKZParam` and `HKZOptions` document `precision == 0` as "not specified":
   their entry points pass this as the `mpfr_default_precision` of
   `resolve_float_type()`, so an explicitly requested MPFR backend without a
   precision runs at the current process default MPFR precision (the value
   `FP_NR<mpfr_t>::get_prec()` reports), while `FloatType::automatic` keeps
   selecting double precision.

   LLL and HLLL keep their own, older semantics: their `precision == 0` means
   "the precision the algorithm selected", so their drivers pass that
   algorithm-selected precision (`wanted`) as `mpfr_default_precision`
   instead of this helper.  A positive `requested_precision` is returned
   unchanged.
*/
inline int mpfr_default_precision(int requested_precision) noexcept
{
  return requested_precision > 0 ? requested_precision
                                 : static_cast<int>(FP_NR<mpfr_t>::get_prec());
}

/**
   Resolves the floating-point backend and precision of a reduction.

   - A non-zero `requested_precision` selects MPFR at that precision; asking for
     a fixed-precision backend together with a precision is rejected.
   - `FloatType::automatic` selects the cheapest backend that provides
     `wanted_precision`: double, then dd/qd when this build enables QD, then
     MPFR at `wanted_precision`.
   - An explicitly requested fixed-precision backend runs at its native
     precision; an explicitly requested MPFR backend without a precision runs
     at `mpfr_default_precision`, or is rejected when that is 0.

   The callers choose `mpfr_default_precision`: LLL/HLLL pass the precision
   their method selected, so their `precision == 0` means "the
   algorithm-selected precision"; BKZ/HKZ pass
   `mpfr_default_precision(precision)`, so their explicit MPFR backend without
   a precision means "the current process default MPFR precision" rather than
   "missing precision".  Every rejection is an `fplll::Error` raised by
   `FPLLL_CHECK`.  The entry points call this function before they modify their
   basis or transformation matrices, so a rejected configuration leaves the
   caller's data untouched.
*/
inline FloatTypeResolution resolve_float_type(FloatType requested, int requested_precision,
                                              int wanted_precision, int mpfr_default_precision)
{
  FPLLL_CHECK(requested_precision >= 0,
              "the floating point precision must be non-negative (got {})", requested_precision);

  if (requested_precision != 0)
  {
    FPLLL_CHECK(requested == FloatType::automatic || requested == FloatType::mpfr,
                "the floating point type must be mpfr when the precision is specified (got '{}')",
                to_string(requested));
    return {.type = FloatType::mpfr, .precision = requested_precision};
  }

  if (requested == FloatType::automatic)
  {
    FPLLL_CHECK(wanted_precision > 0, "internal error: no floating point precision was requested");
    if (wanted_precision <= native_precision(FloatType::double_precision))
    {
      return {.type      = FloatType::double_precision,
              .precision = native_precision(FloatType::double_precision)};
    }
#ifdef FPLLL_WITH_QD
    if (wanted_precision <= native_precision(FloatType::dd))
    {
      return {.type = FloatType::dd, .precision = native_precision(FloatType::dd)};
    }
    if (wanted_precision <= native_precision(FloatType::qd))
    {
      return {.type = FloatType::qd, .precision = native_precision(FloatType::qd)};
    }
#endif
    return {.type = FloatType::mpfr, .precision = wanted_precision};
  }

  FPLLL_CHECK(is_supported(requested),
              "this build of fplll does not support the floating point type '{}'",
              to_string(requested));
  if (requested == FloatType::mpfr)
  {
    FPLLL_CHECK(mpfr_default_precision > 0, "missing precision for the mpfr floating point type");
    return {.type = FloatType::mpfr, .precision = mpfr_default_precision};
  }
  return {.type = requested, .precision = native_precision(requested)};
}

#ifdef FPLLL_WITH_QD

/** Restores the x87 floating-point control word when leaving a scope.

    The dd/qd backends need the wider x87 precision while they run.  The guard
    makes that save/restore pair exception safe and keeps the FPU contract
    identical for every entry point.  On targets where the QD backend does not
    configure an FPU control word, the QD functions are no-ops. */
class FpuRoundGuard
{
public:
  FpuRoundGuard() { fpu_fix_start(&old_cw_); }

  ~FpuRoundGuard() noexcept { fpu_fix_end(&old_cw_); }

  FpuRoundGuard(const FpuRoundGuard &)            = delete;
  FpuRoundGuard &operator=(const FpuRoundGuard &) = delete;

private:
  unsigned int old_cw_ = 0;
};

#endif

/**
   Invokes `f.template operator()<F>()` with the C++ floating-point type that
   implements the resolved backend `type`.

   This is the single place where fplll turns the runtime backend selection
   into its compile-time instantiations: the LLL, HLLL and BKZ entry points each
   contain one call site and no switch of their own.  The dd/qd branches hold
   an `FpuRoundGuard` for the duration of the call, so both backends run under
   the same FPU contract no matter which entry point dispatched them.

   `type` must be resolved: `FloatType::automatic`, and in a build without QD
   also `dd`/`qd`, abort with an internal error.  The switch lists every
   enumerator without a `default`, so adding a backend produces a `-Wswitch`
   warning instead of silently falling through.
*/
template <class Func> decltype(auto) dispatch_float_type(FloatType type, Func &&f)
{
  switch (type)
  {
  case FloatType::double_precision:
    return std::forward<Func>(f).template operator()<double>();
#ifdef FPLLL_WITH_QD
  case FloatType::dd:
  {
    const FpuRoundGuard guard;
    return std::forward<Func>(f).template operator()<dd_real>();
  }
  case FloatType::qd:
  {
    const FpuRoundGuard guard;
    return std::forward<Func>(f).template operator()<qd_real>();
  }
#else
  // A build without QD rejects these backends before a reduction dispatches.
  case FloatType::dd:
  case FloatType::qd:
    break;
#endif
  case FloatType::mpfr:
    return std::forward<Func>(f).template operator()<mpfr_t>();
  case FloatType::automatic:
    break;
  }
  FPLLL_ABORT("internal error: unresolved floating point type '{}'", to_string(type));
}

}  // namespace detail

}  // namespace fplll

#endif
