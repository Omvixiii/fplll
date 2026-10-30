// SPDX-FileCopyrightText: 2005-2008 Damien Stehle
// SPDX-FileCopyrightText: 2007 David Cade
// SPDX-FileCopyrightText: 2011 Xavier Pujol
// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "../lll_family.h"
#include "../wrapper.h"
#include "detail/float_type.h"
#include "hlll_kernel.h"

#include <fplll/defs.h>
#include <fplll/gso/householder.h>
#include <fplll/nr/matrix.h>
#include <fplll/nr/nr.h>
#include <fplll/reduction/hlll.h>
#include <fplll/util.h>

#include <gmp.h>
#include <type_traits>

namespace fplll
{

namespace
{

/**
 * HLLL reduction of a basis over the integers with a fixed floating-point type.
 */
template <class ZT, class FT>
RedStatus hlll_reduction_zf(ZZ_mat<ZT> &b, ZZ_mat<ZT> &u, ZZ_mat<ZT> &u_inv,
                            const HLLLParameters &parameters, LLLMethod method, int precision)
{
  if (b.get_rows() == 0 || b.get_cols() == 0)
  {
    return RedStatus::success;
  }

  HouseholderOptions householder_options;
  householder_options.precision = precision;
  if (std::is_same_v<FT, double>)
  {
    householder_options.row_expo = true;
  }
  if (method == LLLMethod::fast)
  {
    householder_options.row_expo   = true;
    householder_options.force_long = true;
  }
  MatHouseholder<Z_NR<ZT>, FP_NR<FT>> m(b, u, u_inv, householder_options);
  HLLLReduction<Z_NR<ZT>, FP_NR<FT>> hlll_obj(m, parameters);
  hlll_obj.hlll();
  return hlll_obj.get_status();
}

// Verifies that b is HLLL-reduced according to the given parameters.
template <class ZT, class FT>
RedStatus is_hlll_reduced_pr(ZZ_mat<ZT> &b, ZZ_mat<ZT> &u, ZZ_mat<ZT> &u_inv,
                             const HLLLParameters &parameters, int precision)
{
  if (b.get_rows() == 0 || b.get_cols() == 0)
  {
    return RedStatus::success;
  }

  MatHouseholder<Z_NR<ZT>, FP_NR<FT>> m(b, u, u_inv, HouseholderOptions{.precision = precision});
  return is_hlll_reduced<Z_NR<ZT>, FP_NR<FT>>(m, parameters) ? RedStatus::success
                                                             : RedStatus::hlll_failure;
}

/** Precision needed by the proved HLLL variant for `b`. */
int hlll_good_precision(const ZZ_mat<mpz_t> &b, const HLLLParameters &parameters)
{
  return is_empty_basis(b) ? PREC_DOUBLE
                           : hlll_min_prec(b.get_rows(), b.get_cols(), parameters.delta,
                                           parameters.eta, parameters.theta, parameters.c);
}

/** Backend, precision and proved precision of an HLLL run. */
struct HLLLPlan
{
  detail::FloatTypeResolution float_type;
  int good_prec = 0;
};

/**
 * Validates the HLLL options and resolves the floating-point backend.
 *
 * Like prepare_lll(), this runs before the entry points write any matrix.
 */
HLLLPlan prepare_hlll(const ZZ_mat<mpz_t> &b, const HLLLOptions &options)
{
  const HLLLParameters &parameters = options.parameters;
  check_lll_parameters(parameters.delta, parameters.eta);
  FPLLL_CHECK(options.precision >= 0, "the floating point precision must be non-negative (got {})",
              options.precision);
  FPLLL_CHECK(parameters.theta >= 0.0 && parameters.theta < parameters.eta - 0.5,
              "HLLL theta must satisfy 0 <= theta < eta - 0.5 (got {})", parameters.theta);
  FPLLL_CHECK(parameters.c > 0.0, "HLLL c must be positive (got {})", parameters.c);
  FPLLL_CHECK(options.method != LLLMethod::heuristic,
              "the heuristic HLLL method is not implemented");

  // A verification-only run uses MPFR at the proved precision and does not
  // select a backend.
  if (options.no_lll)
  {
    return {.float_type = {}, .good_prec = hlll_good_precision(b, parameters)};
  }

  if (options.method == LLLMethod::wrapper)
  {
    FPLLL_CHECK(options.float_type == FloatType::automatic,
                "The floating point type cannot be specified with the wrapper HLLL method");
    FPLLL_CHECK(options.precision == 0,
                "The precision cannot be specified with the wrapper HLLL method");
    return {};
  }

  const int good_prec = hlll_good_precision(b, parameters);
  const int wanted    = options.precision != 0                ? options.precision
                        : options.method == LLLMethod::proved ? good_prec
                                                              : PREC_DOUBLE;
  HLLLPlan plan{
      .float_type =
          detail::resolve_float_type(options.float_type, options.precision, wanted, wanted),
      .good_prec = good_prec,
  };
  FPLLL_CHECK(options.method != LLLMethod::fast || plan.float_type.type != FloatType::mpfr,
              "the fast HLLL method requires the double, dd or qd floating point type");
  return plan;
}

/**
 * HLLL reduction entry point.
 */
ReductionReport hlll_reduction_impl(ZZ_mat<mpz_t> &b, ZZ_mat<mpz_t> &u, ZZ_mat<mpz_t> &u_inv,
                                    const HLLLOptions &options, const HLLLPlan &plan)
{
  const HLLLParameters &parameters = options.parameters;
  RedStatus status                 = RedStatus::success;

  // Nothing is reduced or verified for an empty basis.
  if (is_empty_basis(b))
  {
    ReductionReport report;
    report.guaranteed = true;
    return report;
  }

  // If no_lll is set, only verify whether the basis is reduced or not.
  if (options.no_lll)
  {
    const int sel_prec = (options.precision != 0) ? options.precision : plan.good_prec;

    if (parameters.verbose)
    {
      log_stream() << "Starting HLLL method 'verification'" << '\n'
                   << "  floating point type 'mpfr_t'" << '\n';

      if (sel_prec < plan.good_prec)
      {
        log_stream() << "  prec < " << plan.good_prec << ", the verification is not guaranteed";
      }
      else
      {
        log_stream() << "  prec >= " << plan.good_prec << ", the verification is guaranteed";
      }

      log_stream() << '\n';
    }

    status = is_hlll_reduced_pr<mpz_t, mpfr_t>(b, u, u_inv, parameters, sel_prec);

    if (parameters.verbose)
    {
      log_stream() << (status == RedStatus::success ? "Basis is reduced" : "Basis is not reduced")
                   << '\n';
    }

    ReductionReport report;
    report.status     = status;
    report.float_type = FloatType::mpfr;
    report.precision  = sel_prec;
    report.guaranteed = status == RedStatus::success && sel_prec >= plan.good_prec;
    return report;
  }

  // Nothing is reduced for an empty basis; the configuration still validates.
  if (is_empty_basis(b))
  {
    ReductionReport report;
    report.guaranteed = true;
    return report;
  }

  /* switch to wrapper */
  if (options.method == LLLMethod::wrapper)
  {
    Wrapper wrapper(b, u, u_inv, parameters);
    const bool complete = wrapper.hlll();
    zeros_first(b, u, u_inv);
    ReductionReport report = wrapper.report;
    if (!complete && report.status == RedStatus::success)
    {
      report.status = RedStatus::hlll_failure;
    }
    return report;
  }

  const FloatType sel_ft = plan.float_type.type;
  const int sel_prec     = plan.float_type.precision;

  if (parameters.verbose)
  {
    log_stream() << "Starting HLLL method '" << to_string(options.method) << "'" << '\n'
                 << "  floating point type '" << to_string(sel_ft) << "'" << '\n';
    if (options.method != LLLMethod::proved || sel_ft == FloatType::double_precision)
    {
      log_stream() << "  The reduction is not guaranteed";
    }
    else if (sel_prec < plan.good_prec)
    {
      log_stream() << "  prec < " << plan.good_prec << ", the reduction is not guaranteed";
    }
    else
    {
      log_stream() << "  prec >= " << plan.good_prec << ", the reduction is guaranteed";
    }
    log_stream() << '\n';
  }

  // Applies the selected method
  status = detail::dispatch_float_type(sel_ft,
                                       [&]<class F>
                                       {
                                         constexpr bool is_mpfr = std::is_same_v<F, mpfr_t>;
                                         return hlll_reduction_zf<mpz_t, F>(b, u, u_inv, parameters,
                                                                            options.method,
                                                                            is_mpfr ? sel_prec : 0);
                                       });
  zeros_first(b, u, u_inv);

  ReductionReport report;
  report.status     = status;
  report.float_type = sel_ft;
  report.precision  = sel_prec;
  report.guaranteed = status == RedStatus::success && options.method == LLLMethod::proved &&
                      sel_prec >= plan.good_prec;
  return report;
}

}  // namespace

ReductionReport hlll_reduction(ZZ_mat<mpz_t> &b, const HLLLOptions &options)
{
  const HLLLPlan plan = prepare_hlll(b, options);
  ZZ_mat<mpz_t> empty_mat;  // Empty u -> transform disabled
  return hlll_reduction_impl(b, empty_mat, empty_mat, options, plan);
}

ReductionReport hlll_reduction(ZZ_mat<mpz_t> &b, ZZ_mat<mpz_t> &u, const HLLLOptions &options)
{
  const HLLLPlan plan = prepare_hlll(b, options);
  ZZ_mat<mpz_t> empty_mat;
  u.gen_identity(b.get_rows());
  return hlll_reduction_impl(b, u, empty_mat, options, plan);
}

ReductionReport hlll_reduction(ZZ_mat<mpz_t> &b, ZZ_mat<mpz_t> &u, ZZ_mat<mpz_t> &u_inv,
                               const HLLLOptions &options)
{
  const HLLLPlan plan = prepare_hlll(b, options);
  u.gen_identity(b.get_rows());
  u_inv.gen_identity(b.get_rows());
  u_inv.transpose();
  const ReductionReport report = hlll_reduction_impl(b, u, u_inv, options, plan);
  u_inv.transpose();
  return report;
}

}  // namespace fplll
