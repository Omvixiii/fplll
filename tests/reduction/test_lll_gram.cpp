// SPDX-FileCopyrightText: 2015 Martin Albrecht
// SPDX-FileCopyrightText: 2019 Koen de Boer & Wessel van Woerden
// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <fplll/fplll.h>  // IWYU pragma: keep
#include <fplll/gso/gso.h>
#include <fplll/gso/gso_gram.h>
#include <fplll/gso/gso_interface.h>
#include <fplll/nr/matrix.h>
#include <fplll/nr/nr.h>
#include <fplll/reduction/lll.h>

#include "../../src/reduction/lll/lll_kernel.h"

#include <catch2/catch_message.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include "test_utils.h"

#ifndef TESTDATADIR
#define TESTDATADIR ".."
#endif

namespace
{

using ZT = fplll::Z_NR<mpz_t>;

/**
   @brief Test the testers: neither representation may be reported as reduced.

   @param A  basis matrix of a lattice
   @param G  Gram matrix of the basis A
   @return true when one of the representations is (incorrectly) reported as LLL-reduced.
*/

template <class FT> bool is_already_reduced(fplll::ZZ_mat<mpz_t> &A, fplll::Matrix<ZT> &G)
{
  fplll::ZZ_mat<mpz_t> U;
  fplll::ZZ_mat<mpz_t> UT;

  fplll::MatGSO<ZT, fplll::FP_NR<FT>> M(A, U, UT, fplll::GSOOptions{.int_gram = true});
  fplll::MatGSOGram<ZT, fplll::FP_NR<FT>> M2(G, U, UT, fplll::GSOOptions{.int_gram = true});

  const bool is_reduced  = fplll::is_lll_reduced<ZT, fplll::FP_NR<FT>>(M, fplll::LLLParameters{});
  const bool is_greduced = fplll::is_lll_reduced<ZT, fplll::FP_NR<FT>>(M2, fplll::LLLParameters{});

  {
    INFO("is_lll_reduced must not report the unreduced basis as reduced");
    CHECK_FALSE(is_reduced);
  }
  {
    INFO("is_lll_reduced must not report the unreduced gram matrix as reduced");
    CHECK_FALSE(is_greduced);
  }

  return is_reduced || is_greduced;
}

/**
   @brief Tests whether LLL applied on A is equivalent to LLLGram applied on G = A*A^T

   @param A basis matrix of a lattice
*/

template <class FT> void test_lll(fplll::ZZ_mat<mpz_t> &A)
{
  fplll::ZZ_mat<mpz_t> U;
  fplll::ZZ_mat<mpz_t> UT;

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

  // Create a MatGSO-object M (basis gso) for A and a MatGSOGram-object Mgram
  // (gram gso) for G.
  fplll::MatGSO<ZT, fplll::FP_NR<FT>> M(A, U, UT, fplll::GSOOptions{.int_gram = true});
  M.update_gso();
  fplll::MatGSOGram<ZT, fplll::FP_NR<FT>> Mgram(G, U, UT, fplll::GSOOptions{.int_gram = true});
  Mgram.update_gso();

  // Neither A nor G may be LLL-reduced yet.
  if (is_already_reduced<FT>(A, G))
  {
    // The historical harness gave up on this case here.
    return;
  }

  // Make two LLLObjects: one of the basis MatGSO-object M, one of the gram
  // MatGSOGram-object Mgram, and LLL-reduce both objects.
  fplll::LLLReduction<ZT, fplll::FP_NR<FT>> LLLObj(M, fplll::LLLParameters{});
  fplll::LLLReduction<ZT, fplll::FP_NR<FT>> LLLObjgram(Mgram, fplll::LLLParameters{});

  LLLObj.lll();
  LLLObjgram.lll();

  // Check whether M and Mgram are really reduced after the LLL reduction.
  const bool is_reduced = fplll::is_lll_reduced<ZT, fplll::FP_NR<FT>>(M, fplll::LLLParameters{});
  const bool is_greduced =
      fplll::is_lll_reduced<ZT, fplll::FP_NR<FT>>(Mgram, fplll::LLLParameters{});

  {
    INFO("the basis GSO-object must be LLL-reduced after calling LLL");
    CHECK(is_reduced);
  }
  {
    INFO("the gram GSO-object must be LLL-reduced after calling LLL");
    CHECK(is_greduced);
  }
  if (!is_reduced || !is_greduced)
  {
    // The historical harness gave up on this case here.
    return;
  }

  // After this reduction, A (the input matrix) and G (the gram matrix) are
  // LLL-reduced accordingly. Therefore A * A^T = G must still hold.
  fplll::ZZ_mat<mpz_t> G_reduced;
  G_reduced.resize(r, r);
  for (int i = 0; i < r; i++)
  {
    for (int j = 0; j < r; j++)
    {
      M.b[i].dot_product(G_reduced(i, j), M.b[j], c);
    }
  }

  for (int i = 0; i < r; i++)
  {
    for (int j = 0; j < i; j++)
    {
      if (G(i, j) != G_reduced(i, j))
      {
        UNSCOPED_INFO("the gram-representation and the basis-representation of the same lattice "
                      "must have the same gram matrix; entry ("
                      << i << ", " << j << ") differs");
      }
      CHECK(G(i, j) == G_reduced(i, j));
    }
  }
}

/**
   @brief Test LLL and LLLGram for the matrix stored in the file `input_filename`.

   @param input_filename   a path
*/

template <class FT> void test_filename(const char *input_filename)
{
  fplll::ZZ_mat<mpz_t> A;
  INFO("input file: " << input_filename);
  REQUIRE(read_file(A, input_filename) == 0);
  test_lll<FT>(A);
}

/**
   @brief Construct a d x (d+1) integer relations matrix with bit size b and test LLL and LLLGram.

   @param d                dimension
   @param b                bit size
*/

template <class FT> void test_int_rel(int d, int b)
{
  fplll::ZZ_mat<mpz_t> A;
  A.resize(d, d + 1);
  A.gen_intrel(b);
  test_lll<FT>(A);
}

}  // namespace

// The inputs are read from files, so these cases do not touch the shared
// random state and can run for every floating-point backend independently.
TEMPLATE_TEST_CASE("LLL and LLLGram reduce the same basis and Gram matrix consistently",
                   "[lll][lll-gram]", FPLLL_TEST_FP_BACKENDS)
{
  test_filename<TestType>(TESTDATADIR "/tests/lattices/example2_in");
  test_filename<TestType>(TESTDATADIR "/tests/lattices/example_cvp_in_lattice");
  test_filename<TestType>(TESTDATADIR "/tests/lattices/example_cvp_in_lattice2");
  test_filename<TestType>(TESTDATADIR "/tests/lattices/example_cvp_in_lattice3");
  test_filename<TestType>(TESTDATADIR "/tests/lattices/example_cvp_in_lattice4");
  test_filename<TestType>(TESTDATADIR "/tests/lattices/example_cvp_in_lattice5");
}

// The integer-relation bases are drawn from the shared GMP random state, so all
// of these reductions stay in one test case in their original order. That keeps
// the generated bases identical to the ones the historical main() used.
TEST_CASE("LLL and LLLGram reduce the same integer-relation bases consistently", "[lll][lll-gram]")
{
  test_int_rel<double>(50, 20);
  test_int_rel<double>(40, 10);

  test_int_rel<double>(100, 200);
  test_int_rel<double>(200, 100);

  test_int_rel<mpfr_t>(50, 20);
  test_int_rel<mpfr_t>(40, 10);

#ifdef FPLLL_WITH_QD
  test_int_rel<dd_real>(50, 20);
  test_int_rel<dd_real>(40, 10);

  test_int_rel<qd_real>(50, 20);
  test_int_rel<qd_real>(40, 10);
#endif
}
