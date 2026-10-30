// SPDX-FileCopyrightText: 2022 Martin R. Albrecht
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <fplll/fplll.h>  // IWYU pragma: keep
#include <fplll/gso/gso.h>
#include <fplll/nr/matrix.h>
#include <fplll/nr/nr.h>

#include <gmp.h>

#include <cstddef>
#include <vector>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

namespace
{

#ifdef FPLLL_WITH_QD
// Restores the x87 control word when the test case ends, so a failing REQUIRE
// cannot leak the QD rounding mode into the test cases that run after it.
class fpu_fix_guard
{
public:
  fpu_fix_guard() { fpu_fix_start(&old_cw); }
  ~fpu_fix_guard() { fpu_fix_end(&old_cw); }

  fpu_fix_guard(const fpu_fix_guard &)            = delete;
  fpu_fix_guard &operator=(const fpu_fix_guard &) = delete;

private:
  unsigned int old_cw;
};
#endif

// Restores the MPFR default precision when the test case ends, so a failing
// REQUIRE cannot leak it into the test cases that run after it.
class mpfr_prec_guard
{
public:
  mpfr_prec_guard() : old_prec(fplll::FP_NR<mpfr_t>::get_prec()) {}
  ~mpfr_prec_guard() { fplll::FP_NR<mpfr_t>::set_prec(old_prec); }

  mpfr_prec_guard(const mpfr_prec_guard &)            = delete;
  mpfr_prec_guard &operator=(const mpfr_prec_guard &) = delete;

private:
  unsigned int old_prec;
};

/**
   @brief Check that Babai rounding recovers the first coordinate of `t`.

   @param n           dimension
   @param bits        bit size of the integer-relation matrix
   @param shouldfail  true when floating point precision is expected to be insufficient
   @param seed        seed for the GMP random state
 */

template <class ZT, class FT>
void test_intrel(long n, long bits, bool shouldfail = false, int seed = 0)
{
  gmp_randseed_ui(fplll::RandGen::get_gmp_state(), seed);
  mpz_t status;
  mpz_init(status);

  fplll::ZZ_mat<ZT> B, U, UT;
  B.resize(n, n + 1);
  B.gen_intrel(bits);

  std::vector<fplll::Z_NR<ZT>> t = std::vector<fplll::Z_NR<ZT>>(static_cast<std::size_t>(n + 1));

  for (long i = 0; i < n; i++)
  {
    mpz_urandomb(status, fplll::RandGen::get_gmp_state(), 1);
    if (mpz_cmp_si(status, 1))
    {
      t[0].add(t[0], B[i][0]);
    }
  }
  mpz_clear(status);

  std::vector<fplll::Z_NR<ZT>> v = t;

  const fplll::ReductionReport lll_report = fplll::lll_reduction(B);
  if (!fplll::succeeded(lll_report))
  {
    UNSCOPED_INFO("LLL reduction failed");
  }
  REQUIRE(fplll::succeeded(lll_report));

  fplll::MatGSO<fplll::Z_NR<ZT>, fplll::FP_NR<FT>> M(B, U, UT);
  M.update_gso();
  M.babai(v, 0, static_cast<int>(n), false);

  std::vector<fplll::Z_NR<ZT>> w = std::vector<fplll::Z_NR<ZT>>(static_cast<std::size_t>(n + 1));

  fplll::Z_NR<ZT> tmp;

  for (long i = 0; i < B.get_rows(); i++)
  {
    for (long j = 0; j < B.get_cols(); j++)
    {
      tmp.mul(v[i], B[i][j]);
      w[j].add(w[j], tmp);
    }
  }

  if ((w[0] == t[0]) == shouldfail)
  {
    UNSCOPED_INFO("n: " << n << ", bits: " << bits << ", shouldfail: " << shouldfail);
    UNSCOPED_INFO("t: " << t);
    UNSCOPED_INFO("w: " << w);
  }
  // Babai rounding must recover the first coordinate exactly, except in the
  // cases that expect the floating point precision to be insufficient.
  CHECK((w[0] == t[0]) != shouldfail);
}

}  // namespace

TEST_CASE("Babai rounding recovers the first coordinate with double arithmetic", "[babai]")
{
  // The historical main() seeded the shared generator once at startup. Every
  // test_intrel call reseeds it with seed 0 for its own matrix, so these test
  // cases are independent of the order in which Catch2 runs them.
  fplll::RandGen::init_with_seed(0);

  test_intrel<mpz_t, double>(10, 20);
  test_intrel<mpz_t, double>(10, 30);
  test_intrel<mpz_t, double>(10, 40);
  test_intrel<mpz_t, double>(10, 50);
  test_intrel<mpz_t, double>(10, 60, true);
}

#ifdef FPLLL_WITH_QD
TEST_CASE("Babai rounding recovers the first coordinate with double-double arithmetic", "[babai]")
{
  // QD needs to have the round-to-double flag set on x86 systems. This function
  // does nothing on non-x86 machines (see the readme for the QD library for more).
  // This is also already done in the wrapper.
  const fpu_fix_guard guard;

  test_intrel<mpz_t, dd_real>(10, 110);
  test_intrel<mpz_t, dd_real>(10, 120, true);
  // The qd_real cases are disabled, exactly as in the historical harness:
  // test_intrel<mpz_t, qd_real>(10, 100);
  // test_intrel<mpz_t, qd_real>(10, 230, true);
}
#endif

TEST_CASE("Babai rounding recovers the first coordinate with mpfr arithmetic", "[babai]")
{
  const mpfr_prec_guard precision_guard;

  fplll::FP_NR<mpfr_t>::set_prec(100);
  test_intrel<mpz_t, mpfr_t>(10, 100);
  fplll::FP_NR<mpfr_t>::set_prec(200);
  test_intrel<mpz_t, mpfr_t>(10, 200);
}
