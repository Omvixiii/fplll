// SPDX-FileCopyrightText: 2015 Martin Albrecht
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <fplll/defs.h>
#include <fplll/gso/gso.h>
#include <fplll/gso/gso_gram.h>
#include <fplll/gso/gso_interface.h>
#include <fplll/gso/householder.h>
#include <fplll/nr/matrix.h>
#include <fplll/nr/nr.h>

#include <cmath>
#include <cstddef>
#include <gmp.h>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#if defined(FPLLL_DEBUG_CHECKS) && !defined(_WIN32)
#include <csignal>
#include <cstdio>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "test_utils.h"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#ifndef TESTDATADIR
#define TESTDATADIR ".."
#endif

namespace
{

template <class ZT, class FT>
fplll::Matrix<FT> matrix_relative_difference(const fplll::Matrix<FT> &r1,
                                             const fplll::Matrix<FT> &r2)
{
  fplll::Matrix<FT> diff_matrix = fplll::Matrix<FT>(r1.get_rows(), r1.get_cols());
  diff_matrix.fill(0.0);
  FT relativation_factor = 0.0;
  for (int i = 0; i < r1.get_rows(); i++)
  {
    for (int j = 0; j < i; j++)
    {  // j < i, because r is lower-triangular, and has only 1 on the diagonal.
      relativation_factor = fplll::abs(r1[i][j]) + fplll::abs(r2[i][j]);
      if (relativation_factor.is_zero())
      {
        diff_matrix[i][j] = fplll::abs(r1[i][j] - r2[i][j]);
      }
      else
      {
        diff_matrix[i][j] = fplll::abs(r1[i][j] - r2[i][j]) / relativation_factor;
      }
    }
  }
  return diff_matrix;
}

// Check that the r-matrices of M1 and M2 are entry-wise equal, up to an error 'error'.
// The relative difference matrix is reported on failure.
template <class ZT, class FT>
void require_rs_equal(const fplll::MatGSO<ZT, FT> &M1, const fplll::MatGSOGram<ZT, FT> &M2,
                      const FT &error, const char *case_name)
{
  const fplll::Matrix<FT> &r1 = M1.get_r_matrix();
  const fplll::Matrix<FT> &r2 = M2.get_r_matrix();
  fplll::Matrix<FT> diff      = matrix_relative_difference<ZT, FT>(r1, r2);

  FT max_entry = 0.0;
  max_entry    = diff.get_max();
  if (max_entry > error)
  {
    std::ostringstream out;
    diff.print(out);
    INFO("case: " << case_name << '\n' << "relative difference matrix:\n" << out.str());
  }
  CHECK(max_entry <= error);
}

template <class FT> void assert_diag_R(const FT &rhd, int i)
{
  INFO("R(" << i << ", " << i << ") must be positive");
  CHECK(rhd.cmp(0.0) > 0);
}

template <class FT>
void assert_mu_r_householder(const FT &rh, const FT &rhd, const FT &mu, const FT &r)
{
  {
    INFO("Error (mu): " << mu << " != " << rh / rhd);
    CHECK(fplll::abs(mu - rh / rhd) <= 0.001);
  }
  {
    INFO("Error (r): " << r << " != " << rh * rhd);
    CHECK(fplll::abs(r - rh * rhd) <= 0.001);
  }
}

/**
 * Test if:
 *   mu(i, j) = R(i, j) / R(j, j)
 *   r(i, j) = R(i, j) * R(j, j)
 */
template <class ZT, class FT> void test_householder(fplll::ZZ_mat<ZT> &A)
{
  fplll::ZZ_mat<ZT> U;
  fplll::ZZ_mat<ZT> UT;
  fplll::MatGSO<fplll::Z_NR<ZT>, fplll::FP_NR<FT>> M(A, U, UT, fplll::GSOOptions{.int_gram = true});
  // With the default options, the exponent returned by get_R(_naively) must be equal to zero.
  fplll::MatHouseholder<fplll::Z_NR<ZT>, fplll::FP_NR<FT>> Mhouseholder(A, U, UT);
  M.update_gso();
  // Here, we just need to refresh R. However, refresh_R() does not modify n_known_rows, but
  // refresh_R_bf() does.
  Mhouseholder.refresh_R_bf();
  Mhouseholder.update_R();
  Mhouseholder.update_R_naively();

  fplll::FP_NR<FT> r;
  fplll::FP_NR<FT> rh;
  fplll::FP_NR<FT> rhd;
  fplll::FP_NR<FT> mu;
  long expo;

  for (int i = 0; i < A.get_rows(); i++)
  {
    Mhouseholder.get_R(rhd, i, i, expo);
    REQUIRE(expo == 0);
    assert_diag_R(rhd, i);
    Mhouseholder.get_R_naively(rhd, i, i, expo);
    REQUIRE(expo == 0);
    assert_diag_R(rhd, i);
  }

  for (int i = 0; i < A.get_rows(); i++)
  {
    for (int j = 0; j < i; j++)
    {
      M.get_r(r, i, j);
      M.get_mu(mu, i, j);
      Mhouseholder.get_R(rh, i, j, expo);
      REQUIRE(expo == 0);
      Mhouseholder.get_R(rhd, j, j, expo);
      REQUIRE(expo == 0);
      assert_mu_r_householder(rh, rhd, mu, r);
      Mhouseholder.get_R_naively(rh, i, j, expo);
      REQUIRE(expo == 0);
      Mhouseholder.get_R_naively(rhd, j, j, expo);
      REQUIRE(expo == 0);
      assert_mu_r_householder(rh, rhd, mu, r);
    }
  }
}

template <class ZT, class FT> void test_ggso(fplll::ZZ_mat<ZT> &A)
{
  // TEST A
  // Method:
  // - Apply 'normal' MatGSO to A.
  // - Extract r-matrix of A
  // - Compute G = A^T A.
  // - Apply gram MatGSO to G.
  // - Extract r-matrix of G
  // -> The r-matrices should be equal.

  // TEST B
  // Apply some 'random' elementary operation on A and on G
  // (of course, the operations on A and G are only 'abstractly' the same)
  // check if their r-matrices are still equal.

  // TEST A
  // ----------------------------
  int r = A.r;

  fplll::ZZ_mat<ZT> U;
  fplll::ZZ_mat<ZT> UT;

  fplll::MatGSO<fplll::Z_NR<ZT>, fplll::FP_NR<FT>> Mbuf(A, U, UT,
                                                        fplll::GSOOptions{.int_gram = true});
  Mbuf.update_gso();
  fplll::Matrix<fplll::Z_NR<ZT>> G = Mbuf.get_g_matrix();

  fplll::MatGSO<fplll::Z_NR<ZT>, fplll::FP_NR<FT>> M(A, U, UT);
  M.update_gso();
  fplll::MatGSOGram<fplll::Z_NR<ZT>, fplll::FP_NR<FT>> M2(G, U, UT);
  M2.update_gso();

  fplll::ZZ_mat<ZT> A1(A);
  fplll::MatGSO<fplll::Z_NR<ZT>, fplll::FP_NR<FT>> M3(A1, U, UT,
                                                      fplll::GSOOptions{.int_gram = true});
  M3.update_gso();

  fplll::FP_NR<FT> err = .001;
  require_rs_equal(M, M2, err, "grammatrix representation and basis representation");

  // TEST B
  // ------------------------

  for (int i = 0; i < (fplll::RandGenInt::get() & 0x7fffffff) % 10 + 1; i++)
  {
    int k = (fplll::RandGenInt::get() & 0x7fffffff) % r;
    int j = (fplll::RandGenInt::get() & 0x7fffffff) % r;
    M.move_row(k, j);
    M2.move_row(k, j);
    M3.move_row(k, j);
  }
  M.update_gso();
  M2.update_gso();
  M3.update_gso();
  require_rs_equal(M, M2, err,
                   "grammatrix representation and basis representation after moving rows");

  {
    auto batch_m  = M.row_operations(0, r);
    auto batch_m2 = M2.row_operations(0, r);
    auto batch_m3 = M3.row_operations(0, r);
    for (int i = 0; i < (fplll::RandGenInt::get() & 0x7fffffff) % 10 + 1; i++)
    {
      int k = (fplll::RandGenInt::get() & 0x7fffffff) % r;
      int j = (fplll::RandGenInt::get() & 0x7fffffff) % r;
      M.row_add(k, j);
      M2.row_add(k, j);
      M3.row_add(k, j);
    }
  }

  M.update_gso();
  M2.update_gso();
  M3.update_gso();
  require_rs_equal(M, M2, err,
                   "grammatrix representation and basis representation after adding rows");
  require_rs_equal(M3, M2, err, "integer Gram matrix and Gram matrix after adding rows");
}

template <class ZT, class FT>
double calculate_slope(fplll::MatGSOInterface<ZT, FT> *M, int start_row, int stop_row)
{
  FT f, log_f;
  long expo;
  std::vector<double> x;
  x.resize(stop_row);
  for (int i = start_row; i < stop_row; i++)
  {
    M->update_gso_row(i);
    f = M->get_r_exp(i, i, expo);
    log_f.log(f, GMP_RNDU);
    x[i] = log_f.get_d() + static_cast<double>(expo) * std::log(2.0);
  }
  int n         = stop_row - start_row;
  double i_mean = 0, x_mean = 0, v1 = 0, v2 = 0;
  for (int i = start_row; i < stop_row; i++)
  {
    i_mean += i;
    x_mean += x[i];
  }
  i_mean /= n;
  x_mean /= n;
  for (int i = start_row; i < stop_row; i++)
  {
    v1 += (i - i_mean) * (x[i] - x_mean);
    v2 += (i - i_mean) * (i - i_mean);
  }
  return v1 / v2;
}

template <class ZT, class FT>
void test_current_slope(fplll::ZZ_mat<ZT> &A, int start_row, int stop_row)
{
  fplll::ZZ_mat<ZT> U, UT;

  fplll::MatGSO<fplll::Z_NR<ZT>, fplll::FP_NR<FT>> M(A, U, UT);
  M.update_gso();

  double slope_found = M.get_current_slope(start_row, stop_row);
  double slope_expected =
      calculate_slope<fplll::Z_NR<ZT>, fplll::FP_NR<FT>>(&M, start_row, stop_row);
  double abs_diff = std::abs(slope_found - slope_expected);

  // Bounded absolute error
  CHECK(abs_diff <= 1e-12);
  if (slope_expected != 0.0)
  {
    // Bounded relative error
    CHECK(std::abs(abs_diff / slope_expected) <= 1e-12);
  }
}

// Runs every GSO check the old harness applied to one basis matrix.
template <class ZT, class FT> void test_matrix(fplll::ZZ_mat<ZT> &A, const std::string &name)
{
  INFO("matrix: " << name << ", backend: " << fplll::num_type_str<FT>());
  test_ggso<ZT, FT>(A);
  test_householder<ZT, FT>(A);
  test_current_slope<ZT, FT>(A, 0, A.get_rows());
}

template <class ZT, class FT> void test_filename(const char *input_filename)
{
  fplll::ZZ_mat<ZT> A;
  REQUIRE(read_file(A, input_filename) == 0);
  test_matrix<ZT, FT>(A, input_filename);
}

/**
   @brief Construct d × (d+1) integer relations matrix with bit size b and test it.

   @param d                dimension
   @param b                bit size
 */
template <class ZT, class FT> void test_int_rel(int d, int b)
{
  fplll::ZZ_mat<ZT> A;
  A.resize(d, d + 1);
  A.gen_intrel(b);
  test_matrix<ZT, FT>(A, "integer relation matrix (d = " + std::to_string(d) +
                             ", b = " + std::to_string(b) + ")");
}

/** Installs an MPFR default precision for the lifetime of the guard. */
class ScopedDefaultPrecision
{
public:
  explicit ScopedDefaultPrecision(unsigned int precision) : saved_(fplll::FP_NR<mpfr_t>::get_prec())
  {
    fplll::FP_NR<mpfr_t>::set_prec(precision);
  }

  ~ScopedDefaultPrecision() { fplll::FP_NR<mpfr_t>::set_prec(saved_); }

  ScopedDefaultPrecision(const ScopedDefaultPrecision &)            = delete;
  ScopedDefaultPrecision &operator=(const ScopedDefaultPrecision &) = delete;
  ScopedDefaultPrecision(ScopedDefaultPrecision &&)                 = delete;
  ScopedDefaultPrecision &operator=(ScopedDefaultPrecision &&)      = delete;

private:
  unsigned int saved_;
};

#if defined(FPLLL_DEBUG_CHECKS) && !defined(_WIN32)

/**
 * Runs `operation` in a child process and checks that the child dies from
 * SIGABRT.
 *
 * The child inherits the state of the parent, including Catch2's signal
 * handlers, so it resets the SIGABRT disposition and must not use any Catch2
 * assertion: the parent assertions are the only way to observe the outcome.
 */
template <class Operation> void require_sigabrt(Operation operation)
{
  static_cast<void>(std::fflush(nullptr));
  const pid_t pid = fork();
  REQUIRE(pid >= 0);
  if (pid == 0)
  {
    // The abort is the expected outcome: keep the test framework from claiming
    // the signal, and keep the child's diagnostics out of the test output.
    static_cast<void>(std::signal(SIGABRT, SIG_DFL));
    static_cast<void>(std::freopen("/dev/null", "w", stdout));
    static_cast<void>(std::freopen("/dev/null", "w", stderr));
    try
    {
      operation();
    }
    catch (...)
    {
      _exit(2);  // an exception is not the expected abort
    }
    _exit(0);  // the write operation was accepted although no batch was open
  }
  int status = 0;
  REQUIRE(waitpid(pid, &status, 0) == pid);
  INFO("child wait status: " << status);
  CHECK(WIFSIGNALED(status));
  if (WIFSIGNALED(status))
  {
    CHECK(WTERMSIG(status) == SIGABRT);
  }
}

/** b[1] := b[1] + b[0] on a fresh GSO without an open row_operations() batch. */
template <class FT> void gso_row_add_without_batch()
{
  using ZT = fplll::Z_NR<mpz_t>;
  fplll::ZZ_mat<mpz_t> A(2, 2);
  A.gen_identity(2);
  fplll::ZZ_mat<mpz_t> U, UT;
  fplll::MatGSO<ZT, fplll::FP_NR<FT>> M(A, U, UT);
  M.update_gso();
  M.row_add(1, 0);
}

/** b[1] := b[1] - b[0] on a fresh GSO without an open row_operations() batch. */
template <class FT> void gso_row_sub_without_batch()
{
  using ZT = fplll::Z_NR<mpz_t>;
  fplll::ZZ_mat<mpz_t> A(2, 2);
  A.gen_identity(2);
  fplll::ZZ_mat<mpz_t> U, UT;
  fplll::MatGSO<ZT, fplll::FP_NR<FT>> M(A, U, UT);
  M.update_gso();
  M.row_sub(1, 0);
}

/** row_addmul() on a fresh GSO without an open row_operations() batch. */
template <class FT> void gso_row_addmul_without_batch()
{
  using ZT = fplll::Z_NR<mpz_t>;
  fplll::ZZ_mat<mpz_t> A(2, 2);
  A.gen_identity(2);
  fplll::ZZ_mat<mpz_t> U, UT;
  fplll::MatGSO<ZT, fplll::FP_NR<FT>> M(A, U, UT);
  M.update_gso();
  fplll::FP_NR<FT> one = 1.0;
  M.row_addmul(1, 0, one);
}

/** row_addmul_we() on a fresh GSO without an open row_operations() batch. */
template <class FT> void gso_row_addmul_we_without_batch()
{
  using ZT = fplll::Z_NR<mpz_t>;
  fplll::ZZ_mat<mpz_t> A(2, 2);
  A.gen_identity(2);
  fplll::ZZ_mat<mpz_t> U, UT;
  fplll::MatGSO<ZT, fplll::FP_NR<FT>> M(A, U, UT);
  M.update_gso();
  fplll::FP_NR<FT> one = 1.0;
  M.row_addmul_we(1, 0, one, 0);
}

/** MatGSOGram::row_add() without an open row_operations() batch. */
template <class FT> void gram_row_add_without_batch()
{
  using ZT = fplll::Z_NR<mpz_t>;
  fplll::ZZ_mat<mpz_t> A(2, 2);
  A.gen_identity(2);
  fplll::ZZ_mat<mpz_t> U, UT;
  fplll::MatGSO<ZT, fplll::FP_NR<FT>> buffer(A, U, UT, fplll::GSOOptions{.int_gram = true});
  buffer.update_gso();
  fplll::Matrix<ZT> G = buffer.get_g_matrix();
  fplll::MatGSOGram<ZT, fplll::FP_NR<FT>> M(G, U, UT);
  M.update_gso();
  M.row_add(1, 0);
}

/** Writes a row that the open row_operations() batch does not cover. */
template <class FT> void gso_row_add_outside_batch_range()
{
  using ZT = fplll::Z_NR<mpz_t>;
  fplll::ZZ_mat<mpz_t> A(2, 2);
  A.gen_identity(2);
  fplll::ZZ_mat<mpz_t> U, UT;
  fplll::MatGSO<ZT, fplll::FP_NR<FT>> M(A, U, UT);
  M.update_gso();
  auto batch = M.row_operations(0, 1);  // the batch covers row 0 only
  M.row_add(1, 0);                      // row 1 is outside the batch
}

#endif  // FPLLL_DEBUG_CHECKS && !_WIN32

// The old main() ran the same nine matrices for every floating-point backend, in
// this order and without seeding the RNG; keeping the grouping preserves the
// random row operations and integer relation matrices exactly.
template <class FT> void test_all_matrices()
{
  test_filename<mpz_t, FT>(TESTDATADIR "/tests/lattices/example2_in");
  test_filename<mpz_t, FT>(TESTDATADIR "/tests/lattices/example3_in");
  test_filename<mpz_t, FT>(TESTDATADIR "/tests/lattices/example_cvp_in_lattice");
  test_filename<mpz_t, FT>(TESTDATADIR "/tests/lattices/example_cvp_in_lattice2");
  test_filename<mpz_t, FT>(TESTDATADIR "/tests/lattices/example_cvp_in_lattice3");
  test_filename<mpz_t, FT>(TESTDATADIR "/tests/lattices/example_cvp_in_lattice4");
  test_filename<mpz_t, FT>(TESTDATADIR "/tests/lattices/example_cvp_in_lattice5");
  test_int_rel<mpz_t, FT>(50, 20);
  test_int_rel<mpz_t, FT>(40, 10);
}

}  // namespace

// The old main() ran the same nine matrices for every floating-point backend without
// seeding the RNG, so the whole sequence below must stay in one test case and in this
// order: the global random stream feeds the row operations and integer relation
// matrices, and reordering (e.g. by TEMPLATE_TEST_CASE backend expansion) would change
// those inputs.
TEST_CASE("GSO dumps agree between basis and Gram representations", "[gso]")
{
  test_all_matrices<double>();
  test_all_matrices<mpfr_t>();
#ifdef FPLLL_WITH_QD
  test_all_matrices<dd_real>();
  test_all_matrices<qd_real>();
#endif
}

// The cases below do not touch the global RNG. They are declared after the RNG
// dependent case above so that its input sequence stays unchanged.
TEST_CASE("Row operations are an RAII batch and the GSO state is read-only", "[gso]")
{
  using ZT            = fplll::Z_NR<mpz_t>;
  using FT            = fplll::FP_NR<double>;
  using GSO           = fplll::MatGSO<ZT, FT>;
  using RowOperations = GSO::RowOperations;

  static_assert(!std::is_copy_constructible_v<RowOperations>);
  static_assert(!std::is_copy_assignable_v<RowOperations>);
  static_assert(std::is_move_constructible_v<RowOperations>);
  static_assert(std::is_move_assignable_v<RowOperations>);
  static_assert(std::is_nothrow_destructible_v<RowOperations>);

  fplll::ZZ_mat<mpz_t> A;
  REQUIRE(read_file(A, TESTDATADIR "/tests/lattices/example2_in") == 0);
  fplll::ZZ_mat<mpz_t> U, UT;
  GSO M(A, U, UT);
  M.update_gso();

  const int d = M.get_d();
  REQUIRE(d >= 2);
  CHECK(d == A.get_rows());
  CHECK(d == M.get_rows_of_b());

  // Structural changes are reflected by get_d().
  M.create_row();
  CHECK(M.get_d() == d + 1);
  M.remove_last_row();
  CHECK(M.get_d() == d);
  M.update_gso();

  // Apply a row operation inside an RAII batch. The batch (and hence the cache
  // invalidation) ends at the closing brace.
  {
    auto batch = M.row_operations(0, d);
    M.row_add(1, 0);
  }
  M.update_gso();

  // The incrementally updated GSO must agree with a GSO computed from scratch
  // on the modified basis.
  fplll::ZZ_mat<mpz_t> A_copy(A);
  GSO fresh(A_copy, U, UT);
  fresh.update_gso();
  const fplll::Matrix<FT> &r_batch = M.get_r_matrix();
  const fplll::Matrix<FT> &r_fresh = fresh.get_r_matrix();
  for (int i = 0; i < d; i++)
  {
    for (int j = 0; j <= i; j++)
    {
      INFO("r(" << i << ", " << j << ") differs");
      CHECK(fplll::abs(r_batch(i, j) - r_fresh(i, j)) <= 0.001);
    }
  }

  // Row exponents are only populated when the option is enabled.
  CHECK_FALSE(M.is_enable_row_expo());

  fplll::ZZ_mat<mpz_t> A_expo(2, 2);
  A_expo.gen_identity(2);
  A_expo[0][0].mul_2si(A_expo[0][0], 40);
  fplll::ZZ_mat<mpz_t> U_expo, UT_expo;
  GSO M_expo(A_expo, U_expo, UT_expo, fplll::GSOOptions{.row_expo = true});
  M_expo.update_gso();
  CHECK(M_expo.is_enable_row_expo());
  CHECK(M_expo.get_row_expo(0) > 0);
  CHECK(M_expo.get_row_expo(0) > M_expo.get_row_expo(1));
}

TEST_CASE("The GSO object owns the MPFR precision", "[gso]")
{
  using ZT = fplll::Z_NR<mpz_t>;
  using FT = fplll::FP_NR<mpfr_t>;

  fplll::ZZ_mat<mpz_t> A(2, 2);
  A.gen_identity(2);
  fplll::ZZ_mat<mpz_t> U, UT;

  const unsigned int old_precision = FT::get_prec();
  {
    fplll::MatGSO<ZT, FT> outer(A, U, UT, fplll::GSOOptions{.precision = 128});
    CHECK(FT::get_prec() == 128);
    outer.update_gso();
    {
      fplll::MatGSO<ZT, FT> inner(A, U, UT, fplll::GSOOptions{.precision = 256});
      CHECK(FT::get_prec() == 256);
      inner.update_gso();
    }
    // The nested object restored the precision of the outer object.
    CHECK(FT::get_prec() == 128);
    {
      // precision = 0 inherits the precision that is currently in effect.
      fplll::MatGSO<ZT, FT> inherited(A, U, UT);
      CHECK(FT::get_prec() == 128);
    }
    CHECK(FT::get_prec() == 128);
    outer.update_gso();
    CHECK(outer.get_r_exp(1, 1).cmp(1.0) == 0);
  }
  CHECK(FT::get_prec() == old_precision);

  {
    // precision = 0 keeps the precision that is currently configured.
    fplll::MatGSO<ZT, FT> default_precision(A, U, UT);
    CHECK(FT::get_prec() == old_precision);
  }
  CHECK(FT::get_prec() == old_precision);

  {
    // The Householder variant follows the same contract.
    fplll::MatHouseholder<ZT, FT> householder(A, U, UT, fplll::HouseholderOptions{.precision = 96});
    CHECK(FT::get_prec() == 96);
  }
  CHECK(FT::get_prec() == old_precision);
}

// GSO-03: the integer Gram matrix is maintained by internal, noexcept row
// operations; the checked public accessors keep their error contract.
TEST_CASE("the integer Gram matrix stays behind the checked public accessors", "[gso]")
{
  using ZT = fplll::Z_NR<mpz_t>;
  using FT = fplll::FP_NR<double>;

  fplll::ZZ_mat<mpz_t> A(3, 3);
  A.gen_identity(3);
  fplll::ZZ_mat<mpz_t> U, UT;

  fplll::MatGSO<ZT, FT> int_gram(A, U, UT, fplll::GSOOptions{.int_gram = true});
  int_gram.update_gso();
  // symmetrize_g() uses the internal accessor and does not throw when the
  // integer Gram matrix is available.
  CHECK_NOTHROW(int_gram.symmetrize_g());
  CHECK(int_gram.get_g_matrix()(1, 0) == 0);

  // The row operations keep the integer Gram matrix in sync.
  {
    auto batch = int_gram.row_operations(0, 3);
    int_gram.row_add(1, 0);
  }
  int_gram.update_gso();
  CHECK(int_gram.get_g_matrix()(1, 0) == 1);

  // Without an integer Gram matrix the public entry points still reject the
  // query with fplll::Error instead of accessing a null pointer.
  fplll::MatGSO<ZT, FT> float_gram(A, U, UT);
  float_gram.update_gso();
  CHECK_THROWS_AS(float_gram.get_g_matrix(), fplll::Error);
  CHECK_THROWS_AS(float_gram.symmetrize_g(), fplll::Error);
}

// The precision guard is a process-wide stack rather than a single saved value:
// clients may hold several GSO objects with different precisions and destroy
// them in any order, which a per-guard saved value cannot handle.
TEST_CASE("GSO precision guards restore the default precision in any order", "[gso]")
{
  using ZT  = fplll::Z_NR<mpz_t>;
  using FT  = fplll::FP_NR<mpfr_t>;
  using GSO = fplll::MatGSO<ZT, FT>;

  fplll::ZZ_mat<mpz_t> A(2, 2);
  A.gen_identity(2);
  fplll::ZZ_mat<mpz_t> U, UT;

  // The MPFR default precision is process-wide state: start from a known value
  // and restore the previous one on every exit path.
  constexpr unsigned int baseline = 53;
  const ScopedDefaultPrecision restore_default_precision(baseline);
  REQUIRE(FT::get_prec() == baseline);

  // LIFO destruction: the inner guard restores the precision of the outer one.
  {
    auto outer = std::make_unique<GSO>(A, U, UT, fplll::GSOOptions{.precision = 128});
    CHECK(FT::get_prec() == 128);
    outer->update_gso();
    {
      auto inner = std::make_unique<GSO>(A, U, UT, fplll::GSOOptions{.precision = 256});
      CHECK(FT::get_prec() == 256);
      inner->update_gso();
    }
    CHECK(FT::get_prec() == 128);
  }
  CHECK(FT::get_prec() == baseline);

  // Out-of-order destruction: the outer guard dies first. It must not reset the
  // precision installed by the inner guard, and the surviving inner guard must
  // still restore the original precision when it dies.
  {
    auto outer = std::make_unique<GSO>(A, U, UT, fplll::GSOOptions{.precision = 128});
    auto inner = std::make_unique<GSO>(A, U, UT, fplll::GSOOptions{.precision = 256});
    CHECK(FT::get_prec() == 256);
    outer.reset();
    CHECK(FT::get_prec() == 256);
    inner.reset();
  }
  CHECK(FT::get_prec() == baseline);

  // Three nested guards destroyed from the middle outwards: the survivors keep
  // the precision they installed and the last guard restores the baseline.
  {
    auto outer  = std::make_unique<GSO>(A, U, UT, fplll::GSOOptions{.precision = 64});
    auto middle = std::make_unique<GSO>(A, U, UT, fplll::GSOOptions{.precision = 128});
    auto inner  = std::make_unique<GSO>(A, U, UT, fplll::GSOOptions{.precision = 256});
    CHECK(FT::get_prec() == 256);
    middle.reset();
    CHECK(FT::get_prec() == 256);
    outer.reset();
    CHECK(FT::get_prec() == 256);
    inner.reset();
  }
  CHECK(FT::get_prec() == baseline);

  // The fixed-precision backend has no default precision to install: the guard
  // stays the empty primary template (no storage) and the MPFR default is
  // untouched.
  static_assert(std::is_empty_v<fplll::detail::ScopedGsoPrecision<fplll::FP_NR<double>>>);
  {
    using FixedGSO = fplll::MatGSO<ZT, fplll::FP_NR<double>>;
    FixedGSO fixed(A, U, UT, fplll::GSOOptions{.precision = 256});
    CHECK(fplll::FP_NR<double>::get_prec() == fplll::PREC_DOUBLE);
    CHECK(FT::get_prec() == baseline);
    fixed.update_gso();
  }
  CHECK(fplll::FP_NR<double>::get_prec() == fplll::PREC_DOUBLE);
  CHECK(FT::get_prec() == baseline);
}

// The batch contract is enforced in debug builds with FPLLL_DEBUG_CHECK, which
// aborts: a missed row_operations() batch would otherwise let the cached Gram
// matrix and Gram-Schmidt coefficients go stale without any diagnostic.
#if defined(FPLLL_DEBUG_CHECKS) && !defined(_WIN32)
TEST_CASE("The row-operations batch contract is enforced in debug builds", "[gso]")
{
  using FT = double;

  SECTION("MatGSO::row_add")
  {
    require_sigabrt([] { gso_row_add_without_batch<FT>(); });
  }
  SECTION("MatGSO::row_sub")
  {
    require_sigabrt([] { gso_row_sub_without_batch<FT>(); });
  }
  SECTION("MatGSO::row_addmul")
  {
    require_sigabrt([] { gso_row_addmul_without_batch<FT>(); });
  }
  SECTION("MatGSO::row_addmul_we")
  {
    require_sigabrt([] { gso_row_addmul_we_without_batch<FT>(); });
  }
  SECTION("MatGSOGram::row_add")
  {
    require_sigabrt([] { gram_row_add_without_batch<FT>(); });
  }
  SECTION("MatGSO::row_add on a row outside the batch range")
  {
    require_sigabrt([] { gso_row_add_outside_batch_range<FT>(); });
  }
}
#else
TEST_CASE("The row-operations batch contract is enforced in debug builds", "[gso]")
{
  // Without FPLLL_DEBUG_CHECKS the checks expand to nothing, so a write outside
  // a batch is accepted (and the cached coefficients go stale).
  SUCCEED("FPLLL_DEBUG_CHECKS is off: a write outside a row_operations() batch is not checked");
}
#endif
