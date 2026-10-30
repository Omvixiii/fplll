// SPDX-FileCopyrightText: 2015 Martin Albrecht
// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <fplll/fplll.h>  // IWYU pragma: keep
#include <fplll/gso/gso.h>
#include <fplll/nr/matrix.h>
#include <fplll/nr/nr.h>

#include "../../src/reduction/lll/lll_kernel.h"
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

   @param A                matrix which is expected not to be LLL-reduced yet.

   The test fails when is_lll_reduced reports success on an unreduced basis.
*/

void require_not_lll_reduced(fplll::ZZ_mat<mpz_t> &A)
{
  fplll::ZZ_mat<mpz_t> U;
  fplll::ZZ_mat<mpz_t> UT;

  fplll::MatGSO<ZT, FT> M(A, U, UT);

  INFO("is_lll_reduced must not report the unreduced basis as reduced");
  REQUIRE_FALSE(fplll::is_lll_reduced<ZT, FT>(M, fplll::LLLParameters{}));
}

/**
   @brief Test LLL reduction.

   @param A                test matrix
   @param options          reduction options
   @param prec             precision used for is_lll_reduced
*/

void test_lll(fplll::ZZ_mat<mpz_t> &A, const fplll::LLLOptions &options, int prec = 0)
{
  INFO("method: " << fplll::to_string(options.method)
                  << ", float type: " << fplll::to_string(options.float_type));
  require_not_lll_reduced(A);

  const fplll::ReductionReport report = fplll::lll_reduction(A, options);
  if (!fplll::succeeded(report))
  {
    UNSCOPED_INFO("LLL reduction failed with error '" << fplll::to_string(report) << "'");
  }
  REQUIRE(fplll::succeeded(report));

  const unsigned int old_prec =
      prec ? fplll::FP_NR<mpfr_t>::set_prec(static_cast<unsigned int>(prec)) : 0;

  fplll::ZZ_mat<mpz_t> U;
  fplll::ZZ_mat<mpz_t> UT;

  fplll::MatGSO<ZT, FT> M(A, U, UT);

  const bool is_reduced = fplll::is_lll_reduced<ZT, FT>(M, fplll::LLLParameters{});

  if (prec)
  {
    fplll::FP_NR<mpfr_t>::set_prec(old_prec);
  }

  INFO("the output of the LLL reduction must be LLL-reduced");
  CHECK(is_reduced);
}

/**
   @brief Test LLL for matrix stored in file pointed to by `input_filename`.

   @param input_filename   a path
   @param options          reduction options
   @param prec             precision used for is_lll_reduced
*/

void test_filename(const char *input_filename, const fplll::LLLOptions &options, int prec = 0)
{
  fplll::ZZ_mat<mpz_t> A;
  INFO("input file: " << input_filename);
  REQUIRE(read_file(A, input_filename) == 0);
  test_lll(A, options, prec);
}

/**
   @brief Construct d x (d+1) integer relations matrix with bit size b and test LLL.

   @param d                dimension
   @param b                bit size
   @param options          reduction options
*/

void test_int_rel(int d, int b, const fplll::LLLOptions &options)
{
  fplll::ZZ_mat<mpz_t> A;
  A.resize(d, d + 1);
  A.gen_intrel(b);
  test_lll(A, options);
}

}  // namespace

TEST_CASE("LLL with the wrapper method reduces the dim55 basis at 128-bit precision", "[lll]")
{
  test_filename(TESTDATADIR "/tests/lattices/dim55_in", fplll::LLLOptions{}, 128);
}

TEST_CASE("LLL with the proved mpfr method reduces the dim55 basis", "[lll]")
{
  test_filename(
      TESTDATADIR "/tests/lattices/dim55_in",
      fplll::LLLOptions{.method = fplll::LLLMethod::proved, .float_type = fplll::FloatType::mpfr});
}

// The integer-relation bases are drawn from the shared GMP random state, so
// these reductions stay in a single test case in their original order. That
// keeps the generated bases identical to the ones the historical main() used
// (Catch2 randomises the order in which test cases run).
TEST_CASE("LLL reduces integer-relation bases with the fast, proved and mpfr methods", "[lll]")
{
  test_int_rel(50, 1000,
               fplll::LLLOptions{.method     = fplll::LLLMethod::fast,
                                 .float_type = fplll::FloatType::double_precision});
  test_int_rel(
      50, 1000,
      fplll::LLLOptions{.method = fplll::LLLMethod::proved, .float_type = fplll::FloatType::mpfr});
  test_int_rel(
      30, 2000,
      fplll::LLLOptions{.method = fplll::LLLMethod::proved, .float_type = fplll::FloatType::mpfr});
}

TEST_CASE("LLL with the heuristic method reduces the example basis", "[lll]")
{
  test_filename(TESTDATADIR "/tests/lattices/example_in",
                fplll::LLLOptions{.method = fplll::LLLMethod::heuristic});
}

TEST_CASE("LLL with the fast double method reduces the example basis", "[lll]")
{
  test_filename(TESTDATADIR "/tests/lattices/example_in",
                fplll::LLLOptions{.method     = fplll::LLLMethod::fast,
                                  .float_type = fplll::FloatType::double_precision});
}

TEST_CASE("LLL with the proved mpfr method reduces the example basis", "[lll]")
{
  test_filename(
      TESTDATADIR "/tests/lattices/example_in",
      fplll::LLLOptions{.method = fplll::LLLMethod::proved, .float_type = fplll::FloatType::mpfr});
}

TEST_CASE("LLL with the proved double method reduces a small basis", "[lll]")
{
  // Small entries keep the proved method within double precision.  This also
  // covers the low-level GSO option combination: the proved method uses the
  // exact integer Gram matrix and must not enable row-exponent scaling.
  fplll::ZZ_mat<mpz_t> A(3, 3);
  A(0, 0) = 4;
  A(0, 1) = 0;
  A(0, 2) = 0;
  A(1, 0) = 2;
  A(1, 1) = 5;
  A(1, 2) = 1;
  A(2, 0) = 1;
  A(2, 1) = 3;
  A(2, 2) = 6;
  test_lll(A, fplll::LLLOptions{.method     = fplll::LLLMethod::proved,
                                .float_type = fplll::FloatType::double_precision});
}

TEST_CASE("LLL with early reduction reduces the example basis with the fast double method", "[lll]")
{
  test_filename(TESTDATADIR "/tests/lattices/example_in",
                fplll::LLLOptions{.parameters = {.early_reduction = true},
                                  .method     = fplll::LLLMethod::fast,
                                  .float_type = fplll::FloatType::double_precision});
}

TEST_CASE("LLL with early reduction reduces the example basis with the heuristic method", "[lll]")
{
  test_filename(TESTDATADIR "/tests/lattices/example_in",
                fplll::LLLOptions{.parameters = {.early_reduction = true},
                                  .method     = fplll::LLLMethod::heuristic});
}

TEST_CASE("LLL proved with an explicit MPFR backend and no precision uses the proved precision",
          "[lll]")
{
  // LLL keeps its documented semantics: precision 0 with an explicit MPFR
  // backend means the precision the algorithm selected (the proved precision
  // for the proved method), not the process default MPFR precision.
  fplll::ZZ_mat<mpz_t> A(3, 3);
  A(0, 0) = 4;
  A(0, 1) = 0;
  A(0, 2) = 0;
  A(1, 0) = 2;
  A(1, 1) = 5;
  A(1, 2) = 1;
  A(2, 0) = 1;
  A(2, 1) = 3;
  A(2, 2) = 6;

  const fplll::ReductionReport report =
      fplll::lll_reduction(A, fplll::LLLOptions{.method     = fplll::LLLMethod::proved,
                                                .float_type = fplll::FloatType::mpfr});
  const int good_prec = fplll::l2_min_prec(A.get_rows(), fplll::LLL_DEF_DELTA, fplll::LLL_DEF_ETA,
                                           fplll::LLL_DEF_EPSILON);

  CHECK(fplll::succeeded(report));
  CHECK(report.float_type == fplll::FloatType::mpfr);
  CHECK(report.precision == good_prec);
  CHECK(report.guaranteed);
}

TEST_CASE("Wrapper skips the backend bookkeeping for an empty basis", "[lll]")
{
  // E-5/P2-3: the constructor used to compute log2(d * n) and l2_min_prec()
  // for a 0x0 basis before lll() could return early.
  fplll::ZZ_mat<mpz_t> basis;
  fplll::ZZ_mat<mpz_t> u;
  fplll::ZZ_mat<mpz_t> u_inv;
  fplll::Wrapper wrapper(basis, u, u_inv, fplll::LLLParameters{});
  CHECK(wrapper.lll());
  CHECK(wrapper.report.status == fplll::RedStatus::success);
  CHECK(wrapper.report.float_type == fplll::FloatType::automatic);
  CHECK(wrapper.report.precision == 0);
  CHECK(wrapper.report.guaranteed);
}
