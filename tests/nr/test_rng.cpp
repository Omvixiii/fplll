// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <fplll/fplll.h>  // IWYU pragma: keep
#include <fplll/nr/matrix.h>
#include <fplll/nr/nr.h>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <gmp.h>
#include <thread>
#include <vector>

namespace
{

bool equal_bases(const fplll::ZZ_mat<mpz_t> &a, const fplll::ZZ_mat<mpz_t> &b)
{
  if (a.get_rows() != b.get_rows() || a.get_cols() != b.get_cols())
  {
    return false;
  }
  for (int i = 0; i < a.get_rows(); ++i)
  {
    for (int j = 0; j < a.get_cols(); ++j)
    {
      if (a[i][j].cmp(b[i][j]) != 0)
      {
        return false;
      }
    }
  }
  return true;
}

}  // namespace

TEST_CASE("the global random generator is reproducibly seeded and thread-safe", "[rng]")
{
  // The same seed must produce the same basis.
  fplll::RandGen::init_with_seed(1234567);
  fplll::ZZ_mat<mpz_t> first(8, 8);
  first.gen_uniform(32);

  fplll::RandGen::init_with_seed(1234567);
  fplll::ZZ_mat<mpz_t> second(8, 8);
  second.gen_uniform(32);

  INFO("seeded generation is not reproducible");
  CHECK(equal_bases(first, second));

  // Concurrent generation must be safe (run under TSan to check races).
  std::vector<std::thread> threads;
  threads.reserve(8);
  for (int t = 0; t < 8; ++t)
  {
    threads.emplace_back(
        []
        {
          fplll::ZZ_mat<mpz_t> basis(6, 6);
          basis.gen_uniform(16);
        });
  }
  for (auto &thread : threads)
  {
    thread.join();
  }
}
