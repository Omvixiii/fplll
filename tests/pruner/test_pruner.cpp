// SPDX-FileCopyrightText: 2016 Leo Ducas
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <fplll/fplll.h>
#include <fplll/gso/gso.h>
#include <fplll/nr/matrix.h>
#include <fplll/nr/nr.h>
#include <gmp.h>
#include <mpfr.h>

#include "../../src/reduction/lll/lll_kernel.h"

#ifdef FPLLL_WITH_QD
#include <qd/dd_real.h>
#include <qd/qd_real.h>
#endif

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace
{
constexpr int N = 56;
#ifdef FPLLL_WITH_QD
constexpr int Nbis = 24;
#endif
}  // namespace

// White-box access to the private helpers of Pruner<FT> (enabled by
// -DFPLLL_TESTING, which the test build defines).
namespace fplll
{

template <class FT> class Pruner<FT>::TestPruner
{
public:
  int n;
  int d;
  fplll::Pruner<FT> pru;

  TestPruner(int dimension) : n(dimension), pru(dimension) { d = n; }

  /** Reference access to the lazily filled tabulated constants. */
  const std::array<FT, fplll::max_pruner_dim> &tabulated_factorial() const
  {
    return Pruner<FT>::tabulated_factorial;
  }

  const std::array<FT, fplll::max_pruner_dim> &tabulated_ball_vol() const
  {
    return Pruner<FT>::tabulated_ball_vol;
  }

  void test_enforce()
  {
    std::vector<double> rs;
    rs.reserve(n);
    for (int i = 0; i < n; ++i)
    {
      rs.emplace_back(1.);
    }
    pru.load_basis_shape(rs);
    typename fplll::Pruner<FT>::evec b(n);
    for (int i = 0; i < d; ++i)
    {
      b[i] = .3 + (1. * ((i * i * i) % d)) / (2 * d);
    }
    int j = d / 2;
    b[j]  = .5;

    FT old_bj = b[j];
    pru.min_pruning_coefficients.resize(d);
    std::fill(pru.min_pruning_coefficients.begin(), pru.min_pruning_coefficients.end(), .1);
    pru.enforce(b, j);

    INFO("the enforced bounds keep b[j] unchanged");
    CHECK(b[j] == old_bj);
    INFO("the enforced bounds terminate with 1");
    CHECK(b[d - 1] == 1.);
    INFO("the enforced bounds are increasing");
    for (int i = 0; i < d - 1; ++i)
    {
      CAPTURE(i);
      CHECK(b[i + 1] >= b[i]);
    }
  }

  void test_eval_poly()
  {
    typename fplll::Pruner<FT>::poly p(4);
    FT x, y;
    x      = .3;
    int ld = 3;
    p[0]   = 1;
    p[1]   = 1;
    p[2]   = 2;
    p[3]   = 3;

    y = pru.eval_poly(ld, p, x);
    INFO("eval_poly(3, p, .3) = " << y);
    CHECK(fplll::abs(y - 1.561) <= 1e-10);
  }

  void test_integrate_poly()
  {
    typename fplll::Pruner<FT>::poly p(5);
    int ld = 3;
    p[0]   = 1.;
    p[1]   = 1.;
    p[2]   = 2.;
    p[3]   = 3.;
    pru.integrate_poly(ld, p);
    CHECK(fplll::abs(p[0] - 0.0) <= 1e-10);
    CHECK(fplll::abs(p[1] - 1.0) <= 1e-10);
    CHECK(fplll::abs(p[2] - 1. / 2) <= 1e-10);
    CHECK(fplll::abs(p[3] - 2. / 3) <= 1e-10);
    CHECK(fplll::abs(p[4] - 3. / 4) <= 1e-10);
  }

  // To test relative volumes, we use the explicit formula
  // for step function given in [GNR], I_alpha(n/4,n/4)
  // computed using the following sage script
  //
  // alpha = XXX
  // n = 24
  // a = n/4
  // b = n/4
  // beta_dist = x**(a-1) * (1 - x)**(b-1)
  // c = integral(beta_dist, x, 0, alpha)
  // f = integral(beta_dist, x, 0, 1.)
  // print c/f

  // XXX = .3 ---> 0.07822479096
  // XXX = .5 ---> 0.5
  // XXX = .7 ---> 0.921775209040006

  void test_relative_volume()
  {
    std::vector<double> pr(n);

    for (int i = 0; i < n / 2; ++i)
    {
      pr[i]           = 1;
      pr[i + (n / 2)] = .3;
    }

    double proba = fplll::svp_probability<fplll::FP_NR<double>>(pr).get_d();
    double error = std::abs(1 - proba / 0.07822479096);
    INFO("relative volume for alpha = .3");
    CAPTURE(proba, error);
    CHECK(error <= .05);

    for (int i = 0; i < n / 2; ++i)
    {
      pr[i]           = 1;
      pr[i + (n / 2)] = .5;
    }

    proba = fplll::svp_probability<fplll::FP_NR<double>>(pr).get_d();
    error = std::abs(1 - proba / 0.5);
    INFO("relative volume for alpha = .5");
    CAPTURE(proba, error);
    CHECK(error <= .05);

    for (int i = 0; i < n / 2; ++i)
    {
      pr[i]           = 1;
      pr[i + (n / 2)] = .7;
    }

    proba = fplll::svp_probability<fplll::FP_NR<double>>(pr).get_d();
    error = std::abs(1 - proba / 0.92177520904);
    INFO("relative volume for alpha = .7");
    CAPTURE(proba, error);
    CHECK(error <= .05);
  }
};

}  // namespace fplll

namespace
{

void set_up_gso_norms(std::vector<double> &gso_sq_norms)
{
  for (int i = 0; i < N; ++i)
  {
    // A bit more than LLL reduced
    gso_sq_norms.emplace_back(std::pow(1.06, -i));
  }
}

template <class FT> void test_prepruned()
{
  INFO("pre-pruned pruner");
  std::vector<double> gso_sq_norms;
  set_up_gso_norms(gso_sq_norms);

  std::vector<double> pr = {1,        1,        1,        1,        1,        1,        1,
                            1,        1,        1,        1,        1,        1,        1,
                            0.937197, 0.937197, 0.871731, 0.871731, 0.814304, 0.814304, 0.762232,
                            0.762232, 0.713898, 0.713898, 0.668279, 0.668279, 0.624701, 0.624701,
                            0.58271,  0.58271,  0.541994, 0.541994, 0.502342, 0.502342, 0.463617,
                            0.463617, 0.425747, 0.425747, 0.388723, 0.388723, 0.35262,  0.35262,
                            0.317642, 0.317642, 0.284261, 0.284261, 0.254584, 0.254584, 0.254584,
                            0.254584, 0.254584, 0.254584, 0.2,      0.2,      0.2,      0.2};

  fplll::Pruner<FT> pru(.85, 20000., gso_sq_norms);
  const double cost = pru.single_enum_cost(pr);
  CAPTURE(cost);
  CHECK_FALSE(std::isnan(cost));
  CHECK(std::abs(1 - cost / 2.01206e+07) <= .01);
  const double proba = pru.measure_metric(pr);
  CAPTURE(proba);
  CHECK(std::abs(1 - proba / .556) <= .01);
  CHECK_FALSE(std::isnan(proba));
}

template <class FT> void test_unpruned()
{
  INFO("un-pruned pruner");
  std::vector<double> gso_sq_norms;
  set_up_gso_norms(gso_sq_norms);

  std::vector<double> pr;
  pr.reserve(N);
  for (int i = 0; i < N; ++i)
  {
    pr.emplace_back(1.);
  }
  fplll::Pruner<FT> pru(.85, 20000., gso_sq_norms);

  {
    const double cost = pru.single_enum_cost(pr);
    CAPTURE(cost);
    CHECK(std::abs(1 - cost / 3.20e+10) <= .02);
    CHECK_FALSE(std::isnan(cost));
    const double proba = pru.measure_metric(pr);
    CAPTURE(proba);
    CHECK(std::abs(1 - proba) <= .02);
    CHECK_FALSE(std::isnan(proba));
  }

  std::vector<std::vector<double>> gso_sq_norms_vec;
  std::vector<double> v1, v2, v3;
  v1 = gso_sq_norms;
  v2 = gso_sq_norms;
  v3 = gso_sq_norms;

  gso_sq_norms_vec.emplace_back(v1);
  gso_sq_norms_vec.emplace_back(v2);
  for (int i = 0; i < N; ++i)
  {
    v3[i] *= 20;
  }
  // 1 basis out of 3 is so large that it essentially induces no cost: new cost should be 2/3 of
  // the previous
  gso_sq_norms_vec.emplace_back(v3);

  INFO("three bases, one of them large enough to induce no cost");
  fplll::Pruner<FT> pru2(.85, 0., gso_sq_norms_vec);
  {
    const double cost = pru2.single_enum_cost(pr);
    CAPTURE(cost);
    CHECK(std::abs(1 - 3. / 2. * cost / 3.20e+10) <= .02);
    const double proba = pru2.measure_metric(pr);
    CAPTURE(proba);
    CHECK(std::abs(1 - proba) <= .02);
  }
}

template <class FT> void test_auto_prune(std::size_t n)
{
  INFO("auto-prune with n = " << n);
  fplll::ZZ_mat<mpz_t> A(static_cast<int>(2 * n), static_cast<int>(2 * n));
  A.gen_qary_bits(static_cast<int>(n), 30);
  fplll::ZZ_mat<mpz_t> U;
  fplll::MatGSO<fplll::Z_NR<mpz_t>, fplll::FP_NR<double>> M(A, U, U);
  fplll::LLLReduction<fplll::Z_NR<mpz_t>, fplll::FP_NR<double>> lll_obj(M);
  lll_obj.lll();
  std::vector<double> r;
  for (std::size_t i = 0; i < 2 * n; ++i)
  {
    fplll::FP_NR<double> x;
    M.get_r(x, static_cast<int>(i), static_cast<int>(i));
    r.push_back(x.get_d());
  }

  fplll::PruningParams pruning;
  double overhead = 1.0e6 * static_cast<double>(n) * static_cast<double>(n);
  double radius_d = r[0] * .3;

  INFO("default optimisation");
  fplll::prune<FT>(pruning, radius_d, overhead, r, 20, fplll::PrunerMetric::expected_solutions);
  CHECK(pruning.expectation > 0.0);
  CHECK(pruning.gh_factor >= .05);
  CHECK(pruning.gh_factor < 20.);
  CHECK(pruning.coefficients[0] == 1.0);

  INFO("gradient descent");
  fplll::prune<FT>(pruning, radius_d, overhead, r, 0.3,
                   fplll::PrunerMetric::probability_of_shortest,
                   fplll::PrunerOptions{.strategy = fplll::PrunerStrategy::gradient});
  CHECK(pruning.expectation <= 1.001);
  CHECK(pruning.expectation > 0.0);
  CHECK(pruning.gh_factor >= .05);
  CHECK(pruning.gh_factor < 20.);
  CHECK(pruning.coefficients[0] == 1.0);

  INFO("gradient descent starting from the input");
  fplll::prune<FT>(
      pruning, radius_d, overhead, r, 0.01, fplll::PrunerMetric::probability_of_shortest,
      fplll::PrunerOptions{.strategy = fplll::PrunerStrategy::gradient, .start_from_input = true});
  CHECK(pruning.expectation <= 1.001);
  CHECK(pruning.expectation > 0.0);
  CHECK(pruning.gh_factor >= .05);
  CHECK(pruning.gh_factor < 20.);
  CHECK(pruning.coefficients[0] == 1.0);

  INFO("Nelder-Mead");
  fplll::prune<FT>(pruning, radius_d, overhead, r, 0.3,
                   fplll::PrunerMetric::probability_of_shortest,
                   fplll::PrunerOptions{.strategy = fplll::PrunerStrategy::nelder_mead});
  CHECK(pruning.expectation <= 1.001);
  CHECK(pruning.expectation > 0.0);
  CHECK(pruning.gh_factor >= .05);
  CHECK(pruning.gh_factor < 20.);
  CHECK(pruning.coefficients[0] == 1.0);

  INFO("Nelder-Mead starting from the input");
  fplll::prune<FT>(pruning, radius_d, overhead, r, 0.3,
                   fplll::PrunerMetric::probability_of_shortest,
                   fplll::PrunerOptions{.strategy         = fplll::PrunerStrategy::nelder_mead,
                                        .start_from_input = true});
  CHECK(pruning.expectation <= 1.001);
  CHECK(pruning.expectation > 0.0);
  CHECK(pruning.gh_factor >= .05);
  CHECK(pruning.gh_factor < 20.);
  CHECK(pruning.coefficients[0] == 1.0);
}

}  // namespace

#ifdef FPLLL_WITH_QD
TEST_CASE("unpruned pruner costs match the reference for dd arithmetic", "[pruner]")
{
  test_unpruned<fplll::FP_NR<dd_real>>();
}
#endif

TEST_CASE("unpruned pruner costs match the reference for double arithmetic", "[pruner]")
{
  test_unpruned<fplll::FP_NR<double>>();
}

TEST_CASE("unpruned pruner costs match the reference for MPFR arithmetic", "[pruner]")
{
  test_unpruned<fplll::FP_NR<mpfr_t>>();
}

TEST_CASE("prepruned pruner costs match the reference for double arithmetic", "[pruner]")
{
  test_prepruned<fplll::FP_NR<double>>();
}

TEST_CASE("prepruned pruner costs match the reference for MPFR arithmetic", "[pruner]")
{
  test_prepruned<fplll::FP_NR<mpfr_t>>();
}

#ifdef FPLLL_WITH_QD
TEST_CASE("pruner keeps the halfway coefficient when enforcing bounds for dd arithmetic",
          "[pruner]")
{
  fplll::Pruner<fplll::FP_NR<dd_real>>::TestPruner tested(Nbis);
  tested.test_enforce();
}

TEST_CASE("pruner evaluates polynomials for dd arithmetic", "[pruner]")
{
  fplll::Pruner<fplll::FP_NR<dd_real>>::TestPruner tested(Nbis);
  tested.test_eval_poly();
}

TEST_CASE("pruner integrates polynomials for dd arithmetic", "[pruner]")
{
  fplll::Pruner<fplll::FP_NR<dd_real>>::TestPruner tested(Nbis);
  tested.test_integrate_poly();
}

TEST_CASE("pruner computes relative volumes for dd arithmetic", "[pruner]")
{
  fplll::Pruner<fplll::FP_NR<dd_real>>::TestPruner tested(Nbis);
  tested.test_relative_volume();
}
#endif

// The generated q-ary lattices come from the global GMP random stream, so the
// auto-prune runs stay in one test case in their original order.
TEST_CASE("auto-prune finds pruning parameters within the reference bounds", "[pruner]")
{
#ifdef FPLLL_WITH_QD
  test_auto_prune<fplll::FP_NR<dd_real>>(20);
#endif
  test_auto_prune<fplll::FP_NR<double>>(20);
  test_auto_prune<fplll::FP_NR<double>>(30);
}

// ---------------------------------------------------------------------------
// Tabulated constants (audit batch B1, PRU-01/02).
//
// The tables used to be filled at the first Pruner construction with GMP/MPFR:
// exact factorials converted to decimal strings and parsed by MPFR, plus a
// 256-bit MPFR ball-volume loop.  The double tables are compile-time constants
// now and the mpfr factorial table is filled with mpz -> mpfr_t directly, so
// pin the values against an independent recomputation of the historical path.
// ---------------------------------------------------------------------------
namespace
{

/** The historical double factorial fill: exact integer -> decimal string ->
    MPFR at 53 bits -> double. */
std::array<double, fplll::max_pruner_dim> reference_factorial_double()
{
  std::array<double, fplll::max_pruner_dim> table{};
  mpz_t factorial;
  mpz_init_set_ui(factorial, 1);
  for (std::size_t i = 0; i < fplll::max_pruner_dim; ++i)
  {
    if (i > 0)
    {
      mpz_mul_ui(factorial, factorial, static_cast<unsigned long>(i));
    }
    std::string decimal(mpz_sizeinbase(factorial, 10) + 2, '\0');
    mpz_get_str(decimal.data(), 10, factorial);
    mpfr_t parsed;
    mpfr_init2(parsed, fplll::PREC_DOUBLE);
    REQUIRE(mpfr_set_str(parsed, decimal.c_str(), 10, MPFR_RNDN) == 0);
    table[i] = mpfr_get_d(parsed, MPFR_RNDN);
    mpfr_clear(parsed);
  }
  mpz_clear(factorial);
  return table;
}

/** The ball-volume fill that the mpfr/dd/qd backends still use: 256-bit
    pi^(i/2) / Gamma(i/2 + 1), rounded to double. */
std::array<double, fplll::max_pruner_dim> reference_ball_volumes()
{
  std::array<double, fplll::max_pruner_dim> table{};
  mpfr_t pi, value, gamma, tmp;
  mpfr_inits2(256, pi, value, gamma, tmp, static_cast<mpfr_ptr>(nullptr));
  mpfr_const_pi(pi, MPFR_RNDN);
  for (std::size_t i = 0; i < fplll::max_pruner_dim; ++i)
  {
    mpfr_set_ui(tmp, static_cast<unsigned long>(i), MPFR_RNDN);
    mpfr_div_2ui(tmp, tmp, 1, MPFR_RNDN);
    mpfr_pow(value, pi, tmp, MPFR_RNDN);
    mpfr_add_ui(tmp, tmp, 1, MPFR_RNDN);
    mpfr_gamma(gamma, tmp, MPFR_RNDN);
    mpfr_div(value, value, gamma, MPFR_RNDN);

    table[i] = mpfr_get_d(value, MPFR_RNDN);
  }
  mpfr_clears(pi, value, gamma, tmp, static_cast<mpfr_ptr>(nullptr));
  mpfr_free_cache();
  return table;
}

/** Visits every exact factorial in the order the historical fill built them. */
template <class Body> void for_each_exact_factorial(Body &&body)
{
  mpz_t factorial;
  mpz_init_set_ui(factorial, 1);
  for (std::size_t i = 0; i < fplll::max_pruner_dim; ++i)
  {
    if (i > 0)
    {
      mpz_mul_ui(factorial, factorial, static_cast<unsigned long>(i));
    }
    body(i, factorial);
  }
  mpz_clear(factorial);
}

}  // namespace

TEST_CASE("pruner tabulated constants are bit-exact", "[pruner]")
{
  using fplll::FP_NR;

  SECTION("double factorial and ball volume")
  {
    fplll::Pruner<FP_NR<double>>::TestPruner probe(4);
    const auto &factorial_table = probe.tabulated_factorial();
    const auto &ball_vol_table  = probe.tabulated_ball_vol();

    const auto reference_factorial = reference_factorial_double();
    const auto reference_ball_vol  = reference_ball_volumes();

    std::size_t factorial_mismatch = 0;
    std::size_t ball_vol_mismatch  = 0;
    for (std::size_t i = 0; i < fplll::max_pruner_dim; ++i)
    {
      if (std::bit_cast<std::uint64_t>(factorial_table[i].get_d()) !=
          std::bit_cast<std::uint64_t>(reference_factorial[i]))
      {
        ++factorial_mismatch;
      }
      if (std::bit_cast<std::uint64_t>(ball_vol_table[i].get_d()) !=
          std::bit_cast<std::uint64_t>(reference_ball_vol[i]))
      {
        ++ball_vol_mismatch;
      }
    }
    CAPTURE(fplll::max_pruner_dim, factorial_mismatch, ball_vol_mismatch);
    CHECK(factorial_mismatch == 0);
    CHECK(ball_vol_mismatch == 0);
  }

  SECTION("mpfr factorial")
  {
    fplll::Pruner<FP_NR<mpfr_t>>::TestPruner probe(4);
    const auto &factorial_table = probe.tabulated_factorial();

    std::size_t mismatch = 0;
    for_each_exact_factorial(
        [&](std::size_t i, const mpz_t factorial)
        {
          std::string decimal(mpz_sizeinbase(factorial, 10) + 2, '\0');
          mpz_get_str(decimal.data(), 10, factorial);
          mpfr_t reference;
          mpfr_init2(reference, fplll::PREC_DOUBLE);
          REQUIRE(mpfr_set_str(reference, decimal.c_str(), 10, MPFR_RNDN) == 0);
          if (mpfr_equal_p(factorial_table[i].get_data(), reference) == 0)
          {
            ++mismatch;
          }
          mpfr_clear(reference);
        });
    CAPTURE(fplll::max_pruner_dim, mismatch);
    CHECK(mismatch == 0);
  }

#ifdef FPLLL_WITH_QD
  SECTION("dd and qd factorial")
  {
    // dd/qd keep the historical decimal-string fill: compare with it.  Raw bits
    // are compared because read() overflows to NaN from 171! on (the historical
    // table content there), and NaN never compares equal numerically.
    std::size_t dd_mismatch = 0;
    {
      fplll::Pruner<FP_NR<dd_real>>::TestPruner probe(4);
      const auto &factorial_table = probe.tabulated_factorial();
      for_each_exact_factorial(
          [&](std::size_t i, const mpz_t factorial)
          {
            std::string decimal(mpz_sizeinbase(factorial, 10) + 2, '\0');
            mpz_get_str(decimal.data(), 10, factorial);
            FP_NR<dd_real> reference;
            reference = decimal.c_str();
            if (std::memcmp(&reference.get_data(), &factorial_table[i].get_data(),
                            sizeof(dd_real)) != 0)
            {
              ++dd_mismatch;
            }
          });
    }
    std::size_t qd_mismatch = 0;
    {
      fplll::Pruner<FP_NR<qd_real>>::TestPruner probe(4);
      const auto &factorial_table = probe.tabulated_factorial();
      for_each_exact_factorial(
          [&](std::size_t i, const mpz_t factorial)
          {
            std::string decimal(mpz_sizeinbase(factorial, 10) + 2, '\0');
            mpz_get_str(decimal.data(), 10, factorial);
            FP_NR<qd_real> reference;
            reference = decimal.c_str();
            if (std::memcmp(&reference.get_data(), &factorial_table[i].get_data(),
                            sizeof(qd_real)) != 0)
            {
              ++qd_mismatch;
            }
          });
    }
    CAPTURE(dd_mismatch, qd_mismatch);
    CHECK(dd_mismatch == 0);
    CHECK(qd_mismatch == 0);
  }
#endif
}
