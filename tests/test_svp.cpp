// SPDX-FileCopyrightText: 2015 Martin Albrecht 2016 Michael Walter
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test_utils.h"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <fplll/fplll.h>
#include <fplll/gso/gso.h>
#include <fplll/gso/gso_interface.h>
#include <fplll/nr/matrix.h>
#include <fplll/nr/nr.h>

#include <cmath>
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

void require_red_success(const fplll::ReductionReport &report, const char *what)
{
  require_red_success(report.status, what);
}

double norm(const std::vector<fplll::Z_NR<mpz_t>> &v)
{
  fplll::Z_NR<mpz_t> norm, tmp;
  for (const auto &i : v)
  {
    tmp.mul(i, i);
    norm.add(norm, tmp);
  }
  return std::sqrt(norm.get_d());
}

/**
   @brief Test if SVP function returns vector with right norm.

   @param A              input lattice
   @param b              shortest vector
 */
void test_svp(fplll::ZZ_mat<mpz_t> &A, std::vector<fplll::Z_NR<mpz_t>> &b)
{
  std::vector<fplll::Z_NR<mpz_t>> sol_coord;   // In the LLL-reduced basis
  std::vector<fplll::Z_NR<mpz_t>> sol_coord2;  // In the initial basis
  std::vector<fplll::Z_NR<mpz_t>> solution;
  fplll::ZZ_mat<mpz_t> u(A.get_rows(), A.get_rows());

  const fplll::ReductionReport report = fplll::lll_reduction(A, u);
  require_red_success(report, "LLL reduction failed");

  const fplll::RedStatus status =
      fplll::shortest_vector(A, sol_coord, fplll::SVPOptions{.method = fplll::SVPMethod::proved});
  require_red_success(status, "shortest_vector failed");

  fplll::vector_matrix_product(sol_coord2, sol_coord, u);
  fplll::vector_matrix_product(solution, sol_coord, A);

  fplll::Z_NR<mpz_t> tmp;
  fplll::Z_NR<mpz_t> norm_s;
  fplll::Z_NR<mpz_t> norm_b;

  for (int i = 0; i < A.get_cols(); i++)
  {
    tmp.mul(solution[i], solution[i]);
    norm_s.add(norm_s, tmp);

    tmp.mul(b[i], b[i]);
    norm_b.add(norm_b, tmp);
  }
  CAPTURE(norm_s, norm_b);
  CHECK(norm_s == norm_b);
}

/**
   @brief Compute the norm of a dual vector (specified by coefficients in the dual basis).

   @param A              input lattice
   @param b              coefficients of shortest dual vector
 */
void dual_length(fplll::FP_NR<mpfr_t> &norm, fplll::ZZ_mat<mpz_t> &A,
                 const std::vector<fplll::Z_NR<mpz_t>> &coords)
{
  int d = static_cast<int>(coords.size());
  {
    INFO("DSVP length error: coefficient vector has wrong dimension: " << A.get_rows() << " vs "
                                                                       << d);
    REQUIRE(A.get_rows() == d);
  }
  std::vector<fplll::FP_NR<mpfr_t>> coords_d(d);
  for (int i = 0; i < d; i++)
  {
    coords_d[i] = coords[i].get_d();
  }

  fplll::ZZ_mat<mpz_t> empty_mat;
  fplll::MatGSO<fplll::Z_NR<mpz_t>, fplll::FP_NR<mpfr_t>> gso(A, empty_mat, empty_mat,
                                                              fplll::GSOOptions{.int_gram = true});
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

   @param A              input lattice
   @param b              shortest dual vector
 */
void test_dual_svp(fplll::ZZ_mat<mpz_t> &A, std::vector<fplll::Z_NR<mpz_t>> &b)
{
  std::vector<fplll::Z_NR<mpz_t>> sol_coord;  // In the LLL-reduced basis
  fplll::ZZ_mat<mpz_t> u;

  fplll::FP_NR<mpfr_t> normb;
  dual_length(normb, A, b);

  const fplll::ReductionReport report = fplll::lll_reduction(A, u);
  require_red_success(report, "LLL reduction failed");

  const fplll::RedStatus status = fplll::shortest_vector(
      A, sol_coord, fplll::SVPOptions{.method = fplll::SVPMethod::fast, .dual = true});
  require_red_success(status, "shortest_vector failed");

  fplll::FP_NR<mpfr_t> norm_sol;
  dual_length(norm_sol, A, sol_coord);

  fplll::FP_NR<mpfr_t> error;
  error = 1;
  error.mul_2si(error, -static_cast<int>(fplll::FP_NR<mpfr_t>::get_prec()));
  normb += error;
  INFO("Returned dual vector too long by more than " << error);
  CHECK(norm_sol <= normb);
}

/**
   @brief test that proved is no worse than fast.

   @param d large dimension of initial lattice
   @param d2 enumeration dimension
 */
void test_svp_proved_vs_fast(std::size_t d, std::size_t d2)
{
  // This is the only test in this program that consumes random numbers; the
  // seed is the one the old main() installed.
  fplll::RandGen::init_with_seed(0);

  fplll::ZZ_mat<mpz_t> A(static_cast<int>(d), static_cast<int>(d));
  fplll::Z_NR<mpz_t> q;
  q = 65537;
  A.gen_qary(static_cast<int>(d / 2), q);

  require_red_success(fplll::lll_reduction(A), "LLL reduction failed");

  fplll::ZZ_mat<mpz_t> B(static_cast<int>(d2),
                         static_cast<int>(d));  // so that LLL doesn't just solve it
  std::vector<fplll::Z_NR<mpz_t>> sol_coord_proved, sol_coord_fast;
  std::vector<fplll::Z_NR<mpz_t>> solution_proved, solution_fast;

  for (std::size_t i = 0; i < d2; i++)
  {
    for (int j = 0; j < A.get_cols(); j++)
    {
      B[static_cast<int>(i)][j] = A[static_cast<int>(i)][j];
    }
  }

  fplll::RedStatus status = fplll::shortest_vector(
      B, sol_coord_proved, fplll::SVPOptions{.method = fplll::SVPMethod::proved});
  require_red_success(status, "shortest_vector failed");

  status = fplll::shortest_vector(B, sol_coord_fast,
                                  fplll::SVPOptions{.method = fplll::SVPMethod::fast});
  require_red_success(status, "shortest_vector failed");

  fplll::vector_matrix_product(solution_proved, sol_coord_proved, B);
  fplll::vector_matrix_product(solution_fast, sol_coord_fast, B);
  CHECK_FALSE(norm(solution_fast) < norm(solution_proved));
}

void test_rank_defect()
{
  fplll::ZZ_mat<mpz_t> A = fplll::ZZ_mat<mpz_t>(4, 4);
  A[0][0]                = 75;
  A[0][1]                = 44;
  A[0][2]                = -5;
  A[0][3]                = -16;
  A[1][0]                = 29;
  A[1][1]                = 7;
  A[1][2]                = -52;
  A[1][3]                = 134;
  A[2][0]                = -89;
  A[2][1]                = 108;
  A[2][2]                = 56;
  A[2][3]                = 29;
  A[3][0]                = -94;
  A[3][1]                = 55;
  A[3][2]                = -194;
  A[3][3]                = -20;
  require_red_success(fplll::lll_reduction(A), "LLL reduction failed");
  std::vector<fplll::Z_NR<mpz_t>> sol_coord;
  CHECK(fplll::shortest_vector(A, sol_coord) == fplll::RedStatus::success);
}

/**
   @brief Test if SVP function returns vector with right norm.

   @param input_filename   filename of an input lattice
   @param output_filename  filename of a shortest vector
 */
void test_svp_filename(const char *input_filename, const char *output_filename)
{
  fplll::ZZ_mat<mpz_t> A;
  REQUIRE(read_file(A, input_filename) == 0);

  std::vector<fplll::Z_NR<mpz_t>> b;
  REQUIRE(read_file(b, output_filename) == 0);

  test_svp(A, b);
}

void test_dual_svp_filename(const char *input_filename, const char *output_filename)
{
  fplll::ZZ_mat<mpz_t> A;
  REQUIRE(read_file(A, input_filename) == 0);

  std::vector<fplll::Z_NR<mpz_t>> b;
  REQUIRE(read_file(b, output_filename) == 0);

  test_dual_svp(A, b);
}

}  // namespace

TEST_CASE("shortest_vector returns the shortest vector of example_svp_in", "[svp]")
{
  test_svp_filename(TESTDATADIR "/tests/lattices/example_svp_in",
                    TESTDATADIR "/tests/lattices/example_svp_out");
}

TEST_CASE("shortest_vector with dual=true is short for example_dsvp_in", "[svp]")
{
  test_dual_svp_filename(TESTDATADIR "/tests/lattices/example_dsvp_in",
                         TESTDATADIR "/tests/lattices/example_dsvp_out");
}

TEST_CASE("shortest_vector handles a rank-deficient basis", "[svp]") { test_rank_defect(); }

TEST_CASE("proved SVP is no worse than fast SVP", "[svp]") { test_svp_proved_vs_fast(100, 30); }
