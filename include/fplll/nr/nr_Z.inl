// SPDX-FileCopyrightText: 2016-2019 Martin R. Albrecht
// SPDX-FileCopyrightText: 2016-2017 Shi Bai
// SPDX-FileCopyrightText: 2016 Damien Stehle
// SPDX-FileCopyrightText: 2017 Vaggelis1994
// SPDX-FileCopyrightText: 2020-2022 Joe Rowell
// SPDX-FileCopyrightText: 2024 Ludo Pulles
// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

/*********************
 *   Integer Class
 *********************/

#ifndef FPLLL_NR_Z_H
#define FPLLL_NR_Z_H

#include <iosfwd>

namespace fplll
{

/* declaration FP_NR */
template <class F> class FP_NR;

/**
 * Z_NR stores integers. This template provides a uniform interface
 *  for doing integer computations with several underlying types
 *  (long and mpz_t).
 */
template <class Z = mpz_t> class Z_NR
{

  Z data;

public:
  /**
   * Constructors.
   */
  inline Z_NR();
  inline Z_NR(const Z_NR<Z> &z);
  inline Z_NR(const Z &z);
  inline ~Z_NR() noexcept;

  /** get data */

  /**
   * Returns the internal representation of the data.
   */
  Z &get_data() { return data; }

  /**
   * Returns the internal representation of the data.
   */
  const Z &get_data() const { return data; }

  /**
   * Converts this object to a double. If it does not fit in
   *  a double, the result is undefined.
   */
  inline double get_d() const;

  /**
   * Converts this object to a long. If it does not fit in a long,
   * the result is undefined.
   */
  inline long get_si() const;

  /**
   * Converts this object to a mpz.
   */
  inline void get_mpz(mpz_t r) const;

  /**
   * Returns the smallest non-negative expo such that |value| < 2^expo.
   */
  inline long exponent() const;

  /**
   * Computes f and expo such that 0.5 <= f < 1 and value ~= 2^expo * f.
   *   The rounding direction is undefined.
   *   This function is implemented for double, mpfr_t and, when available,
   *   dd_real and qd_real; the behaviour is undefined otherwise.
   */
  template <class F> inline void get_f_exp(F &f, long &expo);

  /** set data */

  /**
   * Sets the value to x. When FT=mpfr_t, x is rounded to the nearest
   * integer and if the fractional part of x is 0.5, the even integer
   * is chosen when. Otherwise, the rounding direction is undefined.
   */
  template <class F> inline void set_f(const FP_NR<F> &f);

  /**
   * Sets the value to s, signed integer in basis 10.
   * Returns "false" if parsing failed, or overflow occured.
   */
  inline bool set_str(const char *s);

  /** comparison */

  /**
   * 3-way comparison. Returns a positive number if *this > m, a
   * negative number if *this &lt; m or zero is *this == m.
   */
  inline int cmp(const Z_NR<Z> &m) const;

  /**
   * Sign. Returns a positive number, a negative number or zero
   * if the value of this object is respectively positive,
   * negative or null.
   */
  inline int sgn() const;

  /**
   * Operator
   */
  inline void operator=(const Z_NR<Z> &z);
  inline void operator=(const mpz_t &z);
  inline void operator=(long i);
  inline bool operator<(const Z_NR<Z> &a) const;
  inline bool operator<(long a) const;
  inline bool operator>(const Z_NR<Z> &a) const;
  inline bool operator>(long a) const;
  inline bool operator<=(const Z_NR<Z> &a) const;
  inline bool operator<=(long a) const;
  inline bool operator>=(const Z_NR<Z> &a) const;
  inline bool operator>=(long a) const;
  inline bool operator==(const Z_NR<Z> &a) const;
  inline bool operator==(long a) const;
  inline bool operator!=(const Z_NR<Z> &a) const;
  inline bool operator!=(long a) const;

  /**
   * max between a and b
   */
  Z_NR &max_z(Z_NR<Z> &b)
  {
    if ((*this) <= b)
    {
      return b;
    }
    else
    {
      return (*this);
    }
  }

  /**
   * Returns non-zero if the current value is zero, 0 otherwise.
   */
  bool is_zero() const { return *this == 0; }

  /** arithmetic */

  /**
   * value := a + b.
   */
  inline void add(const Z_NR<Z> &a, const Z_NR<Z> &b);
  inline void add_ui(const Z_NR<Z> &a, unsigned int b);
  /**
   * value := a - b.
   */
  inline void sub(const Z_NR<Z> &a, const Z_NR<Z> &b);
  inline void sub_ui(const Z_NR<Z> &a, unsigned int b);

  /**
   * value := a mod b
   */
  inline void mod(const Z_NR<Z> &a, const Z_NR<Z> &b);

  /**
   * value := -a.
   */
  inline void neg(const Z_NR<Z> &a);

  /**
   * value := a * b.
   */
  inline void mul(const Z_NR<Z> &a, const Z_NR<Z> &b);

  /**
   * value := a * b.
   */
  inline void mul_si(const Z_NR<Z> &a, long b);
  inline void mul_ui(const Z_NR<Z> &a, unsigned long b);

  /**
   * value := a * 2^b.
   * if Z=long and |b| >= size_in_bits(a), the result is undefined.
   */
  inline void mul_2si(const Z_NR<Z> &a, long b);

  /**
   * value := a / 2^b.
   * if Z=long and |b| >= size_in_bits(a), the result is undefined.
   */
  inline void div_2si(const Z_NR<Z> &a, long b);

  /**
   * value := value + a * b.
   */
  inline void addmul(const Z_NR<Z> &a, const Z_NR<Z> &b);
  inline void addmul_ui(const Z_NR<Z> &a, unsigned long b);
  inline void addmul_si(const Z_NR<Z> &a, long b);

  /**
   * value := value - a * b.
   */
  inline void submul(const Z_NR<Z> &a, const Z_NR<Z> &b);
  inline void submul_ui(const Z_NR<Z> &a, unsigned long b);

  /**
   * value := absolute value of a.
   */
  inline void abs(const Z_NR<Z> &a);

  /**
   * Efficiently swaps the values of two Z_NR.
   */
  inline void swap(Z_NR<Z> &a) noexcept;

  /**
   * Generates random integer between 0 and 2^bits-1.
   */
  inline void randb(int bits);
  inline void randb_si(int bits);

  /**
   * Generates random integer between 0 and max - 1
   */
  inline void randm(const Z_NR<Z> &max);
  inline void randm_si(const Z_NR<Z> &max);

  /**
   * Generates smallest prime number above nbr
   */
  inline void nextprime(const Z_NR<Z> &nbr);
};

/** overloading stream operators */

/**
 * Prints x on stream os.
 */
template <class T> std::ostream &operator<<(std::ostream &os, const Z_NR<T> &x);

/**
 * Reads x from stream is.
 */
template <class T> std::istream &operator>>(std::istream &is, Z_NR<T> &x);

}  // namespace fplll

#endif
