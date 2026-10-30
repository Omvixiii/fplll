// SPDX-FileCopyrightText: 2019 Martin Albrecht
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test_utils.h"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <fplll/enum/enumerate.h>
#include <fplll/enum/enumerate_base.h>
#include <fplll/enum/evaluator.h>
#include <fplll/fplll.h>
#include <fplll/gso/gso.h>
#include <fplll/gso/gso_interface.h>
#include <fplll/nr/matrix.h>
#include <fplll/nr/nr.h>

#include <algorithm>
#include <atomic>
#include <cfenv>
#include <cstddef>
#include <gmp.h>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{

// A failed reduction means the rest of the check is meaningless; the INFO
// stays local to this helper so it does not leak into later assertions.
void require_red_success(fplll::RedStatus status, const char *what)
{
  INFO(what << ": " << std::string(fplll::to_string(status)));
  REQUIRE(fplll::succeeded(status));
}

void require_red_success(const fplll::ReductionReport &report, const char *what)
{
  require_red_success(report.status, what);
}

// ENUM-03: the error-bounded evaluators abort the search by throwing when a
// numerical check fails.  The test only needs the exception to escape the
// sequential search unchanged.
class ThrowingEvaluator final : public fplll::Evaluator<fplll::FP_NR<double>>
{
public:
  void eval_sol(const std::vector<fplll::FP_NR<double>> &, const fplll::enumf &,
                fplll::enumf &) override
  {
    throw std::runtime_error("forced evaluator failure");
  }

  void eval_sub_sol(int, const std::vector<fplll::FP_NR<double>> &, const fplll::enumf &) override
  {
  }
};

// E-1: records the rounding mode the search runs under on every thread that
// reports a solution.  The parallel sink serializes the calls, but the counters
// are atomic so the check does not depend on that.
struct RoundingRecorder
{
  void record() noexcept
  {
    ++observations;
    if (std::fegetround() != FE_TONEAREST)
    {
      ++non_nearest;
    }
  }

  std::atomic<int> observations{0};
  std::atomic<int> non_nearest{0};
};

class RoundingRecordingEvaluator final : public fplll::Evaluator<fplll::FP_NR<double>>
{
public:
  explicit RoundingRecordingEvaluator(RoundingRecorder &recorder) : recorder_(recorder) {}

  void eval_sol(const std::vector<fplll::FP_NR<double>> &, const fplll::enumf &,
                fplll::enumf &) override
  {
    recorder_.record();
  }

  void eval_sub_sol(int, const std::vector<fplll::FP_NR<double>> &, const fplll::enumf &) override
  {
  }

private:
  RoundingRecorder &recorder_;
};

// Records the mode of the throwing thread and then aborts the search, which is
// how a numerical evaluator rejects a solution.
class RoundingThrowingEvaluator final : public fplll::Evaluator<fplll::FP_NR<double>>
{
public:
  explicit RoundingThrowingEvaluator(RoundingRecorder &recorder) : recorder_(recorder) {}

  void eval_sol(const std::vector<fplll::FP_NR<double>> &, const fplll::enumf &,
                fplll::enumf &) override
  {
    recorder_.record();
    throw std::runtime_error("forced parallel evaluator failure");
  }

  void eval_sub_sol(int, const std::vector<fplll::FP_NR<double>> &, const fplll::enumf &) override
  {
  }

private:
  RoundingRecorder &recorder_;
};

// E-1: builds a fresh worker pool while the calling thread rounds upwards, so
// the worker threads inherit FE_UPWARD.  Only the per-task guard can then make
// them round-to-nearest during the search.  The thread count is restored by the
// caller through ResetThreadCount.
struct ResetThreadCount
{
  ~ResetThreadCount() { fplll::set_threads(1); }
};

int make_pool_under_upward_rounding()
{
  REQUIRE(fplll::set_threads(1) == 1);
  REQUIRE(std::fesetround(FE_UPWARD) == 0);
  const int threads =
      std::min(4, std::max(1, static_cast<int>(std::thread::hardware_concurrency())));
  REQUIRE(fplll::set_threads(threads) == threads);
  return threads;
}

// Enumerate the first d coordinates of a reduced q-ary lattice: at least one
// vector must be found.
template <class FT> void test_enum(std::size_t d)
{
  fplll::RandGen::init_with_seed(0x1337);
  fplll::ZZ_mat<mpz_t> A(100, 100);
  fplll::Z_NR<mpz_t> q;
  q = 7681;
  A.gen_qary(50, q);
  require_red_success(fplll::lll_reduction(A), "LLL reduction failed");

  fplll::ZZ_mat<mpz_t> U;
  fplll::MatGSO<fplll::Z_NR<mpz_t>, fplll::FP_NR<FT>> M(A, U, U);
  M.update_gso();

  fplll::FastEvaluator<fplll::FP_NR<FT>> evaluator;
  fplll::Enumeration<fplll::Z_NR<mpz_t>, fplll::FP_NR<FT>> enum_obj(M, evaluator);
  fplll::FP_NR<FT> max_dist;
  M.get_r(max_dist, 0, 0);
  max_dist *= 0.99;
  enum_obj.enumerate(0, static_cast<int>(d), max_dist, 0);
  CHECK(!evaluator.empty());
}

// The parameter type is dictated by the public callback_evaluator_callback typedef.
// NOLINTNEXTLINE(readability-non-const-parameter)  // signature fixed by the public typedef
bool callback_firstf(std::size_t, fplll::enumf *new_sol_coord, void *ctx)
{
  return new_sol_coord[0] == static_cast<double *>(ctx)[0];
}

// As above, but with a callback evaluator: the callback accepts exactly the
// vectors whose first coordinate equals the context value 2.
template <class FT> void test_callback_enum(std::size_t d)
{
  fplll::RandGen::init_with_seed(0x1337);
  fplll::ZZ_mat<mpz_t> A(100, 100);
  fplll::Z_NR<mpz_t> q;
  q = 7681;
  A.gen_qary(50, q);
  require_red_success(fplll::lll_reduction(A), "LLL reduction failed");

  fplll::ZZ_mat<mpz_t> U;
  fplll::MatGSO<fplll::Z_NR<mpz_t>, fplll::FP_NR<FT>> M(A, U, U);
  M.update_gso();

  fplll::enumf ctx = 2;
  fplll::CallbackEvaluator<fplll::FP_NR<FT>> evaluator(callback_firstf, &ctx);
  fplll::Enumeration<fplll::Z_NR<mpz_t>, fplll::FP_NR<FT>> enum_obj(M, evaluator);
  fplll::FP_NR<FT> max_dist;
  M.get_r(max_dist, 0, 0);
  max_dist *= 0.99;
  enum_obj.enumerate(0, static_cast<int>(d), max_dist, 0);
  REQUIRE(!evaluator.empty());
  CHECK(evaluator.begin()->second[0].get_si() == 2);
}

// Count the vectors found by list-CVP around the origin and close to it, then
// check the number of solutions for 24 targets in the half lattice.  The
// half-lattice targets draw from the C++ random stream seeded with 0x1337 by
// the preceding test cases, exactly as the historical main() did; the Meson
// registration's --order decl preserves that order.
template <class FT> void test_list_cvp()
{
  fplll::ZZ_mat<mpz_t> A;
  REQUIRE(read_file(A, TESTDATADIR "tests/lattices/example_list_cvp_in_lattice") == 0);

  require_red_success(fplll::lll_reduction(A), "LLL reduction failed");

  // Search for up to 999999 vectors, up to radius 32.5 around the origin
  // the right answer is 196561
  fplll::FP_NR<FT> rad            = 32.5;
  fplll::FP_NR<FT> half_rad       = 16.5;
  const unsigned int right_answer = 196561;

  // Tests with two targets: 0, and something very close to 0.

  const int d = A.get_rows();
  REQUIRE(d == 24);
  const auto dim = static_cast<std::size_t>(d);

  fplll::ZZ_mat<mpz_t> empty_mat;
  fplll::MatGSO<fplll::Z_NR<mpz_t>, fplll::FP_NR<FT>> gso(A, empty_mat, empty_mat,
                                                          fplll::GSOOptions{.int_gram = true});

  {
    gso.update_gso();
    fplll::FastEvaluator<fplll::FP_NR<FT>> evaluator(999999);
    fplll::Enumeration<fplll::Z_NR<mpz_t>, fplll::FP_NR<FT>> enum_obj(gso, evaluator);

    std::vector<fplll::FP_NR<FT>> target(dim, 0.0);
    enum_obj.enumerate(0, d, rad, 0, target);
    CHECK(evaluator.size() == right_answer);
  }

  {
    gso.update_gso();
    fplll::FastEvaluator<fplll::FP_NR<FT>> evaluator(999999);
    fplll::Enumeration<fplll::Z_NR<mpz_t>, fplll::FP_NR<FT>> enum_obj(gso, evaluator);

    std::vector<fplll::FP_NR<FT>> target(dim, 0.0001);
    enum_obj.enumerate(0, d, rad, 0, target);
    CHECK(evaluator.size() == right_answer);
  }

  for (std::size_t rep = 0; rep < 24; ++rep)
  {
    fplll::FastEvaluator<fplll::FP_NR<FT>> evaluator(999999);
    fplll::Enumeration<fplll::Z_NR<mpz_t>, fplll::FP_NR<FT>> enum_obj(gso, evaluator);

    std::vector<fplll::FP_NR<FT>> can_target(dim, 0.0);

    // Generate a point in the half lattice
    for (std::size_t i = 0; i < dim; ++i)
    {
      // Make sure at least one of the coefficients is non-zero.
      std::size_t c = (i == rep || fplll::RandGenInt::get_bit() > 0);
      if (!c)
      {
        continue;
      }
      for (std::size_t j = 0; j < dim; ++j)
      {
        can_target[j] += 0.5 * A[static_cast<int>(i)][static_cast<int>(j)].get_d();
      }
    }

    std::vector<fplll::FP_NR<FT>> target(dim, 0.0);
    // Convert it to GSO basis
    for (std::size_t i = 0; i < dim; ++i)
    {
      for (std::size_t j = 0; j < dim; ++j)
      {
        target[i] += A[static_cast<int>(i)][static_cast<int>(j)].get_d() * can_target[j];
      }

      for (std::size_t j = 0; j < i; ++j)
      {
        fplll::FP_NR<FT> mu_ij;
        gso.get_mu(mu_ij, static_cast<int>(i), static_cast<int>(j));
        target[i] -= target[j] * mu_ij;
      }
    }
    for (std::size_t i = 0; i < dim; ++i)
    {
      fplll::FP_NR<FT> r_ii;
      gso.get_r(r_ii, static_cast<int>(i), static_cast<int>(i));
      target[i] /= r_ii;
    }

    enum_obj.enumerate(0, d, half_rad, 0, target);

    CAPTURE(rep, evaluator.size());
    CHECK((evaluator.size() == 2 || evaluator.size() == 48));
  }
}

}  // namespace

TEST_CASE("enumeration on a q-ary lattice finds a vector", "[enum]") { test_enum<double>(30); }

TEST_CASE("callback enumeration records the requested coordinate", "[enum]")
{
  test_callback_enum<double>(40);
}

TEST_CASE("list CVP counts the expected number of solutions", "[enum]") { test_list_cvp<double>(); }

TEST_CASE("the rounding-mode guard restores the caller's mode on every exit", "[enum]")
{
  const int caller_rounding = std::fegetround();
  REQUIRE(std::fesetround(FE_UPWARD) == 0);
  {
    const fplll::ScopedRoundingMode rounding;
    INFO("the guard did not install round-to-nearest");
    CHECK(std::fegetround() == FE_TONEAREST);
  }
  INFO("the guard did not restore the rounding mode on normal exit");
  CHECK(std::fegetround() == FE_UPWARD);

  try
  {
    const fplll::ScopedRoundingMode rounding;
    throw std::runtime_error("force unwind");
  }
  catch (const std::runtime_error &error)
  {
    CHECK(std::string(error.what()) == "force unwind");
  }
  INFO("the guard did not restore the rounding mode during unwinding");
  CHECK(std::fegetround() == FE_UPWARD);
  REQUIRE(std::fesetround(caller_rounding) == 0);
}

TEST_CASE("enumeration restores the rounding mode when the evaluator throws", "[enum]")
{
  // The 3x3 identity basis is LLL-reduced, and a radius of 1.1 > 1 reaches the
  // candidate +/-e_0, so the evaluator is called before the search can end.
  fplll::ZZ_mat<mpz_t> A(3, 3);
  A.gen_identity(3);
  fplll::ZZ_mat<mpz_t> U;
  fplll::ZZ_mat<mpz_t> UT;
  fplll::MatGSO<fplll::Z_NR<mpz_t>, fplll::FP_NR<double>> gso(A, U, UT);
  gso.update_gso();

  ThrowingEvaluator evaluator;
  fplll::EnumerationDyn<fplll::Z_NR<mpz_t>, fplll::FP_NR<double>> enumeration(gso, evaluator);
  fplll::FP_NR<double> max_dist;
  gso.get_r(max_dist, 0, 0);
  max_dist *= 1.1;

  const int caller_rounding = std::fegetround();
  REQUIRE(std::fesetround(FE_DOWNWARD) == 0);
  // run_sequential() must restore the rounding mode while the exception is in
  // flight, not only on the normal return path.
  CHECK_THROWS_AS(enumeration.enumerate(0, 3, max_dist, 0), std::runtime_error);
  INFO("the caller's rounding mode leaked after the evaluator threw");
  CHECK(std::fegetround() == FE_DOWNWARD);
  REQUIRE(std::fesetround(caller_rounding) == 0);
}

#if FPLLL_MAX_PARALLEL_ENUM_DIM >= 20
// E-1: the built-in parallel enumerator is the default SVP path for dimensions
// 11..FPLLL_MAX_PARALLEL_ENUM_DIM.  The whole search must run under
// round-to-nearest on the calling thread and on every worker, and the caller's
// mode must be restored afterwards.
TEST_CASE("parallel enumeration installs round-to-nearest on every thread", "[enum]")
{
  ResetThreadCount reset_threads;
  const int caller_rounding = std::fegetround();
  const int threads         = make_pool_under_upward_rounding();
  CAPTURE(threads);

  // A 20x20 identity basis is LLL-reduced; a radius of 2 reaches the solutions
  // +/-e_i, so the evaluator is called during the parallel search.
  fplll::ZZ_mat<mpz_t> A(20, 20);
  A.gen_identity(20);
  fplll::ZZ_mat<mpz_t> U;
  fplll::ZZ_mat<mpz_t> U_inv;
  fplll::MatGSO<fplll::Z_NR<mpz_t>, fplll::FP_NR<double>> gso(A, U, U_inv);
  gso.update_gso();

  RoundingRecorder recorder;
  RoundingRecordingEvaluator evaluator(recorder);
  fplll::Enumeration<fplll::Z_NR<mpz_t>, fplll::FP_NR<double>> enum_obj(gso, evaluator);
  fplll::FP_NR<double> max_dist = 2.0;
  enum_obj.enumerate(0, 20, max_dist, 0);

  INFO("the parallel search ran outside round-to-nearest on at least one thread");
  CHECK(recorder.observations.load() > 0);
  CHECK(recorder.non_nearest.load() == 0);

  INFO("the caller's rounding mode was not restored after the search");
  CHECK(std::fegetround() == FE_UPWARD);
  REQUIRE(std::fesetround(caller_rounding) == 0);
}

// E-1: an evaluator that throws must not leave the caller (or a worker) with a
// changed rounding mode, and the exception must keep propagating.
TEST_CASE("parallel enumeration restores the rounding mode when the evaluator throws", "[enum]")
{
  ResetThreadCount reset_threads;
  const int caller_rounding = std::fegetround();
  const int threads         = make_pool_under_upward_rounding();
  CAPTURE(threads);

  fplll::ZZ_mat<mpz_t> A(20, 20);
  A.gen_identity(20);
  fplll::ZZ_mat<mpz_t> U;
  fplll::ZZ_mat<mpz_t> U_inv;
  fplll::MatGSO<fplll::Z_NR<mpz_t>, fplll::FP_NR<double>> gso(A, U, U_inv);
  gso.update_gso();

  RoundingRecorder recorder;
  RoundingThrowingEvaluator evaluator(recorder);
  fplll::Enumeration<fplll::Z_NR<mpz_t>, fplll::FP_NR<double>> enum_obj(gso, evaluator);
  fplll::FP_NR<double> max_dist = 2.0;

  CHECK_THROWS_AS(enum_obj.enumerate(0, 20, max_dist, 0), std::runtime_error);

  INFO("the parallel search did not run under round-to-nearest before throwing");
  CHECK(recorder.observations.load() > 0);
  CHECK(recorder.non_nearest.load() == 0);

  INFO("the caller's rounding mode leaked after the evaluator threw");
  CHECK(std::fegetround() == FE_UPWARD);
  REQUIRE(std::fesetround(caller_rounding) == 0);
}
#endif

// E-2: max_indices is the reset schedule of the exact CVP search (it is only
// produced by the CVPM_PROVED path, which is non-dual).  A dual enumeration has
// no use for it; the historical 6-branch dispatcher silently ran no search at
// all for this combination.  The public constructor can still express it, so it
// must fail explicitly instead of returning an empty enumeration.
TEST_CASE("max_indices are rejected for dual enumeration", "[enum]")
{
  fplll::ZZ_mat<mpz_t> A(3, 3);
  A.gen_identity(3);
  fplll::ZZ_mat<mpz_t> U;
  fplll::ZZ_mat<mpz_t> U_inv;
  fplll::MatGSO<fplll::Z_NR<mpz_t>, fplll::FP_NR<double>> gso(A, U, U_inv);
  gso.update_gso();

  fplll::FastEvaluator<fplll::FP_NR<double>> evaluator;
  const std::vector<int> max_indices{2, 2, 2};
  fplll::Enumeration<fplll::Z_NR<mpz_t>, fplll::FP_NR<double>> enum_obj(gso, evaluator,
                                                                        max_indices);
  fplll::FP_NR<double> max_dist = 2.0;
  const std::vector<fplll::FP_NR<double>> target;
  const std::vector<fplll::enumf> pruning;

  CHECK_THROWS_AS(enum_obj.enumerate(0, 3, max_dist, 0, target, pruning, /*dual=*/true),
                  fplll::Error);
  CHECK(evaluator.empty());
}
