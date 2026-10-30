// SPDX-FileCopyrightText: 2016 Martin Albrecht
// SPDX-FileCopyrightText: 2019 Koen de Boer & Wessel van Woerden
// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test_utils.h"
#include <fplll/defs.h>
#include <fplll/gso/gso.h>
#include <fplll/gso/gso_gram.h>
#include <fplll/gso/gso_interface.h>
#include <fplll/nr/matrix.h>
#include <fplll/nr/nr.h>
#include <fplll/reduction/bkz_param.h>
#include <fplll/reduction/lll.h>
#include <fplll/util.h>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

// BKZReduction is an implementation class and is not part of the installed
// headers.  This test cross-checks the basis and Gram representations through
// it, so it includes the internal header directly.
#include "../../src/reduction/bkz/bkz_kernel.h"
#include "../../src/reduction/lll/lll_kernel.h"

#ifndef TESTDATADIR
#define TESTDATADIR ".."
#endif

namespace
{

using ZT = fplll::Z_NR<mpz_t>;

// Recomputes the Gram matrix from the basis and compares it with `G`.
// Returns true if the two representations disagree.
bool gram_mismatch(const fplll::ZZ_mat<mpz_t> &b, const fplll::ZZ_mat<mpz_t> &G)
{
  const int r = b.r;
  const int c = b.c;
  fplll::ZZ_mat<mpz_t> G_reduced;
  G_reduced.resize(r, r);
  for (int i = 0; i < r; i++)
  {
    for (int j = 0; j < r; j++)
    {
      b[i].dot_product(G_reduced(i, j), b[j], c);
    }
  }

  // Test whether G_reduced = G
  for (int i = 0; i < r; i++)
  {
    for (int j = 0; j < i; j++)
    {
      if (G(i, j) != G_reduced(i, j))
      {
        return true;
      }
    }
  }
  return false;
}

// Restores the global MPFR precision when it goes out of scope.
struct mpfr_precision_guard
{
  explicit mpfr_precision_guard(unsigned int prec) : old_prec(fplll::FP_NR<mpfr_t>::set_prec(prec))
  {
  }
  ~mpfr_precision_guard() { fplll::FP_NR<mpfr_t>::set_prec(old_prec); }
  unsigned int old_prec;
};

void check_gram_matches(const fplll::ZZ_mat<mpz_t> &A, const fplll::ZZ_mat<mpz_t> &G)
{
  INFO("the gram-representation and the basis-representation of the same lattice have an "
       "unequal gram matrix");
  CHECK_FALSE(gram_mismatch(A, G));
}

/**
   @brief Test BKZ on the basis and on its Gram matrix.

   @param A          test matrix; LLL-reduced in place and then BKZ-reduced
   @param param      block-reduction parameters
   @param preprocess_gram  whether the Gram matrix is built from the LLL-reduced basis
*/
template <class FT>
void test_bkz_impl(fplll::ZZ_mat<mpz_t> &A, const fplll::BKZParam &param, int precision,
                   bool exact_gram_gso)
{
  fplll::ZZ_mat<mpz_t> U;
  fplll::ZZ_mat<mpz_t> UT;

  // lllwrapper (no FloatType needed): BKZ always starts with an LLL-reduced basis.
  if (param.no_lll)
  {
    fplll::zeros_last(A, U, UT);
  }
  else
  {
    const fplll::ReductionReport lll_report = fplll::lll_reduction(A, fplll::LLLOptions{});
    INFO("LLL preprocessing failed with error '" << fplll::to_string(lll_report) << "'");
    REQUIRE(fplll::succeeded(lll_report));
  }

  // Create the Gram matrix G of the basis A.
  fplll::ZZ_mat<mpz_t> G;
  const int r = A.r;
  const int c = A.c;
  G.resize(r, r);
  for (int i = 0; i < r; i++)
  {
    for (int j = 0; j < r; j++)
    {
      A[i].dot_product(G(i, j), A[j], c);
    }
  }

  const fplll::GSOOptions gso_options{.int_gram = exact_gram_gso};

  if (precision > 0)
  {
    const mpfr_precision_guard precision_guard(static_cast<unsigned int>(precision));
    fplll::MatGSO<ZT, fplll::FP_NR<FT>> M(A, U, UT, gso_options);
    M.update_gso();
    fplll::MatGSOGram<ZT, fplll::FP_NR<FT>> Mgram(G, U, UT);
    Mgram.update_gso();

    fplll::LLLReduction<ZT, fplll::FP_NR<FT>> LLLObj(M, fplll::LLLParameters{});
    fplll::BKZReduction<ZT, fplll::FP_NR<FT>> BKZObj(M, LLLObj, param);
    BKZObj.bkz();

    fplll::LLLReduction<ZT, fplll::FP_NR<FT>> LLLObjgram(Mgram, fplll::LLLParameters{});
    fplll::BKZReduction<ZT, fplll::FP_NR<FT>> BKZObjgram(Mgram, LLLObjgram, param);
    BKZObjgram.bkz();
  }
  else
  {
    fplll::MatGSO<ZT, fplll::FP_NR<FT>> M(A, U, UT, gso_options);
    M.update_gso();
    fplll::MatGSOGram<ZT, fplll::FP_NR<FT>> Mgram(G, U, UT);
    Mgram.update_gso();

    fplll::LLLReduction<ZT, fplll::FP_NR<FT>> LLLObj(M, fplll::LLLParameters{});
    fplll::BKZReduction<ZT, fplll::FP_NR<FT>> BKZObj(M, LLLObj, param);
    BKZObj.bkz();

    fplll::LLLReduction<ZT, fplll::FP_NR<FT>> LLLObjgram(Mgram, fplll::LLLParameters{});
    fplll::BKZReduction<ZT, fplll::FP_NR<FT>> BKZObjgram(Mgram, LLLObjgram, param);
    BKZObjgram.bkz();
  }

  check_gram_matches(A, G);
}

/**
   @brief Test BKZ reduction.

   @param A                test matrix
   @param param            block-reduction parameters
   @param float_type       floating point type to test
   @param precision        precision if mpfr is used
*/
void test_bkz(fplll::ZZ_mat<mpz_t> &A, const fplll::BKZParam &param,
              fplll::FloatType float_type = fplll::FloatType::automatic, int precision = 0)
{
  const fplll::FloatType sel_ft =
      (float_type != fplll::FloatType::automatic) ? float_type : fplll::FloatType::double_precision;

  switch (sel_ft)
  {
  case fplll::FloatType::double_precision:
    test_bkz_impl<double>(A, param, 0, false);
    break;
  case fplll::FloatType::mpfr:
    test_bkz_impl<mpfr_t>(A, param, precision, true);
    break;
#ifdef FPLLL_WITH_QD
  case fplll::FloatType::dd:
    test_bkz_impl<dd_real>(A, param, 0, false);
    break;
  case fplll::FloatType::qd:
    test_bkz_impl<qd_real>(A, param, 0, false);
    break;
#endif
  default:
    // The original harness returned success here without running the reduction.
    WARN("float type " << fplll::to_string(sel_ft) << " is not supported by this test");
    break;
  }
}

fplll::BKZParam make_bkz_param(int block_size, fplll::BKZVariant variant = fplll::BKZVariant::bkz,
                               bool auto_abort             = false,
                               fplll::FloatType float_type = fplll::FloatType::automatic,
                               int precision               = 0)
{
  fplll::BKZParam param;
  param.block_size = block_size;
  param.variant    = variant;
  param.auto_abort = auto_abort;
  param.float_type = float_type;
  param.precision  = precision;
  return param;
}

/**
   @brief Test BKZ for matrix stored in file pointed to by `input_filename`.

   @param input_filename   a path
   @param param            block-reduction parameters
   @param float_type       floating point type to test
   @param precision        precision if mpfr is used
*/
void test_filename(const char *input_filename, const fplll::BKZParam &param,
                   fplll::FloatType float_type = fplll::FloatType::automatic, int precision = 0)
{
  INFO("input file: " << input_filename);
  fplll::ZZ_mat<mpz_t> A;
  REQUIRE(read_file(A, input_filename) == 0);
  test_bkz(A, param, float_type, precision);
}

/**
   @brief Construct d x (d+1) integer relations matrix with bit size b and test BKZ.

   @param d                dimension
   @param b                bit size
   @param param            block-reduction parameters
   @param float_type       floating point type to test
   @param precision        precision if mpfr is used
*/
void test_int_rel(int d, int b, const fplll::BKZParam &param,
                  fplll::FloatType float_type = fplll::FloatType::automatic, int precision = 0)
{
  INFO("integer relation matrix: dimension " << d << ", bit size " << b);
  fplll::ZZ_mat<mpz_t> A;
  A.resize(d, d + 1);
  A.gen_intrel(b);

  fplll::BKZParam verbose_param = param;
  verbose_param.verbose         = true;
  test_bkz(A, verbose_param, float_type, precision);
}

}  // namespace

TEST_CASE("BKZ matches the Gram matrix for dim55_in with double arithmetic", "[bkz]")
{
  test_filename(TESTDATADIR "/tests/lattices/dim55_in",
                make_bkz_param(10, fplll::BKZVariant::bkz, true));
}

#ifdef FPLLL_WITH_QD
TEST_CASE("BKZ matches the Gram matrix for dim55_in with dd arithmetic", "[bkz]")
{
  test_filename(TESTDATADIR "/tests/lattices/dim55_in",
                make_bkz_param(10, fplll::BKZVariant::sd_bkz, true), fplll::FloatType::dd);
}
#endif

TEST_CASE("BKZ matches the Gram matrix for dim55_in with SLD reduction", "[bkz]")
{
  test_filename(TESTDATADIR "/tests/lattices/dim55_in",
                make_bkz_param(10, fplll::BKZVariant::slide));
}

TEST_CASE("BKZ matches the Gram matrix for dim55_in with MPFR at block size 20", "[bkz]")
{
  test_filename(TESTDATADIR "/tests/lattices/dim55_in",
                make_bkz_param(20, fplll::BKZVariant::bkz, true), fplll::FloatType::mpfr, 128);
}

TEST_CASE("BKZ matches the Gram matrix for dim55_in with MPFR and the SD variant", "[bkz]")
{
  test_filename(TESTDATADIR "/tests/lattices/dim55_in",
                make_bkz_param(20, fplll::BKZVariant::sd_bkz, true), fplll::FloatType::mpfr, 128);
}

TEST_CASE("BKZ matches the Gram matrix for dim55_in with MPFR and SLD reduction", "[bkz]")
{
  test_filename(TESTDATADIR "/tests/lattices/dim55_in",
                make_bkz_param(20, fplll::BKZVariant::slide), fplll::FloatType::mpfr, 128);
}

// The generated integer relation matrices come from the global GMP random
// stream, so these runs stay in one test case in their original order.
TEST_CASE("BKZ matches the Gram matrix for integer relation lattices", "[bkz]")
{
  // 50 x 51 integer relation matrices with 1000-bit entries.
  test_int_rel(50, 1000, make_bkz_param(10, fplll::BKZVariant::bkz, true));
  test_int_rel(50, 1000, make_bkz_param(10, fplll::BKZVariant::sd_bkz, true));
  test_int_rel(50, 1000, make_bkz_param(10, fplll::BKZVariant::slide));
  test_int_rel(50, 1000, make_bkz_param(15, fplll::BKZVariant::bkz, true), fplll::FloatType::mpfr,
               100);
  test_int_rel(50, 1000, make_bkz_param(15, fplll::BKZVariant::sd_bkz, true),
               fplll::FloatType::mpfr, 100);
  test_int_rel(50, 1000, make_bkz_param(15, fplll::BKZVariant::slide), fplll::FloatType::mpfr, 100);
  // 30 x 31 integer relation matrices with 2000-bit entries.
  test_int_rel(30, 2000, make_bkz_param(10, fplll::BKZVariant::bkz, true), fplll::FloatType::mpfr,
               53);
  test_int_rel(30, 2000, make_bkz_param(10, fplll::BKZVariant::sd_bkz, true),
               fplll::FloatType::mpfr, 53);
  test_int_rel(30, 2000, make_bkz_param(10, fplll::BKZVariant::slide), fplll::FloatType::mpfr, 53);
}

TEST_CASE("BKZ matches the Gram matrix for example_in", "[bkz]")
{
  test_filename(TESTDATADIR "/tests/lattices/example_in", make_bkz_param(10));
}

TEST_CASE("BKZ matches the Gram matrix for example_in with the SD variant", "[bkz]")
{
  test_filename(TESTDATADIR "/tests/lattices/example_in",
                make_bkz_param(10, fplll::BKZVariant::sd_bkz));
}

TEST_CASE("BKZ matches the Gram matrix for example_in with SLD reduction", "[bkz]")
{
  test_filename(TESTDATADIR "/tests/lattices/example_in",
                make_bkz_param(10, fplll::BKZVariant::slide));
}

TEST_CASE("BKZ matches the Gram matrix for example_in with double arithmetic", "[bkz]")
{
  test_filename(TESTDATADIR "/tests/lattices/example_in", make_bkz_param(10),
                fplll::FloatType::double_precision);
}

TEST_CASE("BKZ matches the Gram matrix for example_in with double arithmetic and the SD variant",
          "[bkz]")
{
  test_filename(TESTDATADIR "/tests/lattices/example_in",
                make_bkz_param(10, fplll::BKZVariant::sd_bkz), fplll::FloatType::double_precision);
}

TEST_CASE("BKZ matches the Gram matrix for example_in with double arithmetic and SLD reduction",
          "[bkz]")
{
  test_filename(TESTDATADIR "/tests/lattices/example_in",
                make_bkz_param(10, fplll::BKZVariant::slide), fplll::FloatType::double_precision);
}

TEST_CASE("BKZ matches the Gram matrix for example_in with MPFR arithmetic", "[bkz]")
{
  test_filename(TESTDATADIR "/tests/lattices/example_in",
                make_bkz_param(10, fplll::BKZVariant::bkz, true), fplll::FloatType::mpfr, 212);
}

TEST_CASE("BKZ matches the Gram matrix for example_in with MPFR arithmetic and the SD variant",
          "[bkz]")
{
  test_filename(TESTDATADIR "/tests/lattices/example_in",
                make_bkz_param(10, fplll::BKZVariant::sd_bkz, true), fplll::FloatType::mpfr, 212);
}

TEST_CASE("BKZ matches the Gram matrix for example_in with MPFR arithmetic and SLD reduction",
          "[bkz]")
{
  test_filename(TESTDATADIR "/tests/lattices/example_in",
                make_bkz_param(10, fplll::BKZVariant::slide, true), fplll::FloatType::mpfr, 212);
}

// The original harness ran the double-precision example_in cases a second time;
// those runs are kept for parity.
TEST_CASE("BKZ matches the Gram matrix for example_in with double arithmetic (second run)", "[bkz]")
{
  test_filename(TESTDATADIR "/tests/lattices/example_in", make_bkz_param(10),
                fplll::FloatType::double_precision);
}

TEST_CASE("BKZ matches the Gram matrix for example_in with double arithmetic and the SD variant "
          "(second run)",
          "[bkz]")
{
  test_filename(TESTDATADIR "/tests/lattices/example_in",
                make_bkz_param(10, fplll::BKZVariant::sd_bkz), fplll::FloatType::double_precision);
}

TEST_CASE("BKZ matches the Gram matrix for example_in with double arithmetic and SLD reduction "
          "(second run)",
          "[bkz]")
{
  test_filename(TESTDATADIR "/tests/lattices/example_in",
                make_bkz_param(10, fplll::BKZVariant::slide), fplll::FloatType::double_precision);
}
