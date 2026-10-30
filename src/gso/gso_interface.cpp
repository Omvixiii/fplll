// SPDX-FileCopyrightText: 2005-2008 Damien Stehle
// SPDX-FileCopyrightText: 2007 David Cade
// SPDX-FileCopyrightText: 2011 Xavier Pujol
// SPDX-FileCopyrightText: 2019 Koen de Boer
//
// SPDX-License-Identifier: LGPL-2.1-or-later

/* Template source file */

#include <fplll/defs.h>
#include <fplll/gso/gso_interface.h>
#include <fplll/nr/nr.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <gmp.h>
#include <limits>
#include <numbers>
#include <vector>

namespace fplll
{

template <class ZT, class FT>
void MatGSOInterface<ZT, FT>::invalidate_gso_row(int i, int new_valid_cols)
{
  FPLLL_DEBUG_CHECK(i >= 0 && i < n_known_rows && new_valid_cols >= 0 && new_valid_cols <= i + 1);
  gso_valid_cols[i] = std::min(gso_valid_cols[i], new_valid_cols);
}

template <class ZT, class FT> void MatGSOInterface<ZT, FT>::row_op_end(int first, int last)
{
#ifdef FPLLL_DEBUG_CHECKS
  FPLLL_DEBUG_CHECK(row_op_first == first && row_op_last == last);
  row_op_first = row_op_last = -1;
#endif
  for (int i = first; i < last; i++)
  {
    if (!enable_int_gram)
    {
      update_bf(i);
      invalidate_gram_row(i);
      for (int j = i + 1; j < n_known_rows; j++)
      {
        gf(j, i).set_nan();
      }
    }
    invalidate_gso_row(i, 0);
  }
  for (int i = last; i < n_known_rows; i++)
  {
    invalidate_gso_row(i, first);
  }
}

template <class ZT, class FT> long MatGSOInterface<ZT, FT>::get_max_mu_exp(int i, int n_columns)
{
  FPLLL_DEBUG_CHECK(i >= 0 && i < n_known_rows && gso_valid_cols[i] >= n_columns);
  long max_expo = std::numeric_limits<long>::min(), expo;
  for (int j = 0; j < n_columns; j++)
  {
    long expo2 = get_mu_exp(i, j, expo).exponent();
    max_expo   = std::max(max_expo, expo + expo2);
  }
  return max_expo;
}

template <class ZT, class FT> bool MatGSOInterface<ZT, FT>::update_gso_row(int i, int last_j)
{
  if (i >= n_known_rows)
  {
    discover_row();
  }
  FPLLL_DEBUG_CHECK(i >= 0 && i < n_known_rows && last_j >= 0 && last_j < n_source_rows);

  int j = std::max(0, gso_valid_cols[i]);

  for (; j <= last_j; j++)
  {
    get_gram(ftmp1, i, j);
    FPLLL_DEBUG_CHECK(j == i || gso_valid_cols[j] >= j);
    for (int k = 0; k < j; k++)
    {
      ftmp2.mul(mu(j, k), r(i, k));
      ftmp1.sub(ftmp1, ftmp2);
    }
    r(i, j) = ftmp1;
    if (i > j)
    {
      mu(i, j).div(ftmp1, r(j, j));
      if (!mu(i, j).is_finite())
      {
        return false;
      }
    }
  }

  gso_valid_cols[i] = j;  // = max(0, gso_valid_cols[i], last_j + 1)
  return true;
}

template <class ZT, class FT> void MatGSOInterface<ZT, FT>::lock_cols() { cols_locked = true; }

template <class ZT, class FT> void MatGSOInterface<ZT, FT>::unlock_cols()
{
  n_known_rows = n_source_rows;
  cols_locked  = false;
}

template <class ZT, class FT>
double MatGSOInterface<ZT, FT>::get_current_slope(int start_row, int stop_row)
{
  FT f, log_f;
  long expo;
  int n     = stop_row - start_row;
  double v1 = 0, v2 = static_cast<double>(n + 1) * n * (n - 1) / 12.0, weight = (1.0 - n) / 2.0;
  // Calculate v1 and v2 and return v1 / v2, with
  //   v1 = sum_i (i - avg(i))(x_i - avg(x)),
  //   v2 = sum_i (i - avg(i))^2,
  // where i ranges from start_row (incl.) to stop_row (excl.) and x_i is the
  // natural logarithm of the ith GS basis vector.
  for (int i = start_row; i < stop_row; i++)
  {
    update_gso_row(i);
    f = get_r_exp(i, i, expo);
    log_f.log(f, GMP_RNDU);
    v1 += weight * (log_f.get_d() + static_cast<double>(expo) * std::log(2.0));
    weight++;
  }
  return v1 / v2;
}

template <class ZT, class FT> FT MatGSOInterface<ZT, FT>::get_root_det(int start_row, int end_row)
{
  start_row   = std::max(0, start_row);
  end_row     = std::min(d, end_row);
  FT h        = static_cast<double>(end_row - start_row);
  FT root_det = get_log_det(start_row, end_row) / h;
  root_det.exponential(root_det);
  return root_det;
}

template <class ZT, class FT> FT MatGSOInterface<ZT, FT>::get_log_det(int start_row, int end_row)
{
  FT log_det = 0.0;
  start_row  = std::max(0, start_row);
  end_row    = std::min(d, end_row);
  FT h;
  for (int i = start_row; i < end_row; ++i)
  {
    get_r(h, i, i);
    log_det += log(h);
  }
  return log_det;
}

template <class ZT, class FT>
FT MatGSOInterface<ZT, FT>::get_slide_potential(int start_row, int end_row, int block_size)
{
  FT potential = 0.0;
  int p        = (end_row - start_row) / block_size;
  if ((end_row - start_row) % block_size == 0)
  {
    --p;
  }
  for (int i = 0; i < p; ++i)
  {
    potential += (p - i) * get_log_det(i * block_size, (i + 1) * block_size);
  }
  return potential;
}

template <class FT>
void adjust_radius_to_gh_bound(FT &max_dist, long max_dist_expo, int block_size, const FT &root_det,
                               double gh_factor)
{
  double t = static_cast<double>(block_size) / 2.0 + 1;
  t        = std::lgamma(t);
  t        = std::pow(std::numbers::e, t * 2.0 / static_cast<double>(block_size));
  t        = t / std::numbers::pi;
  FT f     = t;
  f        = f * root_det;
  f.mul_2si(f, -max_dist_expo);
  f = f * gh_factor;
  if (f < max_dist)
  {
    max_dist = f;
  }
}

template <class ZT, class FT>
void MatGSOInterface<ZT, FT>::babai(std::vector<ZT> &v, int start, int dimension)
{
  std::vector<FT> w;
  FT tmp = 0.0;
  for (std::size_t i = 0; i < v.size(); i++)
  {
    tmp.set_z(v[i]);
    w.push_back(tmp);
    v[i] = 0;
  }
  babai(v, w, start, dimension);
}

template <class ZT, class FT>
void MatGSOInterface<ZT, FT>::babai(std::vector<ZT> &w, const std::vector<FT> &v, int start,
                                    int dimension)
{
  FT mu_;
  dimension = (dimension == -1) ? d - start : dimension;

  std::vector<FT> x(v);
  for (long i = dimension - 1; i >= 0; i--)
  {
    x[i].rnd(x[i]);
    for (long j = 0; j < i; j++)
    {
      x[j] -= get_mu(mu_, start + i, start + j) * x[i];
    }
  }
  w.resize(static_cast<std::size_t>(dimension));
  for (long i = 0; i < dimension; i++)
  {
    w[i].set_f(x[i]);
  }
}

template class MatGSOInterface<Z_NR<long>, FP_NR<double>>;
template class MatGSOInterface<Z_NR<mpz_t>, FP_NR<double>>;
template void adjust_radius_to_gh_bound<FP_NR<double>>(FP_NR<double> &max_dist, long max_dist_expo,
                                                       int block_size,
                                                       const FP_NR<double> &root_det,
                                                       double gh_factor);

#ifdef FPLLL_WITH_QD
template class MatGSOInterface<Z_NR<long>, FP_NR<dd_real>>;
template class MatGSOInterface<Z_NR<mpz_t>, FP_NR<dd_real>>;
template void adjust_radius_to_gh_bound<FP_NR<dd_real>>(FP_NR<dd_real> &max_dist,
                                                        long max_dist_expo, int block_size,
                                                        const FP_NR<dd_real> &root_det,
                                                        double gh_factor);
template class MatGSOInterface<Z_NR<long>, FP_NR<qd_real>>;
template class MatGSOInterface<Z_NR<mpz_t>, FP_NR<qd_real>>;
template void adjust_radius_to_gh_bound<FP_NR<qd_real>>(FP_NR<qd_real> &max_dist,
                                                        long max_dist_expo, int block_size,
                                                        const FP_NR<qd_real> &root_det,
                                                        double gh_factor);
#endif

template class MatGSOInterface<Z_NR<long>, FP_NR<mpfr_t>>;
template class MatGSOInterface<Z_NR<mpz_t>, FP_NR<mpfr_t>>;
template void adjust_radius_to_gh_bound<FP_NR<mpfr_t>>(FP_NR<mpfr_t> &max_dist, long max_dist_expo,
                                                       int block_size,
                                                       const FP_NR<mpfr_t> &root_det,
                                                       double gh_factor);

}  // namespace fplll
