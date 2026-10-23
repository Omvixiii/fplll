/* Copyright (C) 2026 The fplll development team

   This file is part of fplll. fplll is free software: you
   can redistribute it and/or modify it under the terms of the GNU Lesser
   General Public License as published by the Free Software Foundation,
   either version 2.1 of the License, or (at your option) any later version.

   fplll is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with fplll. If not, see <http://www.gnu.org/licenses/>. */

#include <fplll/fplll.h>

#include <cstdio>
#include <thread>
#include <vector>

using namespace fplll;

namespace
{

bool equal_bases(const ZZ_mat<mpz_t> &a, const ZZ_mat<mpz_t> &b)
{
  if (a.get_rows() != b.get_rows() || a.get_cols() != b.get_cols())
    return false;
  for (int i = 0; i < a.get_rows(); ++i)
  {
    for (int j = 0; j < a.get_cols(); ++j)
    {
      if (a[i][j].cmp(b[i][j]) != 0)
        return false;
    }
  }
  return true;
}

}  // namespace

int main()
{
  // The same seed must produce the same basis.
  RandGen::init_with_seed(1234567);
  ZZ_mat<mpz_t> first(8, 8);
  first.gen_uniform(32);

  RandGen::init_with_seed(1234567);
  ZZ_mat<mpz_t> second(8, 8);
  second.gen_uniform(32);

  if (!equal_bases(first, second))
  {
    std::fprintf(stderr, "seeded generation is not reproducible\n");
    return 1;
  }

  // Concurrent generation must be safe (run under TSan to check races).
  std::vector<std::thread> threads;
  for (int t = 0; t < 8; ++t)
  {
    threads.emplace_back(
        []
        {
          ZZ_mat<mpz_t> basis(6, 6);
          basis.gen_uniform(16);
        });
  }
  for (auto &thread : threads)
    thread.join();

  return 0;
}
