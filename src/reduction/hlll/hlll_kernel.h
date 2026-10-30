// SPDX-FileCopyrightText: 2005-2008 Damien Stehle
// SPDX-FileCopyrightText: 2007 David Cade
// SPDX-FileCopyrightText: 2011 Xavier Pujol
// SPDX-FileCopyrightText: 2017-2018 Laurent Grémy
// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef FPLLL_REDUCTION_HLLL_KERNEL_H
#define FPLLL_REDUCTION_HLLL_KERNEL_H

#include <fplll/defs.h>
#include <fplll/gso/householder.h>
#include <fplll/nr/nr.h>
#include <fplll/reduction/hlll.h>

#include <gmp.h>
#include <vector>

namespace fplll
{

/* The precision of FT must be defined before creating an instance of
   HLLLReduction. */
template <class ZT, class FT> class HLLLReduction
{
public:
  /**
   * Constructor.
   * The precision of FT must be defined before creating an instance of the
   * class and must remain the same until the object is destroyed (or no longer
   * needed).
   */
  HLLLReduction(MatHouseholder<ZT, FT> &m, const HLLLParameters &parameters = {});

  /**
    @brief Householder LLL reduction.
    @return true if the basis is HLLL-reduced, false on numerical failure.
    */
  bool hlll();

  /** Status of the last reduction. */
  [[nodiscard]] RedStatus get_status() const { return status; }

private:
  // Parameters to (delta, eta, theta) hlll-reduce the basis b in m.
  FT delta, eta, theta;
  MatHouseholder<ZT, FT> &m;

  // Verbose mode.
  bool verbose;

  // Temporary variables
  FT ftmp0, ftmp1, ftmp2;
  long expo0 = 0, expo1 = 0, expo2 = 0;

  RedStatus status = RedStatus::success;

  /**
     @brief Size reduction.

     Perform size reduction of b[kappa]. Reduce b[kappa] with
     b[size_reduction_start..size_reduction_end-1].

     @param kappa index of the vector
  */
  void size_reduction(int kappa, int size_reduction_end, int size_reduction_start = 0);

  /** In verbose mode, print the parameters of the computation. */
  void print_params();

  // Precompute dR[k] * 2^(2*row_expo[k]) = delta_ * R(k, k)^2
  std::vector<FT> dR;

  // Compute dR[k]
  void compute_dR(int k);

  // Set the status of the computation and print a message if verbose
  bool set_status(RedStatus new_status);

  // Precompute eR[k] * 2^row_expo[k] = eta * R(k, k)
  std::vector<FT> eR;

  // Compute eR[k]
  void compute_eR(int k);

  // Verify if b[k] is correctly size reduced
  bool verify_size_reduction(int kappa);

  // Test if delta * R(k - 1, k - 1)^2 <= ||b[k]||^2 - sum_{i = 0}^{i < k - 1}R[k][i]^2 (depending
  // on the way ftmp1 is computed, this test can be slightly different, but the purpose keeps the
  // same)
  bool lovasz_test(int k);
};

/** @brief Tests whether the basis described by `m` is HLLL-reduced. */
template <class ZT, class FT>
[[nodiscard]] bool is_hlll_reduced(MatHouseholder<ZT, FT> &m, const HLLLParameters &parameters);

extern template class HLLLReduction<Z_NR<long>, FP_NR<double>>;
extern template class HLLLReduction<Z_NR<mpz_t>, FP_NR<double>>;
extern template class HLLLReduction<Z_NR<long>, FP_NR<mpfr_t>>;
extern template class HLLLReduction<Z_NR<mpz_t>, FP_NR<mpfr_t>>;
#ifdef FPLLL_WITH_QD
extern template class HLLLReduction<Z_NR<long>, FP_NR<dd_real>>;
extern template class HLLLReduction<Z_NR<mpz_t>, FP_NR<dd_real>>;
extern template class HLLLReduction<Z_NR<long>, FP_NR<qd_real>>;
extern template class HLLLReduction<Z_NR<mpz_t>, FP_NR<qd_real>>;
#endif

extern template bool
is_hlll_reduced<Z_NR<long>, FP_NR<double>>(MatHouseholder<Z_NR<long>, FP_NR<double>> &m,
                                           const HLLLParameters &parameters);
extern template bool
is_hlll_reduced<Z_NR<mpz_t>, FP_NR<double>>(MatHouseholder<Z_NR<mpz_t>, FP_NR<double>> &m,
                                            const HLLLParameters &parameters);
extern template bool
is_hlll_reduced<Z_NR<long>, FP_NR<mpfr_t>>(MatHouseholder<Z_NR<long>, FP_NR<mpfr_t>> &m,
                                           const HLLLParameters &parameters);
extern template bool
is_hlll_reduced<Z_NR<mpz_t>, FP_NR<mpfr_t>>(MatHouseholder<Z_NR<mpz_t>, FP_NR<mpfr_t>> &m,
                                            const HLLLParameters &parameters);
#ifdef FPLLL_WITH_QD
extern template bool
is_hlll_reduced<Z_NR<long>, FP_NR<dd_real>>(MatHouseholder<Z_NR<long>, FP_NR<dd_real>> &m,
                                            const HLLLParameters &parameters);
extern template bool
is_hlll_reduced<Z_NR<mpz_t>, FP_NR<dd_real>>(MatHouseholder<Z_NR<mpz_t>, FP_NR<dd_real>> &m,
                                             const HLLLParameters &parameters);
extern template bool
is_hlll_reduced<Z_NR<long>, FP_NR<qd_real>>(MatHouseholder<Z_NR<long>, FP_NR<qd_real>> &m,
                                            const HLLLParameters &parameters);
extern template bool
is_hlll_reduced<Z_NR<mpz_t>, FP_NR<qd_real>>(MatHouseholder<Z_NR<mpz_t>, FP_NR<qd_real>> &m,
                                             const HLLLParameters &parameters);
#endif

}  // namespace fplll

#endif
