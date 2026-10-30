// SPDX-FileCopyrightText: 2016-2019 Martin R. Albrecht
// SPDX-FileCopyrightText: 2016-2017 Shi Bai
// SPDX-FileCopyrightText: 2017 Vaggelis1994
// SPDX-FileCopyrightText: 2017 Gilles Villard
// SPDX-FileCopyrightText: 2018 Laurent Grémy
// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

/************************
 *  Extra utils for NR_FP
 ************************/

#ifndef FPLLL_NR_FP_MISC_H
#define FPLLL_NR_FP_MISC_H

#include <algorithm>  // IWYU pragma: keep
#include <cmath>      // IWYU pragma: keep
#include <limits>     // IWYU pragma: keep

namespace fplll
{

/* -------------------------
 *   set_z (Z_NR --> FP_NR)
 * ------------------------- */

/** set_z (from long to double) */
// The rounding parameter is unnamed here; the suggested fix-it is invalid.
// NOLINTBEGIN(misc-unused-parameters)
template <> template <> inline void FP_NR<double>::set_z(const Z_NR<long> &a, mp_rnd_t /*rnd*/)
{
  data = a.get_d();
}

/** set_z (from default mpz_t to double) */
template <> template <> inline void FP_NR<double>::set_z(const Z_NR<mpz_t> &a, mp_rnd_t /*rnd*/)
// NOLINTEND(misc-unused-parameters)
{
  data = a.get_d();
}

/* set_z (to dd_real) */
#ifdef FPLLL_WITH_QD

/** set_z (from long to dd_real) */
template <> template <> inline void FP_NR<dd_real>::set_z(const Z_NR<long> &a, mp_rnd_t)
{
  data = a.get_data();
}

/** set_z (from default mpz_t to dd_real) */
template <> template <> inline void FP_NR<dd_real>::set_z(const Z_NR<mpz_t> &a, mp_rnd_t /*rnd*/)
{
  double hi = mpz_get_d(a.get_data());

  mpz_t tz;
  mpz_init(tz);
  mpz_set_d(tz, hi);
  mpz_sub(tz, a.get_data(), tz);

  data = dd_real(hi, mpz_get_d(tz));
  mpz_clear(tz);
}

#endif

/* set_z (to qd_real) */
#ifdef FPLLL_WITH_QD

/** set_z (from long to qd_real) */
template <> template <> inline void FP_NR<qd_real>::set_z(const Z_NR<long> &a, mp_rnd_t)
{
  data = a.get_data();
}

/** set_z (from default mpz_t to qd_real) */
template <> template <> inline void FP_NR<qd_real>::set_z(const Z_NR<mpz_t> &a, mp_rnd_t /*rnd*/)
{
  data = mpz_get_d(a.get_data());
}

#endif

/** set_z (from long to mpfr_t) */
template <> template <> inline void FP_NR<mpfr_t>::set_z(const Z_NR<long> &a, mp_rnd_t rnd)
{
  mpfr_set_si(data, a.get_data(), rnd);
}

/** set_z (from default mpz_t to mpfr_t) */
template <> template <> inline void FP_NR<mpfr_t>::set_z(const Z_NR<mpz_t> &a, mp_rnd_t rnd)
{
  mpfr_set_z(data, a.get_data(), rnd);
}

/* -----------------------------
 *   get_z_exp (FP_NR --> Z_NR)
 * ----------------------------- */

/** get_z_exp_we (from double to Z_NR<long>) */
template <>
template <>
inline void FP_NR<double>::get_z_exp_we(Z_NR<long> &a, long &expo, long expo_add) const
{
  expo = 0;
  a    = static_cast<long>(std::ldexp(data, static_cast<int>(expo_add)));
}

/** get_z_exp_we (from double to default mpz_t Z_NR<mpz_t>) */
template <>
template <>
inline void FP_NR<double>::get_z_exp_we(Z_NR<mpz_t> &a, long &expo, long expo_add) const
{
  expo = std::max(exponent() + expo_add - std::numeric_limits<double>::digits, 0L);
  mpz_set_d(a.get_data(), std::ldexp(data, static_cast<int>(expo_add - expo)));
}

/** get_z_exp_we (from double to Z_NR<class Z>) */
template <> template <class Z> inline void FP_NR<double>::get_z_exp(Z_NR<Z> &a, long &expo) const
{
  get_z_exp_we(a, expo, 0);
}

/* get_z_exp and get_z_exp_we (dd_real and qd_real --> Z_NR) */
#ifdef FPLLL_WITH_QD

/** get_z_exp_we (from dd_real to default mpz_t Z_NR<mpz_t>) */
template <>
template <>
inline void FP_NR<dd_real>::get_z_exp_we(Z_NR<mpz_t> &a, long &expo, long expo_add) const
{
  // double-double has almost the same exp as double
  expo = std::max(exponent() + expo_add - std::numeric_limits<double>::digits, 0L);
  // losing precision here
  mpz_set_d(a.get_data(), ::to_double(::ldexp(data, expo_add - expo)));
}

/** get_z_exp_we (from qd_real to default mpz_t Z_NR<mpz_t>) */
template <>
template <>
inline void FP_NR<qd_real>::get_z_exp_we(Z_NR<mpz_t> &a, long &expo, long expo_add) const
{
  expo = std::max(exponent() + expo_add - std::numeric_limits<double>::digits, 0L);
  mpz_set_d(a.get_data(), ::to_double(::ldexp(data, expo_add - expo)));
}

/** get_z_exp_we (from dd_real to Z_NR<long>) */
template <>
template <>
inline void FP_NR<dd_real>::get_z_exp_we(Z_NR<long> &a, long &expo, long expo_add) const
{
  expo = 0;
  a    = ::to_int(::ldexp(data, expo_add));
}

/** get_z_exp_we (from qd_real to Z_NR<long>) */
template <>
template <>
inline void FP_NR<qd_real>::get_z_exp_we(Z_NR<long> &a, long &expo, long expo_add) const
{
  expo = 0;
  a    = ::to_int(::ldexp(data, expo_add));
}

/** get_z_exp (from dd_real to Z_NR<class Z>) */
template <> template <class Z> inline void FP_NR<dd_real>::get_z_exp(Z_NR<Z> &a, long &expo) const
{
  get_z_exp_we(a, expo, 0);
}

/** get_z_exp (from qd_real to Z_NR<class Z>) */
template <> template <class Z> inline void FP_NR<qd_real>::get_z_exp(Z_NR<Z> &a, long &expo) const
{
  get_z_exp_we(a, expo, 0);
}

#endif

/* get_z_exp and get_z_exp_we (mpfr_t --> Z_NR) */

/** get_z_exp (from mpfr_t to Z_NR<long>) */
template <> template <> inline void FP_NR<mpfr_t>::get_z_exp(Z_NR<long> &a, long &expo) const
{
  expo = 0;
  a    = get_si();
}

/** get_z_exp (from mpfr_t to default mpz_t Z_NR<mpz_t>) */
template <> template <> inline void FP_NR<mpfr_t>::get_z_exp(Z_NR<mpz_t> &a, long &expo) const
{
  expo = mpfr_get_z_exp(a.get_data(), data);
  if (expo < 0)
  {
    a.mul_2si(a, expo);
    expo = 0;
  }
}

/** get_z_exp_we (from mpfr_t to Z_NR<class Z>) */
template <>
template <class Z>
inline void FP_NR<mpfr_t>::get_z_exp_we(Z_NR<Z> &a, long &expo, long /*expo_add*/) const
{
  get_z_exp(a, expo);
}

}  // namespace fplll

#endif
