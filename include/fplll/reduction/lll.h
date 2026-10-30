// SPDX-FileCopyrightText: 2005-2008 Damien Stehle
// SPDX-FileCopyrightText: 2007 David Cade
// SPDX-FileCopyrightText: 2011 Xavier Pujol
// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef FPLLL_LLL_H
#define FPLLL_LLL_H

#include "../defs.h"
#include "../nr/matrix.h"
#include <gmp.h>

namespace fplll
{

/** Parameters of an LLL reduction. */
struct LLLParameters
{
  /** Lovász condition threshold, in (1/4, 1). */
  double delta = LLL_DEF_DELTA;
  /** Size-reduction bound, in (1/2, 1). */
  double eta = LLL_DEF_ETA;
  /** Print progress information to log_stream(). */
  bool verbose = false;
  /** Size-reduce newly discovered vectors early. */
  bool early_reduction = false;
  /** Test the Siegel condition instead of the Lovász condition. */
  bool siegel = false;
};

/** Options of the high-level LLL entry points. */
struct LLLOptions
{
  LLLParameters parameters;
  /** LLL variant to run. */
  LLLMethod method = LLLMethod::wrapper;
  /** Floating-point backend.  `automatic` picks the cheapest backend that
      provides the precision the algorithm needs; a non-zero `precision`
      selects MPFR. */
  FloatType float_type = FloatType::automatic;
  /** MPFR precision.  A non-zero value selects the MPFR backend at that
      precision (also with `FloatType::automatic`).  0 means the precision is
      selected by the algorithm: the proved method uses the precision the
      proof needs and the other methods use double precision. */
  int precision = 0;
};

/**
   @brief LLL-reduces the basis `b` in place.

   The options are validated before `b` is modified, so an invalid option
   throws `fplll::Error` and leaves `b` untouched.

   @param b        basis of the lattice to reduce
   @param options  reduction options
   @return         the report: status, backend/precision actually used and
                   whether the proved guarantee was achieved
 */
[[nodiscard]] ReductionReport lll_reduction(ZZ_mat<mpz_t> &b, const LLLOptions &options = {});

/**
   @brief LLL-reduces the basis `b` and stores the transformation matrix in `u`.

   `u` is overwritten with the unimodular transformation that maps the input
   basis to the reduced one.  The options are validated before `b` or `u` is
   modified, so an invalid option throws `fplll::Error` and leaves both
   untouched.

   @param b        basis of the lattice to reduce
   @param u        transformation matrix, overwritten
   @param options  reduction options
   @return         the report: status, backend/precision actually used and
                   whether the proved guarantee was achieved
 */
[[nodiscard]] ReductionReport lll_reduction(ZZ_mat<mpz_t> &b, ZZ_mat<mpz_t> &u,
                                            const LLLOptions &options = {});

/**
   @brief LLL-reduces the basis `b`, storing `u` and its inverse in `u_inv`.

   Like the two-argument overloads, this validates the options before modifying
   any matrix; an invalid option throws `fplll::Error` and leaves `b`, `u` and
   `u_inv` untouched.

   @param b        basis of the lattice to reduce
   @param u        transformation matrix, overwritten
   @param u_inv    inverse of the transformation matrix, overwritten
   @param options  reduction options
   @return         the report: status, backend/precision actually used and
                   whether the proved guarantee was achieved
 */
[[nodiscard]] ReductionReport lll_reduction(ZZ_mat<mpz_t> &b, ZZ_mat<mpz_t> &u,
                                            ZZ_mat<mpz_t> &u_inv, const LLLOptions &options = {});

}  // namespace fplll

#endif
