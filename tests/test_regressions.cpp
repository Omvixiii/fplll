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
#include <sstream>

using namespace fplll;

int main()
{
  // A one-row basis used to make HLLL index a non-existent second row.
  {
    std::istringstream input("[[1 2]]");
    ZZ_mat<mpz_t> basis;
    input >> basis;
    if (hlll_reduction(basis) != RED_SUCCESS)
    {
      std::fprintf(stderr, "one-row HLLL failed\n");
      return 1;
    }
  }

  // An all-zero basis reduces to zero rows; BKZ used to call size_reduction
  // with a negative row index.
  {
    ZZ_mat<mpz_t> basis(2, 2);
    if (bkz_reduction(basis, 2) != RED_SUCCESS)
    {
      std::fprintf(stderr, "zero-basis BKZ failed\n");
      return 2;
    }
  }

  // Regular HLLL reduction must report success.
  {
    std::istringstream input("[[10 0 0 1][0 10 0 1][0 0 10 1]]");
    ZZ_mat<mpz_t> basis;
    input >> basis;
    if (hlll_reduction(basis) != RED_SUCCESS)
    {
      std::fprintf(stderr, "regular HLLL failed\n");
      return 3;
    }
  }

  return 0;
}
