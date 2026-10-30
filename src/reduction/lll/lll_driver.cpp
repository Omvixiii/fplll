// SPDX-FileCopyrightText: 2005-2008 Damien Stehle
// SPDX-FileCopyrightText: 2007 David Cade
// SPDX-FileCopyrightText: 2011 Xavier Pujol
// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "../lll_family.h"
#include "../wrapper.h"
#include "detail/float_type.h"
#include "lll_kernel.h"

#include <fplll/defs.h>
#include <fplll/gso/gso.h>
#include <fplll/gso/gso_interface.h>
#include <fplll/nr/matrix.h>
#include <fplll/nr/nr.h>
#include <fplll/reduction/lll.h>
#include <fplll/util.h>

#include <gmp.h>
#include <type_traits>

namespace fplll
{

namespace
{

/**
 * LLL reduction of a basis over the integers with a fixed floating-point type.
 */
template <class ZT, class FT>
RedStatus lll_reduction_zf(ZZ_mat<ZT> &b, ZZ_mat<ZT> &u, ZZ_mat<ZT> &u_inv,
                           const LLLParameters &parameters, LLLMethod method, int precision)
{
  if (b.get_rows() == 0 || b.get_cols() == 0)
  {
    return RedStatus::success;
  }

  GSOOptions gso_options;
  gso_options.precision = precision;
  if (method == LLLMethod::proved)
  {
    gso_options.int_gram = true;
  }
  if (method == LLLMethod::fast)
  {
    gso_options.row_expo   = true;
    gso_options.force_long = true;
  }
  // See call_lll(): row-exponent scaling cannot be combined with the exact
  // integer Gram matrix.
  if (std::is_same_v<FT, double> && method != LLLMethod::proved)
  {
    gso_options.row_expo = true;
  }
  MatGSO<Z_NR<ZT>, FP_NR<FT>> m_gso(b, u, u_inv, gso_options);
  LLLReduction<Z_NR<ZT>, FP_NR<FT>> lll_obj(m_gso, parameters);
  lll_obj.lll();
  return lll_obj.status;
}

/** Precision needed by the proved LLL variant for `b`. */
int lll_good_precision(const ZZ_mat<mpz_t> &b, const LLLParameters &parameters)
{
  return is_empty_basis(b)
             ? PREC_DOUBLE
             : l2_min_prec(b.get_rows(), parameters.delta, parameters.eta, LLL_DEF_EPSILON);
}

/** Backend, precision and proved precision of an LLL run. */
struct LLLPlan
{
  detail::FloatTypeResolution float_type;
  int good_prec = 0;
};

/**
 * Validates the LLL options and resolves the floating-point backend.
 *
 * The entry points call this before they write `u`/`u_inv`, so an invalid
 * option is reported without modifying any matrix.
 */
LLLPlan prepare_lll(const ZZ_mat<mpz_t> &b, const LLLOptions &options)
{
  const LLLParameters &parameters = options.parameters;
  check_lll_parameters(parameters.delta, parameters.eta);

  if (options.method == LLLMethod::wrapper)
  {
    FPLLL_CHECK(options.float_type == FloatType::automatic,
                "The floating point type cannot be specified with the wrapper LLL method");
    FPLLL_CHECK(options.precision == 0,
                "The precision cannot be specified with the wrapper LLL method");
    return {};
  }

  FPLLL_CHECK(!(options.method == LLLMethod::proved && parameters.early_reduction),
              "LLL method 'proved' with early reduction is not implemented");

  const int good_prec = lll_good_precision(b, parameters);
  const int wanted    = options.precision != 0                ? options.precision
                        : options.method == LLLMethod::proved ? good_prec
                                                              : PREC_DOUBLE;
  LLLPlan plan{
      .float_type =
          detail::resolve_float_type(options.float_type, options.precision, wanted, wanted),
      .good_prec = good_prec,
  };
  FPLLL_CHECK(options.method != LLLMethod::fast || plan.float_type.type != FloatType::mpfr,
              "the fast LLL method requires the double, dd or qd floating point type");
  return plan;
}

/**
 * LLL reduction entry point.
 */
ReductionReport lll_reduction_impl(ZZ_mat<mpz_t> &b, ZZ_mat<mpz_t> &u, ZZ_mat<mpz_t> &u_inv,
                                   const LLLOptions &options, const LLLPlan &plan)
{
  const LLLParameters &parameters = options.parameters;

  // An empty basis needs no backend.  Short-circuit before the wrapper is
  // constructed: its constructor computes log2(d * n) and the proved
  // precision, which used to make an empty basis run a log2(0) cast to long
  // (undefined behaviour) before lll() could return early (E-5/P2-3).
  if (is_empty_basis(b))
  {
    zeros_first(b, u, u_inv);
    ReductionReport report;
    report.guaranteed = true;
    return report;
  }

  /* switch to wrapper */
  if (options.method == LLLMethod::wrapper)
  {
    Wrapper wrapper(b, u, u_inv, parameters);
    wrapper.lll();
    zeros_first(b, u, u_inv);
    return wrapper.report;
  }

  const FloatType sel_ft = plan.float_type.type;
  const int sel_prec     = plan.float_type.precision;

  if (parameters.verbose)
  {
    log_stream() << "Starting LLL method '" << to_string(options.method) << "'" << '\n'
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
  const RedStatus status = detail::dispatch_float_type(
      sel_ft,
      [&]<class F>
      {
        constexpr bool is_mpfr = std::is_same_v<F, mpfr_t>;
        return lll_reduction_zf<mpz_t, F>(b, u, u_inv, parameters, options.method,
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

ReductionReport lll_reduction(ZZ_mat<mpz_t> &b, const LLLOptions &options)
{
  const LLLPlan plan = prepare_lll(b, options);
  ZZ_mat<mpz_t> empty_mat;  // Empty u -> transform disabled
  return lll_reduction_impl(b, empty_mat, empty_mat, options, plan);
}

ReductionReport lll_reduction(ZZ_mat<mpz_t> &b, ZZ_mat<mpz_t> &u, const LLLOptions &options)
{
  const LLLPlan plan = prepare_lll(b, options);
  ZZ_mat<mpz_t> empty_mat;
  u.gen_identity(b.get_rows());
  return lll_reduction_impl(b, u, empty_mat, options, plan);
}

ReductionReport lll_reduction(ZZ_mat<mpz_t> &b, ZZ_mat<mpz_t> &u, ZZ_mat<mpz_t> &u_inv,
                              const LLLOptions &options)
{
  const LLLPlan plan = prepare_lll(b, options);
  u.gen_identity(b.get_rows());
  u_inv.gen_identity(b.get_rows());
  u_inv.transpose();
  const ReductionReport report = lll_reduction_impl(b, u, u_inv, options, plan);
  u_inv.transpose();
  return report;
}

}  // namespace fplll
