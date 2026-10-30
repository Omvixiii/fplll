// SPDX-FileCopyrightText: 2016-2019 Martin R. Albrecht
// SPDX-FileCopyrightText: 2016-2017 Shi Bai
// SPDX-FileCopyrightText: 2017 Vaggelis1994
// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

/************************
 * Extra utils for NR_Z
 ************************/

#ifndef FPLLL_NR_Z_MISC_H
#define FPLLL_NR_Z_MISC_H

#include <cmath>  // IWYU pragma: keep

namespace fplll
{

/* -----------------------------
 *   get_f_exp (Z_NR --> FP_NR)
 * ----------------------------- */

/* get_f_exp (long to double) */
template <> template <> inline void Z_NR<long>::get_f_exp(FP_NR<double> &f, long &expo)
{
  int int_expo;
  f.get_data() = std::frexp(static_cast<double>(data), &int_expo);
  expo         = int_expo;
}

#ifdef FPLLL_WITH_QD
template <> template <> inline void Z_NR<long>::get_f_exp(FP_NR<dd_real> &f, long &expo)
{
  int int_expo;
  f.get_data() = std::frexp(static_cast<double>(data), &int_expo);
  expo         = int_expo;
}
template <> template <> inline void Z_NR<long>::get_f_exp(FP_NR<qd_real> &f, long &expo)
{
  int int_expo;
  f.get_data() = std::frexp(static_cast<double>(data), &int_expo);
  expo         = int_expo;
}
#endif

template <> template <> inline void Z_NR<long>::get_f_exp(FP_NR<mpfr_t> & /*f*/, long & /*expo*/)
{
  FPLLL_ABORT("get_f_exp unimplemented for mpfr_t");
}

/* get_f_exp (mpz_t to double) */
template <> template <> inline void Z_NR<mpz_t>::get_f_exp(FP_NR<double> &f, long &expo)
{
  f.get_data() = mpz_get_d_2exp(&expo, data);
}

#ifdef FPLLL_WITH_QD
template <> template <> inline void Z_NR<mpz_t>::get_f_exp(FP_NR<dd_real> &f, long &expo)
{
  f.get_data() = mpz_get_d_2exp(&expo, data);
}
template <> template <> inline void Z_NR<mpz_t>::get_f_exp(FP_NR<qd_real> &f, long &expo)
{
  f.get_data() = mpz_get_d_2exp(&expo, data);
}
#endif

template <> template <> inline void Z_NR<mpz_t>::get_f_exp(FP_NR<mpfr_t> &f, long &expo)
{
  f = mpz_get_d_2exp(&expo, data);
}

/* -------------------------
 *   set_f (FP_NR --> Z_NR)
 * ------------------------- */

/* set_f (FP_NR to long) */
template <> template <> inline void Z_NR<long>::set_f(const FP_NR<double> &a) { data = a.get_si(); }

#ifdef FPLLL_WITH_QD
template <> template <> inline void Z_NR<long>::set_f(const FP_NR<dd_real> &a)
{
  data = a.get_si();
}

template <> template <> inline void Z_NR<long>::set_f(const FP_NR<qd_real> &a)
{
  data = a.get_si();
}
#endif

template <> template <> inline void Z_NR<long>::set_f(const FP_NR<mpfr_t> &a) { data = a.get_si(); }

/* set_f (FP_NR to mpz_t) */
template <> template <> inline void Z_NR<mpz_t>::set_f(const FP_NR<double> &a)
{
  mpz_set_d(data, a.get_data());
}

#ifdef FPLLL_WITH_QD
template <> template <> inline void Z_NR<mpz_t>::set_f(const FP_NR<dd_real> &a)
{
  mpfr_t t;
  mpfr_init(t);
  a.get_mpfr(t, GMP_RNDN);
  mpfr_get_z(data, t, GMP_RNDN);
  mpfr_clear(t);
}

template <> template <> inline void Z_NR<mpz_t>::set_f(const FP_NR<qd_real> &a)
{
  mpfr_t t;
  mpfr_init(t);
  a.get_mpfr(t, GMP_RNDN);
  mpfr_get_z(data, t, GMP_RNDN);
  mpfr_clear(t);
}
#endif

template <> template <> inline void Z_NR<mpz_t>::set_f(const FP_NR<mpfr_t> &a)
{
  mpfr_get_z(data, a.get_data(), GMP_RNDN);
}

}  // namespace fplll

#endif
