/* Copyright (C) 2016 Martin R. Albrecht

   This file is part of fplll. fplll is free software: you
   can redistribute it and/or modify it under the terms of the GNU Lesser
   General Public License as published by the Free Software Foundation,
   either version 2.1 of the License, or (at your option) any later version.

   fplll is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with fplll. If not, see <http://www.gnu.org/licenses/>. */

#include <cstring>
#include <fplll/detail/precision.h>
#include <fplll/fplll.h>
#include <stdexcept>

using namespace std;
using namespace fplll;

template <class FT> const char *test_backend(const FP_NR<FT> &) { return num_type_str<FT>(); }

template <class FT> int check_close(const FT &actual, double expected, const char *test_case)
{
  const int status = !(abs(actual - expected) < 0.001);
  if (status)
  {
    cerr << "test_nr failed: backend=" << test_backend(actual) << ", case=" << test_case
         << ", expected=" << expected << " (absolute tolerance=0.001), actual=" << actual << '\n';
  }
  return status;
}

template <class FT> int test_str()
{
  int status = 0;
  FT a       = "1.01";
  status |= check_close(a, 1.01, "string-conversion");
  FT b = "-1.25e2";
  status |= check_close(b, -125.0, "negative-scientific-string-conversion");
  FT c = "0.125";
  status |= check_close(c, 0.125, "fraction-string-conversion");
  return status;
}

template <class FT> int test_arithmetic()
{
  FT a = 6.0, b = 3.0, c = -2;
  FT d       = 1.0 - (a * b + 2 * c + 2.0) / c;
  int status = 0;
  status |= d.cmp(9);
  if (d.cmp(9))
  {
    cerr << "test_nr failed: backend=" << test_backend(d)
         << ", case=arithmetic, expected=9, actual=" << d << '\n';
  }
  d += 1.0;
  status |= d.cmp(10);
  if (d.cmp(10))
  {
    cerr << "test_nr failed: backend=" << test_backend(d)
         << ", case=increment, expected=10, actual=" << d << '\n';
  }
  return status;
}

template <class FT> bool test_hypot()
{
  FT a, b, c, d;
  a = 3.0;
  b = 4.0;
  // the hypot function is asymmetric,
  // so we test both hypot(a,b) and hypot(b,a)
  // a^2 + b^2 = c^2 = d^2 = 5^2
  c.hypot(a, b);
  d.hypot(b, a);
  int status = (!(abs(c - 5.0) < 0.001) || !(abs(d - 5.0) < 0.001));

  if (status == 1)
  {
    cerr << "test_nr failed: backend=" << test_backend(c)
         << ", case=hypot(3,4)/hypot(4,3), expected=5 (absolute tolerance=0.001), actual=" << c
         << "/" << d << '\n';
  }
  return status;
}

/**
   @brief

   zero on success.

   @return
*/

template <class FT> int test_std()
{
  FT a;
  a          = 6.1;
  int status = 0;
  status |= check_close(sqrt(a), std::sqrt(a.get_d()), "sqrt(6.1)");
  status |= check_close(log(a), std::log(a.get_d()), "log(6.1)");
  status |= check_close(exp(a), std::exp(a.get_d()), "exp(6.1)");
  status |= check_close(floor(a), std::floor(a.get_d()), "floor(6.1)");
  status |= check_close(pow_si(a, 2), std::pow(a.get_d(), 2.0), "pow_si(6.1,2)");
  return status;
}

template <class FT> int test_root()
{
  FT a;
  a          = 6.1;
  int status = check_close(root(a, 3.0), std::pow(a.get_d(), 1 / 3.0), "root(6.1,3)");
  return status;
}

int test_double_range_parsing()
{
  FP_NR<double> positive = "1e309", negative = "-1e309", tiny = "-1e-400", invalid = "1e309junk";
  if (!std::isinf(positive.get_d()) || positive.get_d() < 0 || !std::isinf(negative.get_d()) ||
      negative.get_d() > 0 || tiny.get_d() != 0.0 || !std::signbit(tiny.get_d()) ||
      invalid.get_d() != 0.0)
  {
    cerr << "test_nr failed: double range parsing\n";
    return 1;
  }
  return 0;
}

int test_integer_parsing()
{
  Z_NR<long> value;
  if (!value.set_str("123"))
    return 1;
  if (value.get_si() != 123)
    return 1;
  if (value.set_str("12x"))
    return 1;
  if (value.set_str(""))
    return 1;
  return 0;
}

int test_scoped_precision()
{
  const unsigned int initial = FP_NR<mpfr_t>::get_prec();
  {
    ScopedPrecision guard(128);
    if (FP_NR<mpfr_t>::get_prec() != 128)
    {
      cerr << "ScopedPrecision did not apply the requested precision" << '\n';
      return 1;
    }
  }
  if (FP_NR<mpfr_t>::get_prec() != initial)
  {
    cerr << "ScopedPrecision did not restore the original precision" << '\n';
    return 1;
  }

  try
  {
    ScopedPrecision guard(64);
    throw std::runtime_error("force unwind");
  }
  catch (const std::runtime_error &)
  {
  }

  if (FP_NR<mpfr_t>::get_prec() != initial)
  {
    cerr << "ScopedPrecision did not restore precision during unwinding" << '\n';
    return 1;
  }
  return 0;
}

int main()
{

  int status = 0;
  status |= test_integer_parsing();
  status |= test_double_range_parsing();
  status |= test_scoped_precision();
  status |= test_arithmetic<FP_NR<double>>();
#ifdef FPLLL_WITH_LONG_DOUBLE
  status |= test_arithmetic<FP_NR<long double>>();
#endif
#ifdef FPLLL_WITH_QD
  status |= test_arithmetic<FP_NR<dd_real>>();
  status |= test_arithmetic<FP_NR<qd_real>>();
#endif
  status |= test_arithmetic<FP_NR<mpfr_t>>();

  status |= test_std<FP_NR<double>>();
#ifdef FPLLL_WITH_LONG_DOUBLE
  status |= test_std<FP_NR<long double>>();
#endif
#ifdef FPLLL_WITH_QD
  status |= test_std<FP_NR<dd_real>>();
  status |= test_std<FP_NR<qd_real>>();
#endif
  status |= test_std<FP_NR<mpfr_t>>();

  status |= test_root<FP_NR<double>>();
#ifdef FPLLL_WITH_LONG_DOUBLE
  status |= test_root<FP_NR<long double>>();
#endif
#ifdef FPLLL_WITH_QD
  status |= test_root<FP_NR<dd_real>>();
  status |= test_root<FP_NR<qd_real>>();
#endif
  status |= test_root<FP_NR<mpfr_t>>();

  status |= test_str<FP_NR<double>>();
#ifdef FPLLL_WITH_LONG_DOUBLE
  status |= test_str<FP_NR<long double>>();
#endif
#ifdef FPLLL_WITH_QD
  status |= test_str<FP_NR<dd_real>>();
  status |= test_str<FP_NR<qd_real>>();
#endif
  status |= test_str<FP_NR<mpfr_t>>();

  status |= test_hypot<FP_NR<double>>();
#ifdef FPLLL_WITH_LONG_DOUBLE
  status |= test_hypot<FP_NR<long double>>();
#endif

#ifdef FPLLL_WITH_QD
  status |= test_hypot<FP_NR<dd_real>>();
  status |= test_hypot<FP_NR<qd_real>>();
#endif
  status |= test_hypot<FP_NR<mpfr_t>>();

  if (status == 0)
  {
    cerr << "All tests passed." << '\n';
    return 0;
  }
  else
  {
    return -1;
  }

  return 0;
}
