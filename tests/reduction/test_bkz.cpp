// SPDX-FileCopyrightText: 2016 Martin Albrecht
// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <fplll/fplll.h>  // IWYU pragma: keep
#include <fplll/gso/gso.h>
#include <fplll/nr/matrix.h>
#include <fplll/nr/nr.h>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstddef>
#include <gmp.h>
#include <ios>
#include <regex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "test_utils.h"

#ifndef TESTDATADIR
#define TESTDATADIR ".."
#endif

namespace
{

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

/** Strategies for block sizes 0..block_size with complete enumeration (a
    default `PruningParams`) and, optionally, the recursive preprocessing of
    the historical tests. */
std::vector<fplll::Strategy> make_strategies(int block_size, bool with_preprocessing)
{
  std::vector<fplll::Strategy> strategies;
  for (int b = 0; b <= block_size; ++b)
  {
    fplll::Strategy strategy;
    strategy.block_size = static_cast<std::size_t>(b);
    strategy.pruning_parameters.emplace_back();
    if (with_preprocessing)
    {
      if (b == 10)
      {
        strategy.preprocessing_block_sizes.emplace_back(5);
      }
      else if (b == 20)
      {
        strategy.preprocessing_block_sizes.emplace_back(10);
      }
      else if (b == 30)
      {
        strategy.preprocessing_block_sizes.emplace_back(15);
      }
    }
    strategies.emplace_back(std::move(strategy));
  }
  return strategies;
}

/** The strategies of the shipped default table, truncated to the block sizes
    the tests need.  Loaded once per test process. */
std::vector<fplll::Strategy> strategies_from_default_file(int block_size)
{
  static const std::vector<fplll::Strategy> table =
      fplll::load_strategies_json(TESTDATADIR "/strategies/default.json");
  return {table.begin(), table.begin() + static_cast<std::ptrdiff_t>(block_size) + 1};
}

/**
   @brief Test BKZ reduction.

   @param A                test matrix
   @param param            block-reduction parameters
*/
void test_bkz(fplll::ZZ_mat<mpz_t> &A, const fplll::BKZParam &param)
{
  CAPTURE(param.block_size, param.precision);
  INFO("float type: " << fplll::to_string(param.float_type));
  const fplll::ReductionReport report = fplll::bkz_reduction(A, param);
  INFO("reduction status: " << fplll::to_string(report));
  REQUIRE(fplll::succeeded(report));
}

/**
   @brief Test the BKZ strategy interface (pruning and recursive preprocessing).

   @param A                test matrix
   @param block_size       block size
*/
void test_bkz_param(fplll::ZZ_mat<mpz_t> &A, int block_size)
{
  fplll::BKZParam param;
  param.block_size = block_size;
  param.strategies = make_strategies(block_size, true);
  CAPTURE(block_size);
  const fplll::ReductionReport report = fplll::bkz_reduction(A, param);
  INFO("reduction status: " << fplll::to_string(report));
  REQUIRE(fplll::succeeded(report));
}

/**
   @brief Test BKZ with linear-descent pruning.

   @param A                test matrix
   @param block_size       block size
*/
void test_bkz_param_linear_pruning(fplll::ZZ_mat<mpz_t> &A, int block_size)
{
  std::vector<fplll::Strategy> strategies = make_strategies(block_size - 1, true);

  fplll::Strategy strategy;
  strategy.block_size = static_cast<std::size_t>(block_size);
  strategy.pruning_parameters.emplace_back(fplll::linear_pruning_params(
      static_cast<std::size_t>(block_size), static_cast<std::size_t>(block_size / 2)));
  strategies.emplace_back(std::move(strategy));

  fplll::BKZParam param;
  param.block_size = block_size;
  param.strategies = std::move(strategies);
  CAPTURE(block_size);
  const fplll::ReductionReport report = fplll::bkz_reduction(A, param);
  INFO("reduction status: " << fplll::to_string(report));
  REQUIRE(fplll::succeeded(report));
}

/**
   @brief Test BKZ with the pruning strategies of the default strategy file.

   @param A                test matrix
   @param block_size       block size
*/
void test_bkz_param_pruning(fplll::ZZ_mat<mpz_t> &A, int block_size)
{
  fplll::BKZParam param;
  param.block_size = block_size;
  param.strategies = strategies_from_default_file(block_size);
  CAPTURE(block_size);
  const fplll::ReductionReport report = fplll::bkz_reduction(A, param);
  INFO("reduction status: " << fplll::to_string(report));
  REQUIRE(fplll::succeeded(report));
}

/**
   @brief Test BKZ for matrix stored in file pointed to by `input_filename`.

   @param input_filename   a path
   @param param            block-reduction parameters
*/
void test_filename(const char *input_filename, const fplll::BKZParam &param)
{
  INFO("input file: " << input_filename);
  fplll::ZZ_mat<mpz_t> A;
  fplll::ZZ_mat<mpz_t> B;
  REQUIRE(read_file(A, input_filename) == 0);
  B = A;
  test_bkz(A, param);
  test_bkz_param(B, param.block_size);
}

/**
   @brief Construct d x (d+1) integer relations matrix with bit size b and test BKZ.

   @param d                dimension
   @param b                bit size
   @param param            block-reduction parameters
*/
void test_int_rel(int d, int b, const fplll::BKZParam &param)
{
  INFO("integer relation matrix: dimension " << d << ", bit size " << b);
  fplll::ZZ_mat<mpz_t> A;
  fplll::ZZ_mat<mpz_t> B;
  A.resize(d, d + 1);
  A.gen_intrel(b);
  B = A;

  fplll::BKZParam verbose_param = param;
  verbose_param.verbose         = true;
  test_bkz(A, verbose_param);

  test_bkz_param(B, param.block_size);
  test_bkz_param_linear_pruning(B, param.block_size);
  test_bkz_param_pruning(B, param.block_size);
}

void test_linear_dep()
{
  fplll::ZZ_mat<mpz_t> A;
  std::stringstream("[[1 2 3]\n [4 5 6]\n [7 8 9]]\n") >> A;
  test_bkz_param(A, 3);
}

/** Sets the process-wide MPFR default precision for the lifetime of the guard. */
class scoped_mpfr_precision
{
public:
  explicit scoped_mpfr_precision(unsigned int precision)
      : old_precision_(fplll::FP_NR<mpfr_t>::set_prec(precision))
  {
  }

  ~scoped_mpfr_precision() noexcept { fplll::FP_NR<mpfr_t>::set_prec(old_precision_); }

  scoped_mpfr_precision(const scoped_mpfr_precision &)            = delete;
  scoped_mpfr_precision &operator=(const scoped_mpfr_precision &) = delete;

private:
  unsigned int old_precision_;
};

}  // namespace

TEST_CASE("BKZ handles a linearly dependent basis", "[bkz]") { test_linear_dep(); }

TEST_CASE("BKZ reduces dim55_in at block size 10", "[bkz]")
{
  test_filename(TESTDATADIR "/tests/lattices/dim55_in",
                make_bkz_param(10, fplll::BKZVariant::bkz, true));
}

#ifdef FPLLL_WITH_QD
TEST_CASE("BKZ reduces dim55_in at block size 10 with dd arithmetic", "[bkz]")
{
  test_filename(TESTDATADIR "/tests/lattices/dim55_in",
                make_bkz_param(10, fplll::BKZVariant::sd_bkz, true, fplll::FloatType::dd));
}
#endif

TEST_CASE("BKZ reduces dim55_in at block size 10 with SLD reduction", "[bkz]")
{
  test_filename(TESTDATADIR "/tests/lattices/dim55_in",
                make_bkz_param(10, fplll::BKZVariant::slide));
}

TEST_CASE("BKZ reduces dim55_in at block size 20 with MPFR precision 128", "[bkz]")
{
  test_filename(TESTDATADIR "/tests/lattices/dim55_in",
                make_bkz_param(20, fplll::BKZVariant::bkz, true, fplll::FloatType::mpfr, 128));
}

TEST_CASE("BKZ reduces dim55_in at block size 20 with MPFR and the SD variant", "[bkz]")
{
  test_filename(TESTDATADIR "/tests/lattices/dim55_in",
                make_bkz_param(20, fplll::BKZVariant::sd_bkz, true, fplll::FloatType::mpfr, 128));
}

TEST_CASE("BKZ reduces dim55_in at block size 20 with MPFR and SLD reduction", "[bkz]")
{
  test_filename(TESTDATADIR "/tests/lattices/dim55_in",
                make_bkz_param(20, fplll::BKZVariant::slide, false, fplll::FloatType::mpfr, 128));
}

// The generated integer relation matrices come from the global GMP random
// stream, so these runs stay in one test case in their original order.
TEST_CASE("BKZ reduces integer relation lattices", "[bkz]")
{
  // 50 x 51 integer relation matrices with 1000-bit entries.
  test_int_rel(50, 1000, make_bkz_param(10, fplll::BKZVariant::bkz, true));
  test_int_rel(50, 1000, make_bkz_param(10, fplll::BKZVariant::sd_bkz, true));
  test_int_rel(50, 1000, make_bkz_param(10, fplll::BKZVariant::slide));
  test_int_rel(50, 1000,
               make_bkz_param(15, fplll::BKZVariant::bkz, true, fplll::FloatType::mpfr, 100));
  test_int_rel(50, 1000,
               make_bkz_param(15, fplll::BKZVariant::sd_bkz, true, fplll::FloatType::mpfr, 100));
  test_int_rel(50, 1000,
               make_bkz_param(15, fplll::BKZVariant::slide, false, fplll::FloatType::mpfr, 100));
  // 30 x 31 integer relation matrices with 2000-bit entries.
  test_int_rel(30, 2000,
               make_bkz_param(10, fplll::BKZVariant::bkz, true, fplll::FloatType::mpfr, 53));
  test_int_rel(30, 2000,
               make_bkz_param(10, fplll::BKZVariant::sd_bkz, true, fplll::FloatType::mpfr, 53));
  test_int_rel(30, 2000,
               make_bkz_param(10, fplll::BKZVariant::slide, false, fplll::FloatType::mpfr, 53));
}

TEST_CASE("BKZ reduces example_in", "[bkz]")
{
  test_filename(TESTDATADIR "/tests/lattices/example_in", make_bkz_param(10));
}

TEST_CASE("BKZ reduces example_in with the SD variant", "[bkz]")
{
  test_filename(TESTDATADIR "/tests/lattices/example_in",
                make_bkz_param(10, fplll::BKZVariant::sd_bkz));
}

TEST_CASE("BKZ reduces example_in with SLD reduction", "[bkz]")
{
  test_filename(TESTDATADIR "/tests/lattices/example_in",
                make_bkz_param(10, fplll::BKZVariant::slide));
}

TEST_CASE("BKZ reduces example_in with double arithmetic", "[bkz]")
{
  test_filename(
      TESTDATADIR "/tests/lattices/example_in",
      make_bkz_param(10, fplll::BKZVariant::bkz, false, fplll::FloatType::double_precision));
}

TEST_CASE("BKZ reduces example_in with double arithmetic and the SD variant", "[bkz]")
{
  test_filename(
      TESTDATADIR "/tests/lattices/example_in",
      make_bkz_param(10, fplll::BKZVariant::sd_bkz, false, fplll::FloatType::double_precision));
}

TEST_CASE("BKZ reduces example_in with double arithmetic and SLD reduction", "[bkz]")
{
  test_filename(
      TESTDATADIR "/tests/lattices/example_in",
      make_bkz_param(10, fplll::BKZVariant::slide, false, fplll::FloatType::double_precision));
}

TEST_CASE("BKZ reduces example_in with MPFR arithmetic", "[bkz]")
{
  test_filename(TESTDATADIR "/tests/lattices/example_in",
                make_bkz_param(10, fplll::BKZVariant::bkz, true, fplll::FloatType::mpfr, 212));
}

TEST_CASE("BKZ reduces example_in with MPFR arithmetic and the SD variant", "[bkz]")
{
  test_filename(TESTDATADIR "/tests/lattices/example_in",
                make_bkz_param(10, fplll::BKZVariant::sd_bkz, true, fplll::FloatType::mpfr, 212));
}

TEST_CASE("BKZ reduces example_in with MPFR arithmetic and SLD reduction", "[bkz]")
{
  test_filename(TESTDATADIR "/tests/lattices/example_in",
                make_bkz_param(10, fplll::BKZVariant::slide, true, fplll::FloatType::mpfr, 212));
}

// The original harness ran the double-precision example_in cases a second time;
// those runs are kept for parity.
TEST_CASE("BKZ reduces example_in with double arithmetic (second run)", "[bkz]")
{
  test_filename(
      TESTDATADIR "/tests/lattices/example_in",
      make_bkz_param(10, fplll::BKZVariant::bkz, false, fplll::FloatType::double_precision));
}

TEST_CASE("BKZ reduces example_in with double arithmetic and the SD variant (second run)", "[bkz]")
{
  test_filename(
      TESTDATADIR "/tests/lattices/example_in",
      make_bkz_param(10, fplll::BKZVariant::sd_bkz, false, fplll::FloatType::double_precision));
}

TEST_CASE("BKZ reduces example_in with double arithmetic and SLD reduction (second run)", "[bkz]")
{
  test_filename(
      TESTDATADIR "/tests/lattices/example_in",
      make_bkz_param(10, fplll::BKZVariant::slide, false, fplll::FloatType::double_precision));
}

TEST_CASE("BKZ exposes a finite GSO for the 50 by 51 integer relation lattice", "[bkz]")
{
  // The GSO dump used to expose the shape of the basis through a JSON file.
  // Read the same values from the in-memory GSO instead.
  fplll::ZZ_mat<mpz_t> A;
  A.resize(50, 51);
  A.gen_intrel(1000);

  fplll::BKZParam param;
  param.block_size = 15;
  param.strategies = make_strategies(15, true);
  REQUIRE(fplll::succeeded(fplll::bkz_reduction(A, param)));

  fplll::ZZ_mat<mpz_t> U;
  fplll::ZZ_mat<mpz_t> UT;
  fplll::MatGSO<fplll::Z_NR<mpz_t>, fplll::FP_NR<double>> M(A, U, UT);
  M.update_gso();

  const fplll::Matrix<fplll::FP_NR<double>> &r  = M.get_r_matrix();
  const fplll::Matrix<fplll::FP_NR<double>> &mu = M.get_mu_matrix();
  REQUIRE(r.get_rows() == A.get_rows());
  REQUIRE(mu.get_rows() == A.get_rows());
  for (int i = 0; i < A.get_rows(); i++)
  {
    const double diagonal = r(i, i).get_d();
    CHECK(std::isfinite(diagonal));
    CHECK(diagonal > 0.0);
  }
}

// ---------------------------------------------------------------------------
// Verbose logging must not leak formatting state (audit batch B1, BKZ-02).
//
// print_tour()/print_params() used to leave the shared log_stream() at
// std::fixed with a reduced precision, so every later log line of every module
// inherited that state.  The replacement renders with std::format and must keep
// the stream's own formatting untouched, while keeping the historical bytes.
// ---------------------------------------------------------------------------
TEST_CASE("verbose BKZ logging leaves the log stream state untouched", "[bkz]")
{
  std::ostringstream captured;
  captured.setf(std::ios::scientific, std::ios::floatfield);
  captured.precision(2);
  const std::ios_base::fmtflags flags_before = captured.flags();
  const std::streamsize precision_before     = captured.precision();
  const std::streamsize width_before         = captured.width();
  const char fill_before                     = captured.fill();

  struct LogStreamRestore
  {
    ~LogStreamRestore() { fplll::set_log_stream(nullptr); }
  } restore_guard;
  fplll::set_log_stream(&captured);

  fplll::ZZ_mat<mpz_t> basis;
  std::stringstream("[[1 2 3]\n [4 5 6]\n [7 8 9]]\n") >> basis;

  fplll::BKZParam param;
  param.block_size                    = 3;  // equals the number of rows: exactly one tour
  param.verbose                       = true;
  const fplll::ReductionReport report = fplll::bkz_reduction(basis, param);
  REQUIRE(fplll::succeeded(report));

  const std::string output = captured.str();

  // print_params() is deterministic: the whole line has to stay byte-identical.
  CHECK(output.find("block size:   3, variant: BKZ, delta: 0.9900, max_loops:   -, "
                    "max_time:   -, autoAbort: (     -,  -), gh_factor: -\n") != std::string::npos);
  // First block-reduction notice of the tour.
  CHECK(output.find("Block [1-   1] BKZ-3 reduced for the first time\n") != std::string::npos);
  // print_tour(): three decimals for the time, six for the slope and the node
  // count; everything but the CPU time is deterministic.
  const std::regex tour_line("End of BKZ loop +-?[0-9]+, time = +-?[0-9]+\\.[0-9]{3}s, "
                             "r_0 = [^,]+, slope = +-?[0-9]+\\.[0-9]{6}, "
                             "log2\\(nodes\\) = +-?[0-9]+\\.[0-9]{6}\n");
  CHECK(std::regex_search(output, tour_line));

  CHECK(captured.flags() == flags_before);
  CHECK(captured.precision() == precision_before);
  CHECK(captured.width() == width_before);
  CHECK(captured.fill() == fill_before);
}

TEST_CASE("BKZ with the automatic float type and no precision runs in double precision", "[bkz]")
{
  // E-6: `automatic` + 0 keeps the BKZ default, i.e. double precision.
  fplll::ZZ_mat<mpz_t> A;
  std::stringstream("[[10 11 3][11 12 4][1 2 9]]") >> A;
  const fplll::ReductionReport report = fplll::bkz_reduction(A, fplll::BKZParam{.block_size = 2});
  CHECK(fplll::succeeded(report));
  CHECK(report.float_type == fplll::FloatType::double_precision);
  CHECK(report.precision == fplll::PREC_DOUBLE);
}

TEST_CASE("BKZ with an explicit MPFR backend and no precision uses the process default", "[bkz]")
{
  // E-6: explicit MPFR + precision 0 means the current process default MPFR
  // precision; it used to be rejected as a missing precision.
  fplll::ZZ_mat<mpz_t> A;
  std::stringstream("[[10 11 3][11 12 4][1 2 9]]") >> A;
  const scoped_mpfr_precision guard(80);
  const fplll::ReductionReport report = fplll::bkz_reduction(
      A, fplll::BKZParam{.block_size = 2, .float_type = fplll::FloatType::mpfr});
  CHECK(fplll::succeeded(report));
  CHECK(report.float_type == fplll::FloatType::mpfr);
  CHECK(report.precision == 80);
}

TEST_CASE("HKZ validates the backend and precision before the trivial-run early exit", "[bkz]")
{
  // E-6: a basis with fewer than two rows is reduced trivially, but an
  // unsupported backend or a negative precision must still be rejected.
  fplll::ZZ_mat<mpz_t> one(1, 2);
  one(0, 0) = 1;
  one(0, 1) = 2;

  if (!fplll::is_supported(fplll::FloatType::qd))
  {
    CHECK_THROWS_AS(
        (void)fplll::hkz_reduction(one, fplll::HKZOptions{.float_type = fplll::FloatType::qd}),
        fplll::Error);
  }
  CHECK_THROWS_AS((void)fplll::hkz_reduction(one, fplll::HKZOptions{.precision = -5}),
                  fplll::Error);

  // A reducible basis still reports its backend and completed tour.
  fplll::ZZ_mat<mpz_t> two;
  std::stringstream("[[10 11][11 12]]") >> two;
  const fplll::ReductionReport report = fplll::hkz_reduction(two);
  CHECK(fplll::succeeded(report));
  CHECK(report.float_type == fplll::FloatType::double_precision);
  CHECK(report.precision == fplll::PREC_DOUBLE);
  CHECK_FALSE(report.guaranteed);
  REQUIRE(report.tours.has_value());
  CHECK(report.tours == 1L);
  CHECK(report.nodes.has_value());
}
