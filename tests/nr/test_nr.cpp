// SPDX-FileCopyrightText: 2016 Martin R. Albrecht
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "../src/detail/float_type.h"
#include <fplll/fplll.h>  // IWYU pragma: keep
#include <fplll/nr/matrix.h>
#include <fplll/nr/nr.h>

#include <catch2/catch_message.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstring>
#include <gmp.h>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "test_utils.h"

namespace
{

template <class FT> const char *test_backend(const fplll::FP_NR<FT> &)
{
  return fplll::num_type_str<FT>();
}

template <class FT> void check_close(const FT &actual, double expected, const char *test_case)
{
  INFO("backend: " << test_backend(actual) << ", case: " << test_case << ", expected: " << expected
                   << " (absolute tolerance=0.001), actual=" << actual);
  CHECK(fplll::abs(actual - expected) < 0.001);
}

template <class FT> void test_str()
{
  FT a = "1.01";
  check_close(a, 1.01, "string-conversion");
  FT b = "-1.25e2";
  check_close(b, -125.0, "negative-scientific-string-conversion");
  FT c = "0.125";
  check_close(c, 0.125, "fraction-string-conversion");
}

template <class FT> void test_arithmetic()
{
  FT a = 6.0, b = 3.0, c = -2;
  FT d = 1.0 - (a * b + 2 * c + 2.0) / c;
  {
    INFO("backend: " << test_backend(d) << ", case=arithmetic, expected=9, actual=" << d);
    CHECK(d.cmp(9) == 0);
  }
  d += 1.0;
  {
    INFO("backend: " << test_backend(d) << ", case=increment, expected=10, actual=" << d);
    CHECK(d.cmp(10) == 0);
  }
}

template <class FT> void test_hypot()
{
  FT a, b, c, d;
  a = 3.0;
  b = 4.0;
  // the hypot function is asymmetric,
  // so we test both hypot(a,b) and hypot(b,a)
  // a^2 + b^2 = c^2 = d^2 = 5^2
  c.hypot(a, b);
  d.hypot(b, a);
  INFO("backend: " << test_backend(c)
                   << ", case=hypot(3,4)/hypot(4,3), expected=5 (absolute tolerance=0.001), actual="
                   << c << "/" << d);
  CHECK(fplll::abs(c - 5.0) < 0.001);
  CHECK(fplll::abs(d - 5.0) < 0.001);
}

template <class FT> void test_std()
{
  FT a;
  a = 6.1;
  check_close(fplll::sqrt(a), std::sqrt(a.get_d()), "sqrt(6.1)");
  check_close(fplll::log(a), std::log(a.get_d()), "log(6.1)");
  check_close(fplll::exp(a), std::exp(a.get_d()), "exp(6.1)");
  check_close(fplll::floor(a), std::floor(a.get_d()), "floor(6.1)");
  check_close(fplll::pow_si(a, 2), std::pow(a.get_d(), 2.0), "pow_si(6.1,2)");
}

template <class FT> void test_root()
{
  FT a;
  a = 6.1;
  check_close(fplll::root(a, 3.0), std::pow(a.get_d(), 1 / 3.0), "root(6.1,3)");
}

// NR-03: a constant Matrix exposes read-only rows, so writing through a
// constant reference no longer compiles; mutable rows keep the full MatrixRow
// interface, and the derived integer matrix follows the same rule.
using FPMatrix = fplll::Matrix<fplll::FP_NR<double>>;
using ZZMatrix = fplll::ZZ_mat<mpz_t>;

template <class M>
concept WritableThroughConstFP = requires(const M &m) { m[0][0] = 1.0; };
template <class M>
concept WritableThroughConstZZ = requires(const M &m) { m[0][0] = fplll::Z_NR<mpz_t>{}; };

static_assert(std::is_same_v<decltype(std::declval<FPMatrix &>()[0]),
                             fplll::MatrixRow<fplll::FP_NR<double>>>);
static_assert(std::is_same_v<decltype(std::declval<const FPMatrix &>()[0]),
                             fplll::ConstMatrixRow<fplll::FP_NR<double>>>);
static_assert(
    std::is_same_v<decltype(std::declval<const FPMatrix &>()[0][0]), const fplll::FP_NR<double> &>);
static_assert(
    std::is_same_v<decltype(std::declval<ZZMatrix &>()[0]), fplll::MatrixRow<fplll::Z_NR<mpz_t>>>);
static_assert(std::is_same_v<decltype(std::declval<const ZZMatrix &>()[0]),
                             fplll::ConstMatrixRow<fplll::Z_NR<mpz_t>>>);
static_assert(!WritableThroughConstFP<FPMatrix>);
static_assert(!WritableThroughConstZZ<ZZMatrix>);

}  // namespace

TEST_CASE("integer strings are parsed strictly", "[nr]")
{
  fplll::Z_NR<long> value;
  CHECK(value.set_str("123"));
  CHECK(value.get_si() == 123);
  CHECK_FALSE(value.set_str("12x"));
  CHECK_FALSE(value.set_str(""));
}

TEST_CASE("double parsing handles out-of-range values", "[nr]")
{
  fplll::FP_NR<double> positive = "1e309", negative = "-1e309", tiny = "-1e-400",
                       invalid = "1e309junk";
  CHECK(std::isinf(positive.get_d()));
  CHECK(positive.get_d() >= 0);
  CHECK(std::isinf(negative.get_d()));
  CHECK(negative.get_d() <= 0);
  CHECK(tiny.get_d() == 0.0);
  CHECK(std::signbit(tiny.get_d()));
  CHECK(invalid.get_d() == 0.0);
}

TEST_CASE("ScopedPrecision applies and restores the MPFR precision", "[nr]")
{
  const unsigned int initial = fplll::FP_NR<mpfr_t>::get_prec();
  {
    fplll::ScopedPrecision guard(128);
    INFO("ScopedPrecision did not apply the requested precision");
    CHECK(fplll::FP_NR<mpfr_t>::get_prec() == 128);
  }
  INFO("ScopedPrecision did not restore the original precision");
  CHECK(fplll::FP_NR<mpfr_t>::get_prec() == initial);

  try
  {
    fplll::ScopedPrecision guard(64);
    throw std::runtime_error("force unwind");
  }
  catch (const std::runtime_error &error)
  {
    // The exception is the sentinel thrown above to force unwinding; any other
    // exception is a genuine failure of this test.
    CHECK(std::strcmp(error.what(), "force unwind") == 0);
  }

  INFO("ScopedPrecision did not restore precision during unwinding");
  CHECK(fplll::FP_NR<mpfr_t>::get_prec() == initial);
}

TEMPLATE_TEST_CASE("FP_NR arithmetic evaluates mixed expressions", "[nr]", FPLLL_TEST_FP_BACKENDS)
{
  test_arithmetic<fplll::FP_NR<TestType>>();
}

TEMPLATE_TEST_CASE("FP_NR std functions match the C standard library", "[nr]",
                   FPLLL_TEST_FP_BACKENDS)
{
  test_std<fplll::FP_NR<TestType>>();
}

TEMPLATE_TEST_CASE("FP_NR root matches std::pow", "[nr]", FPLLL_TEST_FP_BACKENDS)
{
  test_root<fplll::FP_NR<TestType>>();
}

TEMPLATE_TEST_CASE("FP_NR decimal strings are parsed", "[nr]", FPLLL_TEST_FP_BACKENDS)
{
  test_str<fplll::FP_NR<TestType>>();
}

TEMPLATE_TEST_CASE("FP_NR hypot is symmetric", "[nr]", FPLLL_TEST_FP_BACKENDS)
{
  test_hypot<fplll::FP_NR<TestType>>();
}

TEST_CASE("const matrices expose read-only rows", "[nr]")
{
  FPMatrix matrix(2, 2);
  matrix[0].fill(1);
  matrix[1][0] = 2.0;
  matrix[1][1] = 3.0;

  const FPMatrix &cmatrix = matrix;
  CHECK(cmatrix[0][0].get_d() == 1.0);
  CHECK(cmatrix[1][0].get_d() == 2.0);
  CHECK(cmatrix[0].size_nz() == 2);
  CHECK_FALSE(cmatrix[0].is_zero());

  fplll::FP_NR<double> dot = 0.0;
  cmatrix[0].dot_product(dot, cmatrix[1], 2);
  CHECK(dot.get_d() == 5.0);

  // The mutable row view keeps the complete write interface.
  matrix[0].fill(2);
  matrix[0].dot_product(dot, matrix[1], 2);
  CHECK(dot.get_d() == 10.0);

  // The in-tree consumers of row views use the integer matrix.
  ZZMatrix ints(2, 2);
  ints[0][0]            = 1;
  ints[0][1]            = 2;
  ints[1][0]            = 3;
  ints[1][1]            = 4;
  const ZZMatrix &cints = ints;
  fplll::Z_NR<mpz_t> int_dot;
  cints[0].dot_product(int_dot, cints[1], 2);
  CHECK(int_dot.get_si() == 11);
  CHECK_FALSE(cints[0].is_zero());
}
