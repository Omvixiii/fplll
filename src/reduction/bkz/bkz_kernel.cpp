// SPDX-FileCopyrightText: 2011 Xavier Pujol
// SPDX-FileCopyrightText: 2014-2016 Martin R. Albrecht
// SPDX-FileCopyrightText: 2016 Michael Walter
// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "bkz_kernel.h"
#include "../lll/lll_kernel.h"
#include "detail/cpu_time.h"

#include <fplll/defs.h>
#include <fplll/enum/enumerate.h>
#include <fplll/gso/gso_interface.h>
#include <fplll/nr/nr.h>
#include <fplll/pruner/pruner.h>
#include <fplll/reduction/bkz_param.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <format>
#include <gmp.h>
#include <mutex>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

namespace fplll
{

namespace
{

const char *variant_name(BKZVariant variant) noexcept
{
  switch (variant)
  {
  case BKZVariant::bkz:
    return "BKZ";
  case BKZVariant::sd_bkz:
    return "SD-BKZ";
  case BKZVariant::slide:
    return "SLD";
  case BKZVariant::hkz:
    return "HKZ";
  }
  return "BKZ";
}

}  // namespace

template <class ZT, class FT>
BKZReduction<ZT, FT>::BKZReduction(MatGSOInterface<ZT, FT> &m, LLLReduction<ZT, FT> &lll_obj,
                                   const BKZParam &param)
    : param(param), m(m), lll_obj(lll_obj)
{
  for (num_rows = m.get_d(); num_rows > 0 && m.b_row_is_zero(num_rows - 1); num_rows--)
  {
  }
  this->delta = param.delta;
}

template <class ZT, class FT> BKZReduction<ZT, FT>::~BKZReduction() noexcept = default;

template <class ZT, class FT>
void BKZReduction<ZT, FT>::rerandomize_block(int min_row, int max_row, int density)
{
  if (max_row - min_row < 2)
  {
    return;
  }

  std::scoped_lock rng_lock(RandGen::mutex());

  // 1. permute rows
  const std::size_t niter = 4 * static_cast<std::size_t>(max_row - min_row);  // some guestimate

  for (std::size_t i = 0; i < niter; ++i)
  {
    const std::size_t a = gmp_urandomm_ui(RandGen::get_gmp_state(), max_row - min_row) + min_row;
    std::size_t b       = a;
    while (b == a)
    {
      b = gmp_urandomm_ui(RandGen::get_gmp_state(), max_row - min_row) + min_row;
    }
    m.move_row(b, a);
  }

  // 2. triangular transformation matrix with coefficients in -1,0,1
  auto row_ops = m.row_operations(min_row, max_row);
  for (long a = min_row; a < max_row - 1; ++a)
  {
    for (long i = 0; i < density; i++)
    {
      const std::size_t b = gmp_urandomm_ui(RandGen::get_gmp_state(), max_row - (a + 1)) + (a + 1);
      if (gmp_urandomm_ui(RandGen::get_gmp_state(), 2))
      {
        m.row_add(a, b);
      }
      else
      {
        m.row_sub(a, b);
      }
    }
  }
}

template <class ZT, class FT>
const PruningParams &BKZReduction<ZT, FT>::get_pruning(int kappa, unsigned int block_size,
                                                       const BKZRunParameters &param) const
{
  static const PruningParams default_pruning;

  if (param.strategies == nullptr || param.strategies->empty())
  {
    return default_pruning;
  }

  FPLLL_DEBUG_CHECK(param.strategies->size() > block_size);

  const Strategy &strat = (*param.strategies)[block_size];

  long max_dist_expo;
  FT max_dist    = m.get_r_exp(kappa, kappa, max_dist_expo);
  FT gh_max_dist = max_dist;
  FT root_det    = m.get_root_det(kappa, kappa + block_size);
  adjust_radius_to_gh_bound(gh_max_dist, max_dist_expo, block_size, root_det, 1.0);
  return strat.get_pruning(max_dist.get_d() * std::pow(2, max_dist_expo),
                           gh_max_dist.get_d() * std::pow(2, max_dist_expo));
}

template <class ZT, class FT>
bool BKZReduction<ZT, FT>::svp_preprocessing(int kappa, unsigned int block_size,
                                             const BKZRunParameters &param)
{
  bool clean = true;

  const int lll_start = param.bounded_lll ? kappa : 0;
  if (!lll_obj.lll(lll_start, lll_start, kappa + static_cast<int>(block_size), 0))
  {
    throw BKZFailure(lll_obj.status);
  }
  if (lll_obj.n_swaps > 0)
  {
    clean = false;
  }

  // run one tour of recursive preprocessing
  if (param.strategies == nullptr || param.strategies->empty())
  {
    return clean;
  }

  for (const std::size_t preproc_size : (*param.strategies)[block_size].preprocessing_block_sizes)
  {
    int dummy_kappa_max = num_rows;
    const BKZRunParameters preprocessing_param{
        .block_size              = static_cast<int>(preproc_size),
        .strategies              = param.strategies,
        .verbose                 = false,
        .bounded_lll             = false,
        .gh_bound                = true,
        .gh_factor               = BKZ_DEF_GH_FACTOR,
        .min_success_probability = BKZ_DEF_MIN_SUCCESS_PROBABILITY,
        .rerandomization_density = BKZ_DEF_RERANDOMIZATION_DENSITY};
    clean &=
        tour(0, dummy_kappa_max, preprocessing_param, kappa, kappa + static_cast<int>(block_size));
  }

  return clean;
}

template <class ZT, class FT>
bool BKZReduction<ZT, FT>::svp_postprocessing(int kappa, int block_size,
                                              const std::vector<FT> &solution, bool dual)
{
  // Is it already in the basis ?
  int nz_vectors = 0, i_vector = -1;
  for (int i = block_size - 1; i >= 0; i--)
  {
    if (!solution[i].is_zero())
    {
      nz_vectors++;
      if (i_vector == -1 && std::fabs(solution[i].get_d()) == 1)
      {
        i_vector = i;
      }
    }
  }
  // nz_vectors is the number of nonzero coordinates
  // i_vector is the largest index for a \pm 1 coordinate
  FPLLL_DEBUG_CHECK(nz_vectors > 0);

  const int pos = dual ? kappa + block_size - 1 : kappa;
  if (nz_vectors == 1)
  {
    FPLLL_DEBUG_CHECK(i_vector != -1);
    // If the vector already sits at the target position there is nothing to
    // move: this happens with an exact enumeration radius (HKZ sets delta to
    // 1), where the current basis vector itself is a valid solution.
    if (kappa + i_vector == pos)
    {
      return true;  // no progress
    }
    // Yes, it is another vector
    m.move_row(kappa + i_vector, pos);
  }
  else if (i_vector != -1)
  {
    // No, but one coordinate is equal to \pm 1, we'll
    // just compute the new vector in that position.
    int sol_i = solution[i_vector].get_si();
    if (dual)
    {
      sol_i *= -1;
    }

    {
      // In case of primal reduction we can restrict the invalidation to the one
      // vector we are adding rows to.
      const int first = dual ? kappa : kappa + i_vector;
      const int last  = dual ? kappa + block_size : kappa + i_vector + 1;
      auto row_ops    = m.row_operations(first, last);
      for (int i = 0; i < block_size; ++i)
      {
        if (!solution[i].is_zero() && (i != i_vector))
        {
          if (dual)
          {
            m.row_addmul(kappa + i, kappa + i_vector, sol_i * solution[i]);
          }
          else
          {
            m.row_addmul(kappa + i_vector, kappa + i, sol_i * solution[i]);
          }
        }
      }
    }

    m.move_row(kappa + i_vector, pos);
  }
  else
  {
    // No, general case
    svp_postprocessing_generic(kappa, block_size, solution, dual);
  }
  return false;
}

template <class ZT, class FT>
bool BKZReduction<ZT, FT>::svp_postprocessing_generic(int kappa, int block_size,
                                                      const std::vector<FT> &solution, bool dual)
{
  std::vector<FT> x = solution;
  const int d       = block_size;
  // don't want to deal with negativ coefficients
  for (int i = 0; i < d; i++)
  {
    if (x[i] < 0)
    {
      x[i].neg(x[i]);
      m.negate_row_of_b(i + kappa);
    }
  }

  {
    auto row_ops = m.row_operations(kappa, kappa + d);
    // tree based gcd computation on x, performing operations also on b
    // (or the dual operations in case of primal [sounds weird,
    // but is correct] svp reduction)
    int off = 1;
    int k;
    while (off < d)
    {
      k = d - 1;
      while (k - off >= 0)
      {
        if (!(x[k].is_zero() && x[k - off].is_zero()))
        {
          if (x[k] < x[k - off])
          {
            x[k].swap(x[k - off]);
            m.row_swap(kappa + k - off, kappa + k);
          }

          while (!x[k - off].is_zero())
          {
            while (x[k - off] <= x[k])
            {
              x[k] = x[k] - x[k - off];
              if (dual)
              {
                m.row_sub(kappa + k, kappa + k - off);
              }
              else
              {
                m.row_add(kappa + k - off, kappa + k);
              }
            }

            x[k].swap(x[k - off]);
            m.row_swap(kappa + k - off, kappa + k);
          }
        }
        k -= 2 * off;
      }
      off *= 2;
    }
  }

  // the gcd computation will leave the desired vector in last
  // position, so in case of primal reduction we need to move it up
  if (!dual)
  {
    m.move_row(kappa + d - 1, kappa);
  }
  return false;
}

template <class ZT, class FT>
bool BKZReduction<ZT, FT>::svp_reduction(int kappa, int block_size, const BKZRunParameters &param,
                                         bool dual)
{
  const int first = dual ? kappa + block_size - 1 : kappa;

  // ensure we are computing something sensible.
  // note that the size reduction here is required, since
  // we're calling this function on unreduced blocks at times
  // (e.g. in bkz, after the previous block was reduced by a vector
  // already in the basis). if size reduction is not called,
  // old_first might be incorrect (e.g. close to 0) and the function
  // will return an incorrect clean flag
  // WARNING: do not try to increase the size reduction beyond first.
  // GSO might be invalid beyond this and this can cause numerical issues
  // and even nullpointers!
  if (!lll_obj.size_reduction(0, first + 1, 0))
  {
    throw BKZFailure(lll_obj.status);
  }
  FT old_first;
  long old_first_expo;
  old_first = FT(m.get_r_exp(first, first, old_first_expo));

  bool rerandomize             = false;
  double remaining_probability = 1.0;

  while (remaining_probability > 1. - param.min_success_probability)
  {
    if (rerandomize)
    {
      rerandomize_block(kappa + 1, kappa + block_size, param.rerandomization_density);
    }

    svp_preprocessing(kappa, block_size, param);

    // compute enumeration radius
    long max_dist_expo;
    FT max_dist = m.get_r_exp(first, first, max_dist_expo);
    if (dual)
    {
      max_dist.pow_si(max_dist, -1, GMP_RNDU);
      max_dist_expo *= -1;
    }
    max_dist *= delta;

    if (param.gh_bound && block_size > 30)
    {
      const FT root_det = m.get_root_det(kappa, kappa + block_size);
      adjust_radius_to_gh_bound(max_dist, max_dist_expo, block_size, root_det, param.gh_factor);
    }

    const PruningParams &pruning = get_pruning(kappa, block_size, param);

    FPLLL_DEBUG_CHECK(pruning.metric == PrunerMetric::probability_of_shortest);
    evaluator.solutions.clear();
    Enumeration<ZT, FT> enum_obj(m, evaluator);
    enum_obj.enumerate(kappa, kappa + block_size, max_dist, max_dist_expo, {}, pruning.coefficients,
                       dual);
    nodes += static_cast<long>(enum_obj.get_nodes());

    if (!evaluator.empty())
    {
      svp_postprocessing(kappa, block_size, evaluator.begin()->second, dual);
      rerandomize = false;
    }
    else
    {
      rerandomize = true;
    }
    remaining_probability *= (1 - pruning.expectation);
  }

  if (!lll_obj.size_reduction(0, first + 1, 0))
  {
    throw BKZFailure(lll_obj.status);
  }

  // in order to check if we made progress, we compare the new shortest vector to the
  // old one (note that simply checking clean flags is not sufficient since
  // preprocessing can have changed things but we don't know if it made progress)
  long new_first_expo;
  FT new_first = m.get_r_exp(first, first, new_first_expo);
  new_first.mul_2si(new_first, new_first_expo - old_first_expo);
  return dual ? (old_first >= new_first) : (old_first <= new_first);
}

template <class ZT, class FT>
bool BKZReduction<ZT, FT>::tour(const int loop, int &kappa_max, const BKZRunParameters &param,
                                int min_row, int max_row)
{
  bool clean = true;
  clean &= trunc_tour(kappa_max, param, min_row, max_row);
  clean &= hkz(kappa_max, param, std::max(max_row - param.block_size, 0), max_row);

  if (param.verbose)
  {
    print_tour(loop, min_row, max_row);
  }

  // Symmetrize the Gram matrix
  if (m.enable_int_gram)
  {
    m.symmetrize_g();
  }

  return clean;
}

template <class ZT, class FT>
bool BKZReduction<ZT, FT>::trunc_tour(int &kappa_max, const BKZRunParameters &param, int min_row,
                                      int max_row)
{
  bool clean           = true;
  const int block_size = param.block_size;
  for (int kappa = min_row; kappa < max_row - block_size; ++kappa)
  {
    clean &= svp_reduction(kappa, block_size, param);
    if (param.verbose && kappa_max < kappa && clean)
    {
      log_stream() << std::format("Block [1-{:>4}] BKZ-{} reduced for the first time\n", kappa + 1,
                                  param.block_size);
      kappa_max = kappa;
    }
  }

  return clean;
}

template <class ZT, class FT>
bool BKZReduction<ZT, FT>::trunc_dtour(const BKZRunParameters &param, int min_row, int max_row)
{
  bool clean           = true;
  const int block_size = param.block_size;

  for (int kappa = max_row - block_size; kappa > min_row; --kappa)
  {
    clean &= svp_reduction(kappa, block_size, param, true);
  }

  return clean;
}

template <class ZT, class FT>
bool BKZReduction<ZT, FT>::hkz(int &kappa_max, const BKZRunParameters &param, int min_row,
                               int max_row)
{
  bool clean = true;
  for (int kappa = min_row; kappa < max_row - 1; ++kappa)
  {
    const int block_size = max_row - kappa;
    clean &= svp_reduction(kappa, block_size, param);
    if (param.verbose && kappa_max < kappa && clean)
    {
      log_stream() << std::format("Block [1-{:>4}] BKZ-{} reduced for the first time\n", kappa + 1,
                                  param.block_size);
      kappa_max = kappa;
    }
  }

  // this line fixes fpylll issue 73 with stalling BKZ instances
  lll_obj.size_reduction(max_row - 1, max_row, max_row - 2);

  return clean;
}

template <class ZT, class FT>
bool BKZReduction<ZT, FT>::sd_tour(const int loop, const BKZRunParameters &param, int min_row,
                                   int max_row)
{
  int dummy_kappa_max = num_rows;
  bool clean          = true;
  clean &= trunc_dtour(param, min_row, max_row);
  clean &= trunc_tour(dummy_kappa_max, param, min_row, max_row);

  if (param.verbose)
  {
    print_tour(loop, min_row, max_row);
  }

  return clean;
}

template <class ZT, class FT>
bool BKZReduction<ZT, FT>::slide_tour(const int loop, const BKZRunParameters &param, int min_row,
                                      int max_row)
{
  int p = (max_row - min_row) / param.block_size;
  if ((max_row - min_row) % param.block_size)
  {
    ++p;
  }
  bool clean = true;  // this clean variable is only for the inner loop of slide reduction
  do
  {
    clean = true;
    for (int i = 0; i < p; ++i)
    {
      const int kappa      = min_row + i * param.block_size;
      const int block_size = std::min(max_row - kappa, param.block_size);
      clean &= svp_reduction(kappa, block_size, param);
    }
    // SVP reduction takes care of the LLL reduction if bounded_lll is off
    if (param.bounded_lll)
    {
      if (!lll_obj.lll(min_row, min_row, max_row, 0))
      {
        throw BKZFailure(lll_obj.status);
      }
      if (lll_obj.n_swaps > 0)
      {
        clean = false;
      }
    }
  } while (!clean);

  for (int i = 0; i < p - 1; ++i)
  {
    const int kappa = min_row + i * param.block_size + 1;
    svp_reduction(kappa, param.block_size, param, true);
  }

  const FT new_potential = m.get_slide_potential(min_row, max_row, param.block_size);

  if (param.verbose)
  {
    print_tour(loop, min_row, max_row);
  }

  // we check the potential function to see if we made progress
  if (new_potential >= sld_potential)
  {
    return true;
  }

  sld_potential = new_potential;
  return false;
}

template <class ZT, class FT> bool BKZReduction<ZT, FT>::bkz()
{
  const BKZRunParameters run_param{.block_size = param.block_size,
                                   .strategies =
                                       param.strategies.empty() ? nullptr : &param.strategies,
                                   .verbose     = param.verbose,
                                   .bounded_lll = param.bounded_lll,
                                   .gh_bound    = param.gh_factor.has_value(),
                                   .gh_factor   = param.gh_factor.value_or(BKZ_DEF_GH_FACTOR),
                                   .min_success_probability = param.min_success_probability,
                                   .rerandomization_density = param.rerandomization_density};

  RedStatus final_status = RedStatus::success;
  nodes                  = 0;
  const bool sd          = param.variant == BKZVariant::sd_bkz;
  const bool sld         = param.variant == BKZVariant::slide;
  algorithm              = sd ? "SD-BKZ" : sld ? "SLD" : "BKZ";

  // An all-zero basis has no rows left to reduce; hkz() would index row -1.
  if (num_rows < 2)
  {
    return set_status(RedStatus::success);
  }

  bool auto_abort = param.auto_abort;
  BKZAutoAbort<ZT, FT> auto_abort_obj(m, num_rows);

  if (sd && !(param.max_loops || param.max_time || auto_abort))
  {
    log_stream()
        << "Warning: SD Variant of BKZ requires explicit termination condition. Turning auto "
           "abort on!"
        << '\n';
    auto_abort = true;
  }

  if (param.verbose)
  {
    log_stream() << "Entering " << algorithm << ":" << '\n';
    print_params(run_param, log_stream());
    log_stream() << '\n';
  }

  cputime_start =
      (param.max_time || param.verbose) ? detail::cpu_time() : std::chrono::milliseconds{0};

  m.discover_all_rows();

  if (sld)
  {
    m.update_gso();
    sld_potential = m.get_slide_potential(0, num_rows, param.block_size);
  }

  // the following is necessary, since sd-bkz starts with a dual tour and
  // svp_reduction calls size_reduction, which needs to be preceeded by a
  // call to lll lower blocks to avoid seg faults
  if (sd)
  {
    lll_obj.lll(0, 0, num_rows, 0);
  }

  int kappa_max = -1;
  bool clean    = true;
  for (int i = 0;; ++i)
  {
    if (param.max_loops && i >= *param.max_loops)
    {
      final_status = RedStatus::bkz_loops_limit;
      break;
    }
    if (param.max_time && (detail::cpu_time() - cputime_start) >= *param.max_time)
    {
      final_status = RedStatus::bkz_time_limit;
      break;
    }
    if (auto_abort &&
        auto_abort_obj.test_abort(param.auto_abort_scale, param.auto_abort_max_no_dec))
    {
      break;
    }

    try
    {
      if (sd)
      {
        clean = sd_tour(i, run_param, 0, num_rows);
      }
      else if (sld)
      {
        clean = slide_tour(i, run_param, 0, num_rows);
      }
      else
      {
        clean = tour(i, kappa_max, run_param, 0, num_rows);
      }
    }
    catch (const BKZFailure &failure)
    {
      return set_status(failure.status());
    }

    ++tours;

    // if we do hkz reduction, we only need one tour
    if (clean || run_param.block_size >= num_rows)
    {
      break;
    }
  }

  // some post processing
  int dummy_kappa_max = num_rows;
  if (sd)
  {
    try
    {
      // hkz reduce the last window, which sd leaves unreduced
      hkz(dummy_kappa_max, run_param, num_rows - param.block_size, num_rows);
    }
    catch (const BKZFailure &failure)
    {
      return set_status(failure.status());
    }
  }
  if (sld)
  {
    try
    {
      // hkz reduce the blocks (which are otherwise only svp and dual svp reduced)
      int p = num_rows / param.block_size;
      if (num_rows % param.block_size)
      {
        ++p;
      }
      for (int j = 0; j < p; ++j)
      {
        const int kappa = j * param.block_size + 1;
        const int end   = std::min(num_rows, kappa + param.block_size - 1);
        hkz(dummy_kappa_max, run_param, kappa, end);
      }
    }
    catch (const BKZFailure &failure)
    {
      return set_status(failure.status());
    }
  }

  return set_status(final_status);
}

template <class ZT, class FT>
void BKZReduction<ZT, FT>::print_tour(const int loop, int min_row, int max_row)
{
  FT r0;
  FP_NR<mpfr_t> fr0;
  long expo;
  r0  = m.get_r_exp(min_row, min_row, expo);
  fr0 = r0.get_d();
  fr0.mul_2si(fr0, expo);
  // FP_NR's stream operator formats with ostream::precision().  The historical
  // chain set that precision to 3 before printing fr0, so render it the same
  // way, but into a local stream: log_stream() must keep its own state.
  std::ostringstream rendered_fr0;
  rendered_fr0.precision(3);
  rendered_fr0 << fr0;
  log_stream() << std::format(
                      "End of {} loop {:>4}, time = {:>9.3f}s, r_{} = ", algorithm, loop,
                      std::chrono::duration<double>(detail::cpu_time() - cputime_start).count(),
                      min_row)
               << rendered_fr0.str()
               << std::format(", slope = {:>9.6f}, log2(nodes) = {:>9.6f}\n",
                              m.get_current_slope(min_row, max_row),
                              std::log2(static_cast<double>(nodes)));
}

template <class ZT, class FT>
void BKZReduction<ZT, FT>::print_params(const BKZRunParameters &run_param, std::ostream &out)
{
  const std::string max_loops =
      param.max_loops ? std::format("{:>3}", *param.max_loops) : std::string("  -");
  const std::string max_time =
      param.max_time ? std::format("{:.1f}", std::chrono::duration<double>(*param.max_time).count())
                     : std::string("  -");
  const std::string auto_abort =
      param.auto_abort ? std::format("autoAbort: ({:.4f}, {:>2}), ", param.auto_abort_scale,
                                     param.auto_abort_max_no_dec)
                       : std::string("autoAbort: (     -,  -), ");
  const std::string gh_factor = run_param.gh_bound
                                    ? std::format("gh_factor: {:.2f}", run_param.gh_factor)
                                    : std::string("gh_factor: -");
  out << std::format("block size: {:>3}, variant: {}, delta: {:.4f}, max_loops: {}, "
                     "max_time: {}, {}{}\n",
                     param.block_size, variant_name(param.variant), param.delta, max_loops,
                     max_time, auto_abort, gh_factor);
}

template <class ZT, class FT> bool BKZReduction<ZT, FT>::set_status(RedStatus new_status)
{
  status = new_status;
  if (param.verbose)
  {
    if (status == RedStatus::success)
    {
      log_stream() << "End of " << algorithm << ": success" << '\n';
    }
    else
    {
      log_stream() << "End of " << algorithm << ": failure: " << to_string(status) << '\n';
    }
  }
  return status == RedStatus::success;
}

template <class ZT, class FT> bool BKZAutoAbort<ZT, FT>::test_abort(double scale, int maxNoDec)
{
  const double new_slope = -m.get_current_slope(start_row, num_rows);
  if (no_dec == -1 || new_slope < scale * old_slope)
  {
    no_dec = 0;
  }
  else
  {
    no_dec++;
  }
  old_slope = std::min(old_slope, new_slope);
  return no_dec >= maxNoDec;
}

/** explicit instantiations **/

#define FPLLL_INSTANTIATE_BKZ(ZT, FT)                                                              \
  template class BKZReduction<ZT, FT>;                                                             \
  template class BKZAutoAbort<ZT, FT>;

FPLLL_INSTANTIATE_BKZ(Z_NR<long>, FP_NR<double>)
FPLLL_INSTANTIATE_BKZ(Z_NR<mpz_t>, FP_NR<double>)
#ifdef FPLLL_WITH_QD
FPLLL_INSTANTIATE_BKZ(Z_NR<long>, FP_NR<dd_real>)
FPLLL_INSTANTIATE_BKZ(Z_NR<mpz_t>, FP_NR<dd_real>)
FPLLL_INSTANTIATE_BKZ(Z_NR<long>, FP_NR<qd_real>)
FPLLL_INSTANTIATE_BKZ(Z_NR<mpz_t>, FP_NR<qd_real>)
#endif
FPLLL_INSTANTIATE_BKZ(Z_NR<long>, FP_NR<mpfr_t>)
FPLLL_INSTANTIATE_BKZ(Z_NR<mpz_t>, FP_NR<mpfr_t>)

#undef FPLLL_INSTANTIATE_BKZ

}  // namespace fplll
