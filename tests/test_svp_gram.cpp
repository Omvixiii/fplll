// SPDX-FileCopyrightText: 2015 Martin Albrecht 2016 Michael Walter
// SPDX-FileCopyrightText: 2019 Koen de Boer & Wessel van Woerden
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test_utils.h"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <fplll/fplll.h>
#include <fplll/gso/gso_gram.h>
#include <fplll/gso/gso_interface.h>
#include <fplll/nr/matrix.h>
#include <fplll/nr/nr.h>

#include "../src/reduction/lll/lll_kernel.h"

#include <cstddef>
#include <gmp.h>
#include <vector>

#ifndef TESTDATADIR
#define TESTDATADIR ".."
#endif

namespace
{

// A failed reduction means the rest of the check is meaningless; the INFO
// stays local to this helper so it does not leak into later assertions.
void require_red_success(fplll::RedStatus status, const char *what)
{
  INFO(what << ": " << fplll::to_string(status));
  REQUIRE(fplll::succeeded(status));
}

// The old harness treated a Gram matrix that is not LLL-reduced as a hard
// failure for the current fixture.
void require_lll_reduced(fplll::MatGSOGram<fplll::Z_NR<mpz_t>, fplll::FP_NR<mpfr_t>> &Mgram)
{
  const bool is_greduced = fplll::is_lll_reduced(Mgram, fplll::LLLParameters{});
  {
    INFO("LLL reduction failed");
    REQUIRE(is_greduced);
  }
}

/**
   @brief Test if SVP function returns vector with right norm.

   @param G              input lattice Gram matrix
   @param b              shortest vector
 */
void test_svp(fplll::ZZ_mat<mpz_t> &G, std::vector<fplll::Z_NR<mpz_t>> &b)
{
  std::vector<fplll::Z_NR<mpz_t>> sol_coord;  // In the LLL-reduced gram matrix

  fplll::ZZ_mat<mpz_t> U;
  fplll::ZZ_mat<mpz_t> UT;

  // Make GSO object of G & apply GramSchmidt
  fplll::MatGSOGram<fplll::Z_NR<mpz_t>, fplll::FP_NR<mpfr_t>> Mgram(
      G, U, UT, fplll::GSOOptions{.int_gram = true});
  Mgram.update_gso();

  // Compute the length of b w.r.t. G
  fplll::Z_NR<mpz_t> norm_b;
  Mgram.sqnorm_coordinates(norm_b, b);

  // Make LLL object & apply LLL
  fplll::LLLReduction<fplll::Z_NR<mpz_t>, fplll::FP_NR<mpfr_t>> LLLObjgram(Mgram);
  LLLObjgram.lll();

  // Check if LLL reduced
  require_lll_reduced(Mgram);

  // Symmetrize the Gram Matrix
  Mgram.symmetrize_g();

  // Apply SVP algorithm, check whether it yields success
  fplll::RedStatus status = fplll::shortest_vector(
      Mgram, sol_coord, fplll::SVPOptions{.method = fplll::SVPMethod::proved});
  require_red_success(status, "shortest_vector failed");

  // Compare the length of found solution with given solution b.
  fplll::Z_NR<mpz_t> norm_s;
  Mgram.sqnorm_coordinates(norm_s, sol_coord);

  CAPTURE(norm_s, norm_b);
  CHECK(norm_s == norm_b);

  // Apply list svp algorithm
  std::vector<std::vector<fplll::Z_NR<mpz_t>>> sols_coord;
  std::vector<fplll::enumf> sols_dist;
  status = fplll::shortest_vectors(Mgram, sols_coord, sols_dist, 500,
                                   fplll::SVPOptions{.method = fplll::SVPMethod::proved});
  require_red_success(status, "shortest_vectors failed");
}

/**
   @brief Compute the norm of a dual vector (specified by coefficients in the dual basis).

   @param G              input lattice Gram matrix
   @param b              coefficients of shortest dual vector
 */
void dual_length(fplll::FP_NR<mpfr_t> &norm, fplll::ZZ_mat<mpz_t> &G,
                 const std::vector<fplll::Z_NR<mpz_t>> &coords)
{
  int d = static_cast<int>(coords.size());
  {
    INFO("DSVP length error: coefficient vector has wrong dimension: " << G.get_rows() << " vs "
                                                                       << d);
    REQUIRE(G.get_rows() == d);
  }
  std::vector<fplll::FP_NR<mpfr_t>> coords_d(d);
  for (int i = 0; i < d; i++)
  {
    coords_d[i] = coords[i].get_d();
  }

  fplll::ZZ_mat<mpz_t> empty_mat;
  fplll::MatGSOGram<fplll::Z_NR<mpz_t>, fplll::FP_NR<mpfr_t>> gso(
      G, empty_mat, empty_mat, fplll::GSOOptions{.int_gram = true});
  {
    INFO("GSO Failure.");
    REQUIRE(gso.update_gso());
  }
  fplll::FP_NR<mpfr_t> tmp;
  gso.get_r(tmp, d - 1, d - 1);
  tmp.pow_si(tmp, -1);

  std::vector<fplll::FP_NR<mpfr_t>> alpha(d);
  fplll::FP_NR<mpfr_t> mu, alpha2, r_inv;
  norm = 0.0;
  for (int i = 0; i < d; i++)
  {
    alpha[i] = coords_d[i];
    for (int j = 0; j < i; j++)
    {
      gso.get_mu(mu, i, j);
      alpha[i] -= mu * alpha[j];
    }
    gso.get_r(r_inv, i, i);
    r_inv.pow_si(r_inv, -1);
    alpha2.pow_si(alpha[i], 2);
    norm += alpha2 * r_inv;
  }
}

/**
   @brief Test if dual SVP function returns vector with right norm.

   @param G              input lattice Gram matrix
   @param b              shortest dual vector
 */
void test_dual_svp(fplll::ZZ_mat<mpz_t> &G, std::vector<fplll::Z_NR<mpz_t>> &b)
{
  std::vector<fplll::Z_NR<mpz_t>> sol_coord;  // In the LLL-reduced basis

  fplll::FP_NR<mpfr_t> normb;
  dual_length(normb, G, b);

  // Make GSO object of G & apply GramSchmidt
  fplll::ZZ_mat<mpz_t> empty_mat;
  fplll::MatGSOGram<fplll::Z_NR<mpz_t>, fplll::FP_NR<mpfr_t>> Mgram(
      G, empty_mat, empty_mat, fplll::GSOOptions{.int_gram = true});
  Mgram.update_gso();

  // Make LLL object & apply LLL
  fplll::LLLReduction<fplll::Z_NR<mpz_t>, fplll::FP_NR<mpfr_t>> LLLObjgram(Mgram);
  LLLObjgram.lll();

  // Check if LLL reduced
  require_lll_reduced(Mgram);

  // Symmetrize the Gram Matrix
  Mgram.symmetrize_g();

  fplll::RedStatus status = fplll::shortest_vector(
      Mgram, sol_coord, fplll::SVPOptions{.method = fplll::SVPMethod::fast, .dual = true});
  require_red_success(status, "shortest_vector failed");

  fplll::FP_NR<mpfr_t> norm_sol;
  dual_length(norm_sol, G, sol_coord);

  fplll::FP_NR<mpfr_t> error;
  error = 1;
  error.mul_2si(error, -static_cast<int>(fplll::FP_NR<mpfr_t>::get_prec()));
  normb += error;
  INFO("Returned dual vector too long by more than " << error);
  CHECK(norm_sol <= normb);
}

/**
   @brief Test if SVP function returns vector with right norm.

   @param input_filename   filename of an input Gram matrix
   @param output_filename  filename of a shortest vector
 */
void test_svp_filename(const char *input_filename, const char *output_filename)
{
  fplll::ZZ_mat<mpz_t> G;
  REQUIRE(read_file(G, input_filename) == 0);

  std::vector<fplll::Z_NR<mpz_t>> b;
  REQUIRE(read_file(b, output_filename) == 0);

  test_svp(G, b);
}

void test_dual_svp_filename(const char *input_filename, const char *output_filename)
{
  fplll::ZZ_mat<mpz_t> G;
  REQUIRE(read_file(G, input_filename) == 0);

  std::vector<fplll::Z_NR<mpz_t>> b;
  REQUIRE(read_file(b, output_filename) == 0);

  test_dual_svp(G, b);
}

}  // namespace

TEST_CASE("SVP on a dimension 7 Gram matrix finds the shortest vector", "[svp][gso-gram]")
{
  test_svp_filename(TESTDATADIR "/tests/lattices/grammatrix_dimension7",
                    TESTDATADIR "/tests/lattices/grammatrix_dimension7_out");
}

TEST_CASE("SVP on a dimension 4 Gram matrix finds the shortest vector", "[svp][gso-gram]")
{
  test_svp_filename(TESTDATADIR "/tests/lattices/grammatrix_dimension4",
                    TESTDATADIR "/tests/lattices/grammatrix_dimension4_out");
}

TEST_CASE("dual SVP on a dimension 7 Gram matrix returns a short vector", "[svp][gso-gram]")
{
  test_dual_svp_filename(TESTDATADIR "/tests/lattices/grammatrix_dimension7",
                         TESTDATADIR "/tests/lattices/grammatrix_dimension7_out");
}

TEST_CASE("dual SVP on a dimension 4 Gram matrix returns a short vector", "[svp][gso-gram]")
{
  test_dual_svp_filename(TESTDATADIR "/tests/lattices/grammatrix_dimension4",
                         TESTDATADIR "/tests/lattices/grammatrix_dimension4_out");
}
