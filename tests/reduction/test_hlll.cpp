// SPDX-FileCopyrightText: 2015 Martin Albrecht
// SPDX-FileCopyrightText: 2017-2018 Laurent Grémy
// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <fplll/fplll.h>  // IWYU pragma: keep
#include <fplll/gso/householder.h>
#include <fplll/nr/matrix.h>
#include <fplll/nr/nr.h>

#include "../../src/reduction/hlll/hlll_kernel.h"
#include "../../src/reduction/wrapper.h"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "test_utils.h"

#ifndef TESTDATADIR
#define TESTDATADIR ".."
#endif

namespace
{

using ZT = fplll::Z_NR<mpz_t>;
using FT = fplll::FP_NR<mpfr_t>;

/**
   @brief Test the tester.

   @param A                matrix which is expected not to be HLLL-reduced yet.

   The test fails when is_hlll_reduced reports success on an unreduced basis.
*/

void require_not_hlll_reduced(fplll::ZZ_mat<mpz_t> &A)
{
  fplll::ZZ_mat<mpz_t> U;
  fplll::ZZ_mat<mpz_t> UT;

  fplll::MatHouseholder<ZT, FT> M(A, U, UT);

  INFO("is_hlll_reduced must not report the unreduced basis as reduced");
  REQUIRE_FALSE(fplll::is_hlll_reduced<ZT, FT>(M, fplll::HLLLParameters{}));
}

/**
   @brief Test HLLL reduction.

   @param A                test matrix
   @param options          reduction options
*/

void test_hlll(fplll::ZZ_mat<mpz_t> &A, const fplll::HLLLOptions &options)
{
  INFO("method: " << fplll::to_string(options.method)
                  << ", float type: " << fplll::to_string(options.float_type));
  require_not_hlll_reduced(A);

  const fplll::ReductionReport report = fplll::hlll_reduction(A, options);
  if (!fplll::succeeded(report))
  {
    UNSCOPED_INFO("HLLL reduction failed with error '" << fplll::to_string(report) << "'");
  }
  REQUIRE(fplll::succeeded(report));

  const int prec =
      fplll::hlll_min_prec(A.get_rows(), A.get_cols(), fplll::LLL_DEF_DELTA, fplll::LLL_DEF_ETA,
                           fplll::HLLL_DEF_THETA, fplll::HLLL_DEF_C);

  const unsigned int old_prec = fplll::FP_NR<mpfr_t>::set_prec(static_cast<unsigned int>(prec));

  fplll::ZZ_mat<mpz_t> U;
  fplll::ZZ_mat<mpz_t> UT;

  fplll::MatHouseholder<ZT, FT> M(A, U, UT);

  const bool is_reduced = fplll::is_hlll_reduced<ZT, FT>(M, fplll::HLLLParameters{});

  fplll::FP_NR<mpfr_t>::set_prec(old_prec);

  INFO("the output of the HLLL reduction must be HLLL-reduced");
  CHECK(is_reduced);
}

/**
   @brief Test HLLL for matrix stored in file pointed to by `input_filename`.

   @param input_filename   a path
   @param options          reduction options
*/

void test_filename(const char *input_filename, const fplll::HLLLOptions &options)
{
  fplll::ZZ_mat<mpz_t> A;
  INFO("input file: " << input_filename);
  REQUIRE(read_file(A, input_filename) == 0);
  test_hlll(A, options);
}

/**
   @brief Construct d x (d+1) integer relations matrix with bit size b and test HLLL.

   @param d                dimension
   @param b                bit size
   @param options          reduction options
*/

void test_int_rel(int d, int b, const fplll::HLLLOptions &options)
{
  fplll::ZZ_mat<mpz_t> A;
  A.resize(d, d + 1);
  A.gen_intrel(b);
  test_hlll(A, options);
}

}  // namespace

TEST_CASE("HLLL with the wrapper method reduces the dim55 basis", "[hlll]")
{
  test_filename(TESTDATADIR "/tests/lattices/dim55_in", fplll::HLLLOptions{});
}

TEST_CASE("HLLL with the proved mpfr method reduces the dim55 basis", "[hlll]")
{
  test_filename(
      TESTDATADIR "/tests/lattices/dim55_in",
      fplll::HLLLOptions{.method = fplll::LLLMethod::proved, .float_type = fplll::FloatType::mpfr});
}

// The integer-relation bases are drawn from the shared GMP random state, so
// these reductions stay in one test case in their original order. That keeps
// the generated bases identical to the ones the historical main() used.
TEST_CASE("HLLL reduces integer-relation bases with the fast, proved and mpfr methods", "[hlll]")
{
  test_int_rel(50, 1000,
               fplll::HLLLOptions{.method     = fplll::LLLMethod::fast,
                                  .float_type = fplll::FloatType::double_precision});
  test_int_rel(
      30, 1000,
      fplll::HLLLOptions{.method = fplll::LLLMethod::proved, .float_type = fplll::FloatType::mpfr});
  test_int_rel(
      20, 2000,
      fplll::HLLLOptions{.method = fplll::LLLMethod::proved, .float_type = fplll::FloatType::mpfr});
}

TEST_CASE("HLLL with the fast double method reduces the example basis", "[hlll]")
{
  test_filename(TESTDATADIR "/tests/lattices/example_in",
                fplll::HLLLOptions{.method     = fplll::LLLMethod::fast,
                                   .float_type = fplll::FloatType::double_precision});
}

TEST_CASE("HLLL with the proved mpfr method reduces the example basis", "[hlll]")
{
  test_filename(
      TESTDATADIR "/tests/lattices/example_in",
      fplll::HLLLOptions{.method = fplll::LLLMethod::proved, .float_type = fplll::FloatType::mpfr});
}

TEST_CASE("Wrapper skips the backend bookkeeping for an empty HLLL basis", "[hlll]")
{
  // E-5/P2-3: the HLLL constructor used to compute hlll_min_prec(0, ...).
  fplll::ZZ_mat<mpz_t> basis;
  fplll::ZZ_mat<mpz_t> u;
  fplll::ZZ_mat<mpz_t> u_inv;
  fplll::Wrapper wrapper(basis, u, u_inv, fplll::HLLLParameters{});
  CHECK(wrapper.hlll());
  CHECK(wrapper.report.status == fplll::RedStatus::success);
  CHECK(wrapper.report.float_type == fplll::FloatType::automatic);
  CHECK(wrapper.report.precision == 0);
  CHECK(wrapper.report.guaranteed);
}
