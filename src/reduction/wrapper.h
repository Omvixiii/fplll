// SPDX-FileCopyrightText: 2005-2008 Damien Stehle
// SPDX-FileCopyrightText: 2007 David Cade
// SPDX-FileCopyrightText: 2011 Xavier Pujol
// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef FPLLL_REDUCTION_WRAPPER_H
#define FPLLL_REDUCTION_WRAPPER_H

#include <gmp.h>

#include <fplll/defs.h>
#include <fplll/nr/matrix.h>
#include <fplll/reduction/hlll.h>
#include <fplll/reduction/lll.h>

namespace fplll
{

/* The matrix b must not be modified before calling lll().
   lll() must be called only once. */

/**
 * @brief Self-tuning backend and precision escalation for LLL and HLLL.
 *
 * This class implements the `LLLMethod::wrapper` path that the LLL/HLLL
 * entry points delegate to: it selects the floating-point backend and
 * precision, retries a numerically failed run at a higher precision and calls
 * the low-level ``LLLReduction``/``HLLLReduction`` kernels.  It is an
 * implementation detail; the installed API is the free functions in
 * ``<fplll/reduction/lll.h>`` and ``<fplll/reduction/hlll.h>``.
 *
 * All of the parameters passed to this class are non-const references, and the
 * methods overwrite ``u`` with the transformation matrix applied to ``b`` to
 * produce the reduced basis.  In other words, if ``C`` is the reduced basis,
 * then ``C = U B`` for some unimodular transformation matrix ``U``.  ``u`` must
 * therefore be either empty (transform disabled) or the identity matrix.
 *
 * Similarly, the parameter ``u_inv`` receives the inverse matrix ``U^-1``,
 * which recovers the original basis from the reduced one.  All operations on
 * this matrix are carried out on its transpose (see gso_interface.h); the
 * driver transposes it around the call.
 */
class Wrapper
{
public:
  /* u must be either empty or the identity matrix */
  Wrapper(ZZ_mat<mpz_t> &b, ZZ_mat<mpz_t> &u, ZZ_mat<mpz_t> &u_inv,
          const LLLParameters &parameters);

  /* Used for HLLL */
  Wrapper(ZZ_mat<mpz_t> &b, ZZ_mat<mpz_t> &u, ZZ_mat<mpz_t> &u_inv,
          const HLLLParameters &parameters);

  bool lll();

  bool hlll();

  RedStatus status;

  /** Report of the last lll()/hlll() call: the backend/precision actually used
      by the final low-level call, its status and whether that call was the
      proved variant at a sufficient precision. */
  ReductionReport report;

private:
  ZZ_mat<mpz_t> &b;
  ZZ_mat<mpz_t> &u;
  ZZ_mat<mpz_t> &u_inv;

  ZZ_mat<long> b_long;

  LLLParameters lll_parameters;
  HLLLParameters hlll_parameters;

  int good_prec;
  bool use_long;

  bool little(int kappa, int precision);

  template <class Z, class F>
  int call_lll(ZZ_mat<Z> &bz, ZZ_mat<Z> &uz, ZZ_mat<Z> &u_inv_z, LLLMethod method, int precision,
               double delta, double eta);

  template <class F> int fast_lll(double delta, double eta);

  template <class Z, class F>
  int heuristic_lll(ZZ_mat<Z> &bz, ZZ_mat<Z> &uz, ZZ_mat<Z> &u_inv_z, int precision, double delta,
                    double eta);

  template <class Z, class F>
  int proved_lll(ZZ_mat<Z> &bz, ZZ_mat<Z> &uz, ZZ_mat<Z> &u_inv_z, int precision, double delta,
                 double eta);

  int heuristic_loop(int precision);
  int proved_loop(int precision);
  int last_lll();

  void set_use_long(bool value);
  int increase_prec(int precision);

  int max_exponent;
  int n;
  int d;
  int last_early_red;
  bool last_call_proved = false;

  // High level function: selects the best way to call HLLL depending on the
  // method and the precision.
  template <class F> bool call_hlll(LLLMethod method, int precision);

  template <class F> bool fast_hlll();

  template <class F> bool proved_hlll(int precision);

  // Performs proved_hlll until good_prec is reached or the lattice is reduced.
  int hlll_proved_loop(int precision);

  bool last_hlll();
};

}  // namespace fplll

#endif
