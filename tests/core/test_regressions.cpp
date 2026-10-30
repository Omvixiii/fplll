// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <fplll/fplll.h>
#include <fplll/gso/gso.h>
#include <fplll/gso/gso_interface.h>
#include <fplll/nr/matrix.h>
#include <fplll/nr/nr.h>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <gmp.h>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{

fplll::ZZ_mat<mpz_t> parse_matrix(std::string_view text)
{
  std::istringstream input{std::string(text)};
  fplll::ZZ_mat<mpz_t> basis;
  input >> basis;
  return basis;
}

/** True when the matrices have the same shape and entries. */
bool same_matrix(const fplll::ZZ_mat<mpz_t> &a, const fplll::ZZ_mat<mpz_t> &b)
{
  if (a.get_rows() != b.get_rows() || a.get_cols() != b.get_cols())
  {
    return false;
  }
  for (int i = 0; i < a.get_rows(); ++i)
  {
    for (int j = 0; j < a.get_cols(); ++j)
    {
      if (a(i, j) != b(i, j))
      {
        return false;
      }
    }
  }
  return true;
}

fplll::ZZ_mat<mpz_t> regular_basis() { return parse_matrix("[[10 11 3][11 12 4][1 2 9]]"); }

}  // namespace

TEST_CASE("LLL validates its options before writing the transformation", "[lll][regression]")
{
  // An invalid delta used to be reported only after u had been overwritten with
  // the identity matrix.
  fplll::ZZ_mat<mpz_t> basis                = parse_matrix("[[1 1][0 1]]");
  const fplll::ZZ_mat<mpz_t> original_basis = basis;
  fplll::ZZ_mat<mpz_t> transform(1, 1);
  transform(0, 0) = 7;

  CHECK_THROWS_AS(
      (void)fplll::lll_reduction(
          basis, transform, fplll::LLLOptions{.parameters = fplll::LLLParameters{.delta = 0.1}}),
      fplll::Error);
  CHECK(same_matrix(basis, original_basis));
  CHECK(transform.get_rows() == 1);
  CHECK(transform.get_cols() == 1);
  CHECK(transform(0, 0) == 7);
}

TEST_CASE("HLLL validates its options before writing the transformation", "[hlll][regression]")
{
  fplll::ZZ_mat<mpz_t> basis                = parse_matrix("[[10 0 0 1][0 10 0 1][0 0 10 1]]");
  const fplll::ZZ_mat<mpz_t> original_basis = basis;
  fplll::ZZ_mat<mpz_t> transform            = basis;
  transform.gen_identity(3);

  CHECK_THROWS_AS(
      (void)fplll::hlll_reduction(
          basis, transform, fplll::HLLLOptions{.parameters = fplll::HLLLParameters{.theta = 0.5}}),
      fplll::Error);
  CHECK(same_matrix(basis, original_basis));
  CHECK(transform(0, 0) == 1);
  CHECK(transform(1, 1) == 1);
  CHECK(transform(2, 2) == 1);
}

TEST_CASE("reductions reject a backend the build does not support before writing",
          "[lll][bkz][regression]")
{
  if (fplll::is_supported(fplll::FloatType::qd))
  {
    SUCCEED("this build has the QD backend");
    return;
  }

  // LLL used to replace u before reporting the unsupported backend.
  {
    fplll::ZZ_mat<mpz_t> basis                = regular_basis();
    const fplll::ZZ_mat<mpz_t> original_basis = basis;
    fplll::ZZ_mat<mpz_t> transform(1, 1);
    transform(0, 0) = 7;
    CHECK_THROWS_AS(
        (void)fplll::lll_reduction(basis, transform,
                                   fplll::LLLOptions{.method     = fplll::LLLMethod::proved,
                                                     .float_type = fplll::FloatType::qd}),
        fplll::Error);
    CHECK(same_matrix(basis, original_basis));
    CHECK(transform(0, 0) == 7);
  }

  // BKZ used to LLL-preprocess the basis before reporting the backend; the
  // transformation matrix must stay untouched as well.
  {
    fplll::ZZ_mat<mpz_t> basis                = regular_basis();
    const fplll::ZZ_mat<mpz_t> original_basis = basis;
    fplll::ZZ_mat<mpz_t> transform(1, 1);
    transform(0, 0) = 7;
    const fplll::BKZParam param{.block_size = 2, .float_type = fplll::FloatType::qd};
    CHECK_THROWS_AS((void)fplll::bkz_reduction(basis, transform, param), fplll::Error);
    CHECK(same_matrix(basis, original_basis));
    CHECK(transform.get_rows() == 1);
    CHECK(transform(0, 0) == 7);
  }
}

TEST_CASE("BKZ rejects an SD block larger than the basis before reducing", "[bkz][regression]")
{
  // SD-BKZ computes `num_rows - block_size`, which used to go negative and
  // index the basis out of bounds.
  fplll::ZZ_mat<mpz_t> basis                = regular_basis();
  const fplll::ZZ_mat<mpz_t> original_basis = basis;
  const fplll::BKZParam param{.block_size = 4, .variant = fplll::BKZVariant::sd_bkz};
  CHECK_THROWS_AS((void)fplll::bkz_reduction(basis, param), fplll::Error);
  CHECK(same_matrix(basis, original_basis));
}

TEST_CASE("BKZ honours an explicit precision with automatic float type", "[bkz][regression]")
{
  // `automatic` used to ignore precision and silently run in double precision;
  // it now selects MPFR like LLL does.
  fplll::ZZ_mat<mpz_t> basis = regular_basis();
  const fplll::BKZParam param{.block_size = 2, .precision = 64};
  const fplll::ReductionReport report = fplll::bkz_reduction(basis, param);
  REQUIRE(fplll::succeeded(report));
  CHECK(report.float_type == fplll::FloatType::mpfr);
  CHECK(report.precision == 64);
}

TEST_CASE("proved LLL below the proved precision reports no guarantee", "[lll][report][regression]")
{
  // E-12: the proved method succeeds at a too-low precision but must not claim
  // the guarantee; this used to be untested.
  fplll::ZZ_mat<mpz_t> basis = regular_basis();
  const int good_prec        = fplll::l2_min_prec(basis.get_rows(), fplll::LLL_DEF_DELTA,
                                                  fplll::LLL_DEF_ETA, fplll::LLL_DEF_EPSILON);
  REQUIRE(good_prec > 10);

  const fplll::ReductionReport report =
      fplll::lll_reduction(basis, fplll::LLLOptions{.method     = fplll::LLLMethod::proved,
                                                    .float_type = fplll::FloatType::mpfr,
                                                    .precision  = 10});
  CHECK(report.status == fplll::RedStatus::success);
  CHECK(fplll::succeeded(report));
  CHECK(report.float_type == fplll::FloatType::mpfr);
  CHECK(report.precision == 10);
  CHECK_FALSE(report.guaranteed);
}

TEST_CASE("LLL reports no backend for an empty basis", "[lll][report][regression]")
{
  // E-5/P2-3: the wrapper constructor used to evaluate log2(0) before lll()
  // returned early.  Every empty shape now reports automatic/0/guaranteed.
  const auto check_report = [](fplll::ZZ_mat<mpz_t> &basis, const fplll::LLLOptions &options)
  {
    const fplll::ReductionReport report = fplll::lll_reduction(basis, options);
    CHECK(report.status == fplll::RedStatus::success);
    CHECK(fplll::succeeded(report));
    CHECK(report.float_type == fplll::FloatType::automatic);
    CHECK(report.precision == 0);
    CHECK(report.guaranteed);
    CHECK_FALSE(report.tours.has_value());
    CHECK_FALSE(report.nodes.has_value());
  };

  fplll::ZZ_mat<mpz_t> empty;
  check_report(empty, fplll::LLLOptions{});
  empty.resize(0, 0);
  check_report(empty, fplll::LLLOptions{.method = fplll::LLLMethod::proved});

  fplll::ZZ_mat<mpz_t> no_rows(0, 3);
  check_report(no_rows, fplll::LLLOptions{});

  fplll::ZZ_mat<mpz_t> no_cols(2, 0);
  check_report(no_cols, fplll::LLLOptions{});
}

TEST_CASE("HLLL, BKZ and HKZ do not run a backend for an empty basis",
          "[hlll][bkz][hkz][report][regression]")
{
  {
    fplll::ZZ_mat<mpz_t> empty;
    const fplll::ReductionReport report = fplll::hlll_reduction(empty);
    CHECK(fplll::succeeded(report));
    CHECK(report.float_type == fplll::FloatType::automatic);
    CHECK(report.precision == 0);
  }
  {
    fplll::ZZ_mat<mpz_t> empty;
    const fplll::ReductionReport report =
        fplll::bkz_reduction(empty, fplll::BKZParam{.block_size = 2});
    CHECK(fplll::succeeded(report));
    CHECK(report.float_type == fplll::FloatType::automatic);
    CHECK(report.precision == 0);
    CHECK_FALSE(report.guaranteed);
    REQUIRE(report.tours.has_value());
    REQUIRE(report.nodes.has_value());
    CHECK(report.tours == 0L);
    CHECK(report.nodes == 0L);
  }
  {
    fplll::ZZ_mat<mpz_t> empty;
    const fplll::ReductionReport report = fplll::hkz_reduction(empty);
    CHECK(fplll::succeeded(report));
    CHECK(report.float_type == fplll::FloatType::automatic);
    CHECK(report.precision == 0);
  }
}

TEST_CASE("BKZ reports the time limit at a tour boundary", "[bkz][regression]")
{
  fplll::ZZ_mat<mpz_t> basis = regular_basis();
  const fplll::BKZParam param{.block_size = 2, .max_time = std::chrono::milliseconds{0}};
  const fplll::ReductionReport report = fplll::bkz_reduction(basis, param);
  CHECK(report.status == fplll::RedStatus::bkz_time_limit);
  CHECK_FALSE(report.guaranteed);
  REQUIRE(report.tours.has_value());
  CHECK(report.tours == 0L);
  CHECK(fplll::to_string(report) == "time limit exceeded in BKZ");
}

TEST_CASE("shortest_vectors fills its outputs for max_sols == 1", "[svp][regression]")
{
  // The list variant used to return success with empty output arrays because
  // the collection loop was gated on max_aux_sols != 0.
  fplll::ZZ_mat<mpz_t> basis = regular_basis();
  fplll::ZZ_mat<mpz_t> empty_mat;
  fplll::MatGSO<fplll::Z_NR<mpz_t>, fplll::FP_NR<mpfr_t>> gso(basis, empty_mat, empty_mat,
                                                              fplll::GSOOptions{.int_gram = true});
  REQUIRE(gso.update_gso());

  std::vector<std::vector<fplll::Z_NR<mpz_t>>> coordinates;
  std::vector<fplll::enumf> distances;
  REQUIRE(fplll::succeeded(fplll::shortest_vectors(gso, coordinates, distances, 1)));
  CHECK(coordinates.size() == 1);
  CHECK(distances.size() == 1);
}

TEST_CASE("HLLL reduces a one-row basis", "[hlll][regression]")
{
  // A one-row basis used to make HLLL index a non-existent second row.
  fplll::ZZ_mat<mpz_t> basis = parse_matrix("[[1 2]]");
  CHECK(fplll::succeeded(fplll::hlll_reduction(basis)));
}

TEST_CASE("BKZ reduces an all-zero basis", "[bkz][regression]")
{
  // An all-zero basis reduces to zero rows; BKZ used to call size_reduction
  // with a negative row index.
  fplll::ZZ_mat<mpz_t> basis(2, 2);
  const fplll::BKZParam param{.block_size = 2};
  CHECK(fplll::succeeded(fplll::bkz_reduction(basis, param)));
}

TEST_CASE("HLLL reduces a regular basis", "[hlll][regression]")
{
  fplll::ZZ_mat<mpz_t> basis = parse_matrix("[[10 0 0 1][0 10 0 1][0 0 10 1]]");
  CHECK(fplll::succeeded(fplll::hlll_reduction(basis)));
}

TEST_CASE("LLL reports the backend, precision and guarantee it used", "[lll][report][regression]")
{
  fplll::ZZ_mat<mpz_t> basis = regular_basis();
  const fplll::ReductionReport report =
      fplll::lll_reduction(basis, fplll::LLLOptions{.method     = fplll::LLLMethod::proved,
                                                    .float_type = fplll::FloatType::mpfr,
                                                    .precision  = 128});
  CHECK(fplll::succeeded(report));
  CHECK(report.status == fplll::RedStatus::success);
  CHECK(fplll::to_string(report) == "success");
  CHECK(report.float_type == fplll::FloatType::mpfr);
  CHECK(report.precision == 128);
  CHECK(report.guaranteed);
  // Tour statistics are only reported by BKZ and HKZ.
  CHECK_FALSE(report.tours.has_value());
  CHECK_FALSE(report.nodes.has_value());
}

TEST_CASE("LLL heuristic reports no guarantee", "[lll][report][regression]")
{
  fplll::ZZ_mat<mpz_t> basis          = regular_basis();
  const fplll::ReductionReport report = fplll::lll_reduction(
      basis, fplll::LLLOptions{.method     = fplll::LLLMethod::heuristic,
                               .float_type = fplll::FloatType::double_precision});
  CHECK(fplll::succeeded(report));
  CHECK_FALSE(report.guaranteed);
  CHECK(report.float_type == fplll::FloatType::double_precision);
  CHECK(report.precision == fplll::PREC_DOUBLE);
}

TEST_CASE("LLL wrapper reports the backend it selected", "[lll][report][regression]")
{
  fplll::ZZ_mat<mpz_t> basis          = regular_basis();
  const fplll::ReductionReport report = fplll::lll_reduction(basis);
  CHECK(fplll::succeeded(report));
  // `automatic` is only reported when no backend was needed.
  CHECK(report.float_type != fplll::FloatType::automatic);
  CHECK(report.guaranteed);
  CHECK(report.precision > 0);
}

TEST_CASE("BKZ reports its backend and tour statistics", "[bkz][report][regression]")
{
  fplll::ZZ_mat<mpz_t> basis = regular_basis();
  const fplll::ReductionReport report =
      fplll::bkz_reduction(basis, fplll::BKZParam{.block_size = 2});
  CHECK(fplll::succeeded(report));
  CHECK(report.float_type == fplll::FloatType::double_precision);
  CHECK(report.precision == fplll::PREC_DOUBLE);
  // BKZ cannot certify the reduction.
  CHECK_FALSE(report.guaranteed);
  REQUIRE(report.tours.has_value());
  CHECK(report.tours >= 1L);
  CHECK(report.nodes.has_value());
}

TEST_CASE("the comma output format handles empty matrices", "[matrix][regression]")
{
  // `[]` and `[[]]` are accepted by the reader; printing them in the comma
  // format used to index matrix[r - 1][c - 1] out of bounds.
  std::ostringstream empty_out;
  const fplll::ZZ_mat<mpz_t> empty;
  empty.print_comma(empty_out);
  CHECK(empty_out.str() == "[]\n");

  std::ostringstream zero_columns_out;
  const fplll::ZZ_mat<mpz_t> zero_columns(1, 0);
  zero_columns.print_comma(zero_columns_out);
  CHECK(zero_columns_out.str() == "[[]]\n");

  std::ostringstream two_zero_rows_out;
  const fplll::ZZ_mat<mpz_t> two_zero_rows(2, 0);
  two_zero_rows.print_comma(two_zero_rows_out);
  CHECK(two_zero_rows_out.str() == "[[],\n[]]\n");

  // The output reads back to the same shape.
  const fplll::ZZ_mat<mpz_t> parsed = parse_matrix("[[]]\n");
  CHECK(parsed.get_rows() == 1);
  CHECK(parsed.get_cols() == 0);
}

TEST_CASE("HKZ reports one completed tour", "[hkz][report][regression]")
{
  // HKZ is the BKZ variant with block_size == number of rows, so it stops
  // after a single tour.
  fplll::ZZ_mat<mpz_t> basis          = regular_basis();
  const fplll::ReductionReport report = fplll::hkz_reduction(basis);
  CHECK(fplll::succeeded(report));
  CHECK_FALSE(report.guaranteed);
  REQUIRE(report.tours.has_value());
  CHECK(report.tours == 1L);
}
