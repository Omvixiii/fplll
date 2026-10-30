// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <fplll/fplll.h>  // IWYU pragma: keep
#include <fplll/nr/matrix.h>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdio>
#include <fstream>
#include <gmp.h>
#include <string>
#include <vector>

namespace
{

std::string write_strategy_file(const char *name, const std::string &content)
{
  const std::string path = std::string("test_config_") + name + ".json";
  std::ofstream out(path);
  out << content;
  return path;
}

template <class F> bool throws(F function)
{
  try
  {
    function();
  }
  catch (...)
  {
    return true;
  }
  return false;
}

// Writes the strategy tables into the test working directory and removes them
// again when the test case finishes.
struct strategy_files
{
  strategy_files()
      : valid(write_strategy_file("valid", R"([{"block_size": 1, "preprocessing_block_sizes": [],)"
                                           R"( "pruning_parameters": [[1.0, [1.0], 1.0]]}])")),
        missing(write_strategy_file("missing", R"([{"pruning_parameters": [[1.0, [1.0], 1.0]]}])")),
        not_array(write_strategy_file("not_array", R"({"block_size": 1})")),
        bad_json(write_strategy_file("bad_json", "{not json"))
  {
  }

  ~strategy_files()
  {
    // No allocation here: a destructor must not throw.
    std::remove(valid.c_str());
    std::remove(missing.c_str());
    std::remove(not_array.c_str());
    std::remove(bad_json.c_str());
  }

  strategy_files(const strategy_files &)            = delete;
  strategy_files &operator=(const strategy_files &) = delete;

  std::string valid;
  std::string missing;
  std::string not_array;
  std::string bad_json;
};

fplll::ZZ_mat<mpz_t> small_basis()
{
  fplll::ZZ_mat<mpz_t> basis(2, 2);
  basis(0, 0) = 1;
  basis(0, 1) = 1;
  basis(1, 0) = 0;
  basis(1, 1) = 1;
  return basis;
}

}  // namespace

TEST_CASE_METHOD(strategy_files, "a valid strategy table is loaded", "[config]")
{
  const std::vector<fplll::Strategy> strategies = fplll::load_strategies_json(valid);
  CHECK(strategies.size() == 2);

  // A strategy table that does not cover the requested block size must be
  // rejected instead of indexing out of bounds.
  fplll::ZZ_mat<mpz_t> basis = small_basis();
  const fplll::BKZParam param{.block_size = 3, .strategies = strategies};
  CHECK(throws([&] { (void)fplll::bkz_reduction(basis, param); }));
}

TEST_CASE_METHOD(strategy_files, "invalid strategy tables are rejected", "[config]")
{
  CHECK(throws([&] { (void)fplll::load_strategies_json(missing); }));
  CHECK(throws([&] { (void)fplll::load_strategies_json(not_array); }));
  CHECK(throws([&] { (void)fplll::load_strategies_json(bad_json); }));
  CHECK(throws([&]
               { (void)fplll::load_strategies_json("fplll_strategy_file_that_does_not_exist"); }));
}

TEST_CASE("BKZ parameters are validated lazily", "[config]")
{
  fplll::ZZ_mat<mpz_t> basis = small_basis();

  // Block size too small.
  CHECK(throws([&] { (void)fplll::bkz_reduction(basis, fplll::BKZParam{.block_size = 1}); }));

  // A strategy table without an entry for the requested block size.
  CHECK(throws(
      [&]
      {
        const std::vector<fplll::Strategy> strategies(2);
        (void)fplll::bkz_reduction(basis,
                                   fplll::BKZParam{.block_size = 2, .strategies = strategies});
      }));

  // Empty strategies disable pruning and recursive preprocessing.
  CHECK(fplll::succeeded(fplll::bkz_reduction(basis, fplll::BKZParam{.block_size = 2})));
}

TEST_CASE("BKZ validates its numeric parameters", "[config]")
{
  fplll::ZZ_mat<mpz_t> basis = small_basis();

  // delta must be in (0.25, 1]; values below the LLL range are rejected.
  CHECK(throws(
      [&] { (void)fplll::bkz_reduction(basis, fplll::BKZParam{.block_size = 2, .delta = 0.1}); }));
  CHECK(fplll::succeeded(
      fplll::bkz_reduction(basis, fplll::BKZParam{.block_size = 2, .delta = 1.0})));

  // A negative time budget is a configuration error.
  CHECK(throws(
      [&]
      {
        (void)fplll::bkz_reduction(
            basis, fplll::BKZParam{.block_size = 2, .max_time = std::chrono::milliseconds{-1}});
      }));

  // SD-BKZ requires a block that fits into the basis.
  CHECK(throws(
      [&]
      {
        (void)fplll::bkz_reduction(
            basis, fplll::BKZParam{.block_size = 3, .variant = fplll::BKZVariant::sd_bkz});
      }));
}
