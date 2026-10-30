// SPDX-FileCopyrightText: 2005-2008 Damien Stehle
// SPDX-FileCopyrightText: 2007 David Cade
// SPDX-FileCopyrightText: 2011 Xavier Pujol
// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef FPLLL_REDUCTION_LLL_KERNEL_H
#define FPLLL_REDUCTION_LLL_KERNEL_H

#include <fplll/defs.h>
#include <fplll/gso/gso_interface.h>
#include <fplll/nr/nr.h>
#include <fplll/reduction/lll.h>

#include <gmp.h>
#include <vector>

namespace fplll
{

/* The precision of FT must be defined before creating an instance of
   LLLReduction. The GSO object can be modified between each call to lll(). */
template <class ZT, class FT> class LLLReduction
{
public:
  /**
   * Constructor.
   * The precision of FT must be defined before creating an instance of the
   * class and must remain the same until the object is destroyed (or no longer
   * needed).
   */
  LLLReduction(MatGSOInterface<ZT, FT> &m, const LLLParameters &parameters = {});
  /**
     @brief LLL reduction.

     @param kappa_min minimal index to go back to
     @param kappa_start index to start processing at
     @param kappa_end end index (exclusive)
     @param size_reduction_start only perform size reductions using vectors starting at this index
     @return success or failure (due to numerical instability)
  */

  bool lll(int kappa_min = 0, int kappa_start = 0, int kappa_end = -1,
           int size_reduction_start = 0);

  /**
     @brief Size reduction.

     Perform size reduction for all vectors between `kappa_start` and `kappa_end`.

     @param kappa_min start index
     @param kappa_end end index (exclusive)
     @param size_reduction_start only perform size reductions using vectors starting at this index
     @return success or failure (due to numerical instability)
  */

  bool size_reduction(int kappa_min = 0, int kappa_end = -1, int size_reduction_start = 0);

  RedStatus status   = RedStatus::success;
  int final_kappa    = 0;
  int last_early_red = 0;
  int zeros          = 0;
  int n_swaps        = 0;

private:
  /**
     @brief Size reduction.

     @param kappa index to size reduce
     @param size_reduction_end only perform size reductions using vectors up to this index
     @param size_reduction_start only perform size reductions using vectors starting at this index
     @return
  */

  bool babai(int kappa, int size_reduction_end, int size_reduction_start = 0);
  bool early_reduction(int start, int size_reduction_start = 0);
  void print_params();
  bool set_status(RedStatus new_status);

  MatGSOInterface<ZT, FT> &m;
  FT delta, eta, swap_threshold;

  bool enable_early_red;
  bool siegel;
  bool verbose;

  std::vector<FT> lovasz_tests;
  std::vector<FT> babai_mu;
  std::vector<long> babai_expo;
  FT mu_m_ant, ftmp1;
};

/** @brief Tests whether the basis described by `m` is LLL-reduced. */
template <class ZT, class FT>
[[nodiscard]] bool is_lll_reduced(MatGSOInterface<ZT, FT> &m, const LLLParameters &parameters);

extern template class LLLReduction<Z_NR<long>, FP_NR<double>>;
extern template class LLLReduction<Z_NR<mpz_t>, FP_NR<double>>;
extern template class LLLReduction<Z_NR<long>, FP_NR<mpfr_t>>;
extern template class LLLReduction<Z_NR<mpz_t>, FP_NR<mpfr_t>>;
#ifdef FPLLL_WITH_QD
extern template class LLLReduction<Z_NR<long>, FP_NR<dd_real>>;
extern template class LLLReduction<Z_NR<mpz_t>, FP_NR<dd_real>>;
extern template class LLLReduction<Z_NR<long>, FP_NR<qd_real>>;
extern template class LLLReduction<Z_NR<mpz_t>, FP_NR<qd_real>>;
#endif

extern template bool
is_lll_reduced<Z_NR<long>, FP_NR<double>>(MatGSOInterface<Z_NR<long>, FP_NR<double>> &m,
                                          const LLLParameters &parameters);
extern template bool
is_lll_reduced<Z_NR<mpz_t>, FP_NR<double>>(MatGSOInterface<Z_NR<mpz_t>, FP_NR<double>> &m,
                                           const LLLParameters &parameters);
extern template bool
is_lll_reduced<Z_NR<long>, FP_NR<mpfr_t>>(MatGSOInterface<Z_NR<long>, FP_NR<mpfr_t>> &m,
                                          const LLLParameters &parameters);
extern template bool
is_lll_reduced<Z_NR<mpz_t>, FP_NR<mpfr_t>>(MatGSOInterface<Z_NR<mpz_t>, FP_NR<mpfr_t>> &m,
                                           const LLLParameters &parameters);
#ifdef FPLLL_WITH_QD
extern template bool
is_lll_reduced<Z_NR<long>, FP_NR<dd_real>>(MatGSOInterface<Z_NR<long>, FP_NR<dd_real>> &m,
                                           const LLLParameters &parameters);
extern template bool
is_lll_reduced<Z_NR<mpz_t>, FP_NR<dd_real>>(MatGSOInterface<Z_NR<mpz_t>, FP_NR<dd_real>> &m,
                                            const LLLParameters &parameters);
extern template bool
is_lll_reduced<Z_NR<long>, FP_NR<qd_real>>(MatGSOInterface<Z_NR<long>, FP_NR<qd_real>> &m,
                                           const LLLParameters &parameters);
extern template bool
is_lll_reduced<Z_NR<mpz_t>, FP_NR<qd_real>>(MatGSOInterface<Z_NR<mpz_t>, FP_NR<qd_real>> &m,
                                            const LLLParameters &parameters);
#endif

}  // namespace fplll

#endif
