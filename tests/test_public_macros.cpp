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

/* The public check macros must work in downstream code that does not import
   namespace fplll.  This translation unit deliberately stays in the global
   namespace. */

#include <fplll/fplll.h>

int main()
{
  FPLLL_CHECK(1 + 1 == 2, "arithmetic");
  FPLLL_DEBUG_CHECK(1 + 1 == 2);
  return 0;
}
