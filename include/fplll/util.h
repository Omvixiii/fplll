// SPDX-FileCopyrightText: 2005-2008 Damien Stehle
// SPDX-FileCopyrightText: 2007 David Cade
// SPDX-FileCopyrightText: 2011 Xavier Pujol
// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later
/** \file util.h
    Miscellaneous helpers shared by the reduction algorithms. */

#ifndef FPLLL_UTIL_H
#define FPLLL_UTIL_H

#include "defs.h"
#include "nr/matrix.h"
#include "nr/nr.h"
#include "nr/numvect.h"
#include <cstddef>
#include <vector>

namespace fplll
{

/* Computes result = x * m (it is not required that result is initialized,
   x.size() must be equal to m.GetNumRows()) */
template <class ZT>
void vector_matrix_product(std::vector<ZT> &result, const std::vector<ZT> &x, const Matrix<ZT> &m)
{
  const int nrows = m.get_rows();
  const int ncols = m.get_cols();
  FPLLL_CHECK(x.size() == static_cast<std::size_t>(nrows),
              "vector_matrix_product(): dimensions do not match!");
  gen_zero_vect(result, ncols);
  for (int i = 0; i < nrows; i++)
  {
    for (int j = 0; j < ncols; j++)
    {
      result[j].addmul(x[i], m(i, j));
    }
  }
}

/* Computes result = x * m (it is not required that result is initialized,
   x.size() must be equal to m.GetNumRows()) */
template <class ZT>
void vector_matrix_product(NumVect<ZT> &result, const NumVect<ZT> &x, const Matrix<ZT> &m)
{
  const int nrows = m.get_rows();
  const int ncols = m.get_cols();
  FPLLL_CHECK(x.size() == nrows, "vector_matrix_product(): dimensions do not match!");
  result.gen_zero(ncols);
  for (int i = 0; i < nrows; i++)
  {
    for (int j = 0; j < ncols; j++)
    {
      result[j].addmul(x[i], m(i, j));
    }
  }
}

inline constexpr double DEF_GSO_PREC_EPSILON = 0.03;

/**
 * Returns the minimum precision required to ensure that error bounds on the
 * GSO are valid. Computes rho such that for all 0 &lt;= i &lt; d and 0 &lt;= j &lt;= i:
 *
 *   |r~_i - r_i| / r_i     <= d * rho ^ (i + 1) * 2 ^ (2 - prec)
 *
 *   |mu~_(i,j) - mu_(i,j)| <= d * rho ^ (i + 1) * 2 ^ (4 - prec)
 */
[[nodiscard]] int gso_min_prec(double &rho, int d, double delta, double eta,
                               double epsilon = DEF_GSO_PREC_EPSILON);

/** Returns the minimum precision for the proved version of LLL. */
[[nodiscard]] int l2_min_prec(int d, double delta, double eta, double epsilon);

/** Returns the minimum precision for the proved version of HLLL. */
[[nodiscard]] int hlll_min_prec(int d_i, int n_i, double delta, double eta, double theta, double c);

/** Computes the volume of a d-dimensional hypersphere of radius 1. */
void sphere_volume(FP_NR<mpfr_t> &volume, int d);

/** Estimates the cost of the enumeration for SVP. */
void cost_estimate(FP_NR<mpfr_t> &cost, const FP_NR<mpfr_t> &bound, const Matrix<FP_NR<mpfr_t>> &r,
                   int dimMax);

/** Moves the zero rows of `b` to the front, keeping the rows of `u`/`u_inv_t`
    in sync when they are not empty. */
template <class ZT> void zeros_first(ZZ_mat<ZT> &b, ZZ_mat<ZT> &u, ZZ_mat<ZT> &u_inv_t)
{
  const int d = b.get_rows();
  int i       = d;
  while (i > 0 && b[i - 1].is_zero())
  {
    --i;
  }
  if (i > 0 && i < d)
  {
    b.rotate(0, i, d - 1);
    if (!u.empty())
    {
      u.rotate(0, i, d - 1);
    }
    if (!u_inv_t.empty())
    {
      u_inv_t.rotate(0, i, d - 1);
    }
  }
}

/** Moves the zero rows of `b` to the end, keeping the rows of `u`/`u_inv_t`
    in sync when they are not empty. */
template <class ZT> void zeros_last(ZZ_mat<ZT> &b, ZZ_mat<ZT> &u, ZZ_mat<ZT> &u_inv_t)
{
  const int d = b.get_rows();
  int i       = 0;
  while (i < d && b[i].is_zero())
  {
    ++i;
  }
  if (i > 0 && i < d)
  {
    b.rotate(0, i, d - 1);
    if (!u.empty())
    {
      u.rotate(0, i, d - 1);
    }
    if (!u_inv_t.empty())
    {
      u_inv_t.rotate(0, i, d - 1);
    }
  }
}

}  // namespace fplll

#endif
