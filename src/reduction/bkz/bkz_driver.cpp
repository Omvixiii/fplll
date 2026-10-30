// SPDX-FileCopyrightText: 2011 Xavier Pujol
// SPDX-FileCopyrightText: 2014-2016 Martin R. Albrecht
// SPDX-FileCopyrightText: 2016 Michael Walter
// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "../lll/lll_kernel.h"
#include "../wrapper.h"
#include "bkz_kernel.h"
#include "detail/float_type.h"

#include <fplll/defs.h>
#include <fplll/gso/gso.h>
#include <fplll/gso/gso_interface.h>
#include <fplll/nr/matrix.h>
#include <fplll/nr/nr.h>
#include <fplll/reduction/bkz.h>
#include <fplll/reduction/bkz_param.h>
#include <fplll/reduction/lll.h>
#include <fplll/util.h>

#include <chrono>
#include <cstddef>
#include <gmp.h>
#include <type_traits>

namespace fplll
{

namespace
{

/** Validates a BKZ parameter object before any reduction work is done. */
void validate_bkz_param(const BKZParam &param)
{
  FPLLL_CHECK(param.block_size >= 2, "BKZ block size must be at least 2 (got {})",
              param.block_size);
  FPLLL_CHECK(param.delta > 0.25 && param.delta <= 1.0, "BKZ delta must be in (0.25, 1] (got {})",
              param.delta);

  if (!param.strategies.empty())
  {
    FPLLL_CHECK(param.strategies.size() > static_cast<std::size_t>(param.block_size),
                "BKZ strategy table has no entry for block size {} (largest available: {})",
                param.block_size, param.strategies.size() - 1);
    for (const Strategy &strategy : param.strategies)
    {
      for (const std::size_t preproc : strategy.preprocessing_block_sizes)
      {
        FPLLL_CHECK(preproc < param.strategies.size(),
                    "BKZ preprocessing block size {} is outside the strategy table (largest "
                    "available: {})",
                    preproc, param.strategies.size() - 1);
      }
    }
  }

  if (param.gh_factor)
  {
    FPLLL_CHECK(*param.gh_factor > 0.0, "BKZ Gaussian-heuristic factor must be positive (got {})",
                *param.gh_factor);
  }
  if (param.max_loops)
  {
    FPLLL_CHECK(*param.max_loops >= 0, "BKZ maximum number of loops must be non-negative (got {})",
                *param.max_loops);
  }
  if (param.max_time)
  {
    FPLLL_CHECK(*param.max_time >= std::chrono::milliseconds::zero(),
                "BKZ time limit must be non-negative (got {} ms)", param.max_time->count());
  }
  FPLLL_CHECK(param.min_success_probability > 0.0 && param.min_success_probability <= 1.0,
              "BKZ minimum success probability must be in (0, 1] (got {})",
              param.min_success_probability);
  FPLLL_CHECK(param.rerandomization_density >= 0,
              "BKZ rerandomization density must be non-negative (got {})",
              param.rerandomization_density);
}

/** Resolved backend and LLL parameters of a BKZ run. */
struct BKZPlan
{
  detail::FloatTypeResolution float_type;
  LLLParameters lll_parameters;
};

/** Number of non-zero rows of `b`. */
int count_non_zero_rows(const ZZ_mat<mpz_t> &b)
{
  int rows = 0;
  for (int i = 0; i < b.get_rows(); ++i)
  {
    if (!b[i].is_zero())
    {
      ++rows;
    }
  }
  return rows;
}

/**
 * Validates the BKZ parameters and resolves the floating-point backend.
 *
 * This runs before the entry points modify `b` or `u`.  SD-BKZ reduces a full
 * window of `block_size` rows, so a block larger than the number of non-zero
 * rows is rejected here instead of producing negative tour bounds.
 */
BKZPlan prepare_bkz(const ZZ_mat<mpz_t> &b, const BKZParam &param)
{
  validate_bkz_param(param);

  const int num_rows = count_non_zero_rows(b);
  if (param.variant == BKZVariant::sd_bkz && num_rows >= 2)
  {
    FPLLL_CHECK(param.block_size <= num_rows,
                "SD-BKZ block size {} exceeds the number of non-zero rows ({})", param.block_size,
                num_rows);
  }

  const double lll_delta = param.delta < 1.0 ? param.delta : LLL_DEF_DELTA;
  return BKZPlan{
      .float_type     = detail::resolve_float_type(param.float_type, param.precision, PREC_DOUBLE,
                                                   detail::mpfr_default_precision(param.precision)),
      .lll_parameters = LLLParameters{.delta = lll_delta, .eta = LLL_DEF_ETA},
  };
}

// Runs the block reduction with a fixed floating-point backend.
template <class FT>
ReductionReport bkz_reduction_f(ZZ_mat<mpz_t> &b, const BKZParam &param,
                                const LLLParameters &lll_parameters, ZZ_mat<mpz_t> &u,
                                ZZ_mat<mpz_t> &u_inv, int precision)
{
  GSOOptions gso_options;
  gso_options.precision = precision;
  if constexpr (std::is_same_v<FT, FP_NR<double>>)
  {
    gso_options.row_expo = true;
  }

  ZZ_mat<long> bl;
  ZZ_mat<long> ul;
  ZZ_mat<long> ul_inv;

  // we check if we can convert the basis to long integers for performance
  if (convert<long, mpz_t>(bl, b, 10) && convert<long, mpz_t>(ul, u, 10) &&
      convert<long, mpz_t>(ul_inv, u_inv, 10))
  {
    MatGSO<Z_NR<long>, FT> m_gso(bl, ul, ul_inv, gso_options);
    LLLReduction<Z_NR<long>, FT> lll_obj(m_gso, lll_parameters);
    BKZReduction<Z_NR<long>, FT> bkz_obj(m_gso, lll_obj, param);
    bkz_obj.bkz();
    convert<mpz_t, long>(b, bl, 0);
    convert<mpz_t, long>(u, ul, 0);
    convert<mpz_t, long>(u_inv, ul_inv, 0);
    return ReductionReport{
        .status = bkz_obj.status, .tours = bkz_obj.tours, .nodes = bkz_obj.nodes};
  }
  else
  {
    MatGSO<Z_NR<mpz_t>, FT> m_gso(b, u, u_inv, gso_options);
    LLLReduction<Z_NR<mpz_t>, FT> lll_obj(m_gso, lll_parameters);
    BKZReduction<Z_NR<mpz_t>, FT> bkz_obj(m_gso, lll_obj, param);
    bkz_obj.bkz();
    return ReductionReport{
        .status = bkz_obj.status, .tours = bkz_obj.tours, .nodes = bkz_obj.nodes};
  }
}

ReductionReport bkz_reduction_impl(ZZ_mat<mpz_t> &b, ZZ_mat<mpz_t> &u, const BKZParam &param,
                                   const BKZPlan &plan)
{
  ZZ_mat<mpz_t> u_inv;  // Empty inverse transform -> not computed

  // Nothing to reduce: no backend is needed, so the report keeps the defaults
  // (automatic/0) and only reports the trivially-empty run statistics.
  if (b.get_rows() == 0 || b.get_cols() == 0)
  {
    ReductionReport report;
    report.guaranteed = false;
    report.tours      = 0;
    report.nodes      = 0;
    return report;
  }

  ReductionReport report;
  report.float_type = plan.float_type.type;
  report.precision  = plan.float_type.precision;
  report.guaranteed = false;
  report.tours      = 0;
  report.nodes      = 0;

  /* lllwrapper (no FloatType needed) */
  if (param.no_lll)
  {
    zeros_last(b, u, u_inv);
  }
  else
  {
    Wrapper wrapper(b, u, u_inv, plan.lll_parameters);
    if (!wrapper.lll())
    {
      report.status = wrapper.status;
      return report;
    }
  }

  /* bkz (with float_type) */
  auto reduce = [&]<class F> -> ReductionReport
  {
    return bkz_reduction_f<FP_NR<F>>(b, param, plan.lll_parameters, u, u_inv,
                                     plan.float_type.precision);
  };
  report            = detail::dispatch_float_type(plan.float_type.type, reduce);
  report.float_type = plan.float_type.type;
  report.precision  = plan.float_type.precision;
  report.guaranteed = false;
  zeros_first(b, u, u_inv);
  return report;
}

}  // namespace

ReductionReport bkz_reduction(ZZ_mat<mpz_t> &b, const BKZParam &param)
{
  const BKZPlan plan = prepare_bkz(b, param);
  ZZ_mat<mpz_t> empty_mat;  // Empty u -> transform disabled
  return bkz_reduction_impl(b, empty_mat, param, plan);
}

ReductionReport bkz_reduction(ZZ_mat<mpz_t> &b, ZZ_mat<mpz_t> &u, const BKZParam &param)
{
  const BKZPlan plan = prepare_bkz(b, param);
  u.gen_identity(b.get_rows());
  return bkz_reduction_impl(b, u, param, plan);
}

ReductionReport hkz_reduction(ZZ_mat<mpz_t> &b, const HKZOptions &options)
{
  // Validate the requested backend and precision before the trivial-run early
  // exit: a configuration that is rejected for a reducible basis (unsupported
  // backend, negative precision) must be rejected for a small or empty basis
  // as well instead of being silently accepted (E-6/P2-5).
  (void)detail::resolve_float_type(options.float_type, options.precision, PREC_DOUBLE,
                                   detail::mpfr_default_precision(options.precision));

  if (b.get_rows() < 2 || b.get_cols() == 0)
  {
    ReductionReport report;
    report.guaranteed = false;
    report.tours      = 0;
    report.nodes      = 0;
    return report;
  }

  const BKZParam param{.block_size = b.get_rows(),
                       .delta      = 1.0,
                       .variant    = BKZVariant::hkz,
                       .verbose    = options.verbose,
                       .float_type = options.float_type,
                       .precision  = options.precision};
  return bkz_reduction(b, param);
}

}  // namespace fplll
