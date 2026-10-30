// SPDX-FileCopyrightText: 2005-2008 Damien Stehle
// SPDX-FileCopyrightText: 2007 David Cade
// SPDX-FileCopyrightText: 2011 Xavier Pujol
// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef FPLLL_REDUCTION_LLL_FAMILY_H
#define FPLLL_REDUCTION_LLL_FAMILY_H

#include <fplll/defs.h>
#include <fplll/nr/matrix.h>

#include <gmp.h>

namespace fplll
{

/** Shared entry-point helpers of the LLL and HLLL drivers. */

/** Checks the LLL parameters shared by the LLL and HLLL entry points. */
inline void check_lll_parameters(double delta, double eta)
{
  FPLLL_CHECK(delta > 0.25 && delta < 1.0, "LLL/HLLL delta must be in (0.25, 1) (got {})", delta);
  FPLLL_CHECK(eta > 0.5 && eta < 1.0, "LLL/HLLL eta must be in (0.5, 1) (got {})", eta);
}

/** True when the reductions can skip `b` without touching it. */
inline bool is_empty_basis(const ZZ_mat<mpz_t> &b)
{
  return b.get_rows() == 0 || b.get_cols() == 0;
}

}  // namespace fplll

#endif
