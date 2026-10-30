// SPDX-FileCopyrightText: 2005-2008 Damien Stehle
// SPDX-FileCopyrightText: 2007 David Cade
// SPDX-FileCopyrightText: 2011 Xavier Pujol
// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

/* Template source file */

#include "lll_kernel.h"
#include "detail/cpu_time.h"

#include <fplll/defs.h>
#include <fplll/gso/gso_interface.h>
#include <fplll/nr/nr.h>
#include <fplll/nr/numvect.h>
#include <fplll/reduction/lll.h>

#include <cmath>
#include <cstdint>
#include <gmp.h>
#include <limits>

namespace fplll
{

namespace
{

bool is_power_of_2(int i) { return (i & (i - 1)) == 0; }

}  // namespace

template <class ZT, class FT>
LLLReduction<ZT, FT>::LLLReduction(MatGSOInterface<ZT, FT> &m, const LLLParameters &parameters)
    : m(m)
{
  /* No early reduction in proved mode (i.e. enable_int_gram=true).
     NOTE: To make this possible, the hypothesis "g(i, j) is valid if
     0 <= i < n_known_rows and j <= i" in gso.h should be changed and
     MatGSOInterface<ZT, FT>::discover_row() should be rewritten. */
  enable_early_red = parameters.early_reduction && !m.enable_int_gram;
  siegel           = parameters.siegel;
  verbose          = parameters.verbose;
  delta            = parameters.delta;
  eta              = parameters.eta;
  swap_threshold   = siegel ? delta - eta * eta : delta;
  zeros            = 0;
}

template <class ZT, class FT>
bool LLLReduction<ZT, FT>::lll(int kappa_min, int kappa_start, int kappa_end,
                               int size_reduction_start)
{
  if (kappa_end == -1)
  {
    kappa_end = m.get_d();
  }

  FPLLL_DEBUG_CHECK(kappa_min <= kappa_start && kappa_start < kappa_end && kappa_end <= m.get_d());
  const std::int64_t start_time = verbose ? detail::cpu_time_milliseconds() : 0;
  int kappa                     = kappa_start + 1;
  int kappa_max                 = 0;
  const int d                   = kappa_end - kappa_min;

  zeros       = 0;
  n_swaps     = 0;
  final_kappa = 0;
  if (verbose)
  {
    print_params();
  }
  extend_vect(lovasz_tests, kappa_end);
  extend_vect(babai_mu, kappa_end);
  extend_vect(babai_expo, kappa_end);

  for (; zeros < d && m.b_row_is_zero(0); zeros++)
  {
    m.move_row(kappa_min, kappa_end - 1 - zeros);
  }

  if (zeros < d && ((kappa_start > 0 && !babai(kappa_start, kappa_start, size_reduction_start)) ||
                    !m.update_gso_row(kappa_start)))
  {
    final_kappa = kappa_start;
    return false;
  }

  long long iter, max_iter;
  max_iter = static_cast<long long>(d - 2 * d * (d + 1) *
                                            ((m.get_max_exp_of_b() + 3) / std::log(delta.get_d())));

  for (iter = 0; iter < max_iter && kappa < kappa_end - zeros; iter++)
  {
    if (kappa > kappa_max)
    {
      if (verbose)
      {
        log_stream() << "Discovering vector " << kappa - kappa_min + 1 + zeros << "/" << d
                     << " cputime=" << detail::cpu_time_milliseconds() - start_time << '\n';
      }
      kappa_max = kappa;
      if (enable_early_red && is_power_of_2(kappa) && kappa > last_early_red)
      {
        if (!early_reduction(kappa, size_reduction_start))
        {
          final_kappa = kappa;
          return false;
        }
      }
    }

    // Lazy size reduction
    if (!babai(kappa, kappa, size_reduction_start))
    {
      final_kappa = kappa;
      return false;
    }

    // Tests Lovasz's condition
    m.get_gram(lovasz_tests[0], kappa, kappa);
    for (int i = 1; i <= kappa; i++)
    {
      ftmp1.mul(m.get_mu_exp(kappa, i - 1), m.get_r_exp(kappa, i - 1));
      lovasz_tests[i].sub(lovasz_tests[i - 1], ftmp1);
    }
    ftmp1.mul(m.get_r_exp(kappa - 1, kappa - 1), swap_threshold);
    if (m.enable_row_expo)
    {
      ftmp1.mul_2si(ftmp1, 2 * (m.get_row_expo(kappa - 1) - m.get_row_expo(kappa)));
    }

    if (ftmp1 > lovasz_tests[siegel ? kappa : kappa - 1])
    {
      n_swaps++;
      // Failure, computes the insertion index
      const int old_k = kappa;
      for (kappa--; kappa > kappa_min; kappa--)
      {
        ftmp1.mul(m.get_r_exp(kappa - 1, kappa - 1), swap_threshold);
        if (m.enable_row_expo)
        {
          ftmp1.mul_2si(ftmp1, 2 * (m.get_row_expo(kappa - 1) - m.get_row_expo(old_k)));
        }
        if (ftmp1 < lovasz_tests[siegel ? kappa : kappa - 1])
        {
          break;
        }
      }
      // Moves the vector
      if (lovasz_tests[kappa] > 0)
      {
        m.move_row(old_k, kappa);
      }
      else
      {
        zeros++;
        m.move_row(old_k, kappa_end - zeros);
        kappa = old_k;
        continue;
      }
    }

    m.set_r(kappa, kappa, lovasz_tests[kappa]);
    kappa++;
  }
  // Symmetrize the Gram matrix
  if (m.enable_int_gram)
  {
    m.symmetrize_g();
  }

  if (kappa < kappa_end - zeros)
  {
    return set_status(RedStatus::lll_failure);
  }
  else
  {
    return set_status(RedStatus::success);
  }
}

template <class ZT, class FT>
bool LLLReduction<ZT, FT>::babai(int kappa, int size_reduction_end, int size_reduction_start)
{
  long max_expo = std::numeric_limits<long>::max();

  for (int iter = 0;; iter++)
  {
    if (!m.update_gso_row(kappa, size_reduction_end - 1))
    {
      return set_status(RedStatus::gso_failure);
    }

    bool loop_needed = false;
    for (int j = size_reduction_end - 1; j >= size_reduction_start && !loop_needed; j--)
    {
      m.get_mu(ftmp1, kappa, j);
      ftmp1.abs(ftmp1);
      loop_needed |= (ftmp1 > eta);
    }
    if (!loop_needed)
    {
      break;
    }

    if (iter >= 2)
    {
      const long new_max_expo = m.get_max_mu_exp(kappa, size_reduction_end);
      if (new_max_expo > max_expo - SIZE_RED_FAILURE_THRESH)
      {
        return set_status(RedStatus::babai_failure);
      }
      max_expo = new_max_expo;
    }

    for (int j = size_reduction_start; j < size_reduction_end; j++)
    {
      babai_mu[j] = m.get_mu_exp(kappa, j, babai_expo[j]);
    }
    {
      auto batch = m.row_operations(kappa, kappa + 1);
      for (int j = size_reduction_end - 1; j >= size_reduction_start; j--)
      {
        mu_m_ant.rnd_we(babai_mu[j], babai_expo[j]);
        if (mu_m_ant.zero_p())
        {
          continue;
        }
        // Approximate update of the mu_(kappa,k)'s
        for (int k = size_reduction_start; k < j; k++)
        {
          ftmp1.mul(mu_m_ant, m.get_mu_exp(j, k));
          /* When enable_row_expo=true, the following line relies on the fact that
             get_mu_exp(a, b, expo) returns expo = row_expo[a] - row_expo[b]. */
          babai_mu[k].sub(babai_mu[k], ftmp1);
        }
        // Operation on the basis
        mu_m_ant.neg(mu_m_ant);
        m.row_addmul_we(kappa, j, mu_m_ant, babai_expo[j]);
      }
    }
  }
  return true;
}

template <class ZT, class FT>
bool LLLReduction<ZT, FT>::size_reduction(int kappa_min, int kappa_end, int size_reduction_start)
{
  if (kappa_end == -1)
  {
    kappa_end = m.get_d();
  }

  extend_vect(babai_mu, kappa_end);
  extend_vect(babai_expo, kappa_end);

  for (int k = kappa_min; k < kappa_end; k++)
  {
    if ((k > 0 && !babai(k, k, size_reduction_start)) || !m.update_gso_row(k))
    {
      return false;
    }
  }
  return set_status(RedStatus::success);
}

template <class ZT, class FT>
bool LLLReduction<ZT, FT>::early_reduction(int start, int size_reduction_start)
{
  m.lock_cols();
  if (verbose)
  {
    log_stream() << "Early reduction start=" << start + 1 << '\n';
  }
  for (int i = start; i < m.get_d(); i++)
  {
    if (!babai(i, start, size_reduction_start))
    {
      return false;
    }
  }
  m.unlock_cols();
  last_early_red = start;
  return true;
}

template <class ZT, class FT> void LLLReduction<ZT, FT>::print_params()
{
  log_stream() << "Entering LLL" << "\ndelta = " << delta << "\neta = " << eta
               << "\nprecision = " << FT::get_prec()
               << "\nexact_dot_product = " << static_cast<int>(m.enable_int_gram)
               << "\nrow_expo = " << static_cast<int>(m.enable_row_expo)
               << "\nearly_red = " << static_cast<int>(enable_early_red)
               << "\nsiegel_cond = " << static_cast<int>(siegel)
               << "\nlong_in_babai = " << static_cast<int>(m.row_op_force_long) << '\n';
}

template <class ZT, class FT> bool LLLReduction<ZT, FT>::set_status(RedStatus new_status)
{
  status = new_status;
  if (verbose)
  {
    if (status == RedStatus::success)
    {
      log_stream() << "End of LLL: success" << '\n';
    }
    else
    {
      log_stream() << "End of LLL: failure: " << to_string(status) << '\n';
    }
  }
  return status == RedStatus::success;
}

template <class ZT, class FT>
bool is_lll_reduced(MatGSOInterface<ZT, FT> &m, const LLLParameters &parameters)
{
  FT ftmp1;
  FT ftmp2;
  FT delta_ = parameters.delta;
  m.update_gso();
  for (int i = 0; i < m.get_d(); i++)
  {
    for (int j = 0; j < i; j++)
    {
      m.get_mu(ftmp1, i, j);
      ftmp1.abs(ftmp1);
      if (ftmp1 > parameters.eta)
      {
        return false;
      }
    }
  }
  for (int i = 1; i < m.get_d(); i++)
  {
    m.get_mu(ftmp2, i, i - 1);
    ftmp2.mul(ftmp2, ftmp2);  // μ^2

    ftmp2.sub(delta_, ftmp2);  // δ - μ^2
    m.get_r(ftmp1, i - 1, i - 1);
    ftmp2.mul(ftmp1, ftmp2);  // (δ - μ^2) ⋅ r_{i-1,i-1}

    m.get_r(ftmp1, i, i);  // r_{i,i}

    if (ftmp1 < ftmp2)
    {
      return false;
    }
  }
  return true;
}

/** explicit instantiations **/

#define FPLLL_INSTANTIATE_LLL(ZT, FT)                                                              \
  template class LLLReduction<ZT, FT>;                                                             \
  template bool is_lll_reduced<ZT, FT>(MatGSOInterface<ZT, FT> & m,                                \
                                       const LLLParameters &parameters);

FPLLL_INSTANTIATE_LLL(Z_NR<long>, FP_NR<double>)
FPLLL_INSTANTIATE_LLL(Z_NR<mpz_t>, FP_NR<double>)
#ifdef FPLLL_WITH_QD
FPLLL_INSTANTIATE_LLL(Z_NR<long>, FP_NR<dd_real>)
FPLLL_INSTANTIATE_LLL(Z_NR<mpz_t>, FP_NR<dd_real>)
FPLLL_INSTANTIATE_LLL(Z_NR<long>, FP_NR<qd_real>)
FPLLL_INSTANTIATE_LLL(Z_NR<mpz_t>, FP_NR<qd_real>)
#endif
FPLLL_INSTANTIATE_LLL(Z_NR<long>, FP_NR<mpfr_t>)
FPLLL_INSTANTIATE_LLL(Z_NR<mpz_t>, FP_NR<mpfr_t>)

#undef FPLLL_INSTANTIATE_LLL

}  // namespace fplll
