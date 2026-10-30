// SPDX-FileCopyrightText: 2005-2008 Damien Stehle
// SPDX-FileCopyrightText: 2007 David Cade
// SPDX-FileCopyrightText: 2011 Xavier Pujol
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef FPLLL_GSO_H
#define FPLLL_GSO_H

#include <algorithm>
#include <cstddef>
#include <vector>

#include "../defs.h"
#include "../nr/matrix.h"
#include "../util.h"
#include "gso_interface.h"

namespace fplll
{

/**
 * MatGSO provides an interface for performing elementary operations on a basis
 * and computing its Gram matrix and its Gram-Schmidt orthogonalization.
 * The Gram-Schmidt coefficients are computed on demand. The object keeps track
 * of which coefficients are valid after each row operation.
 */
template <class ZT, class FT> class MatGSO : public MatGSOInterface<ZT, FT>
{
public:
  using MatGSOInterface<ZT, FT>::n_known_rows;
  using MatGSOInterface<ZT, FT>::n_source_rows;
  using MatGSOInterface<ZT, FT>::u;
  using MatGSOInterface<ZT, FT>::enable_transform;
  using MatGSOInterface<ZT, FT>::cols_locked;
  using MatGSOInterface<ZT, FT>::enable_int_gram;
  using MatGSOInterface<ZT, FT>::gso_valid_cols;
  using MatGSOInterface<ZT, FT>::enable_inverse_transform;
  using MatGSOInterface<ZT, FT>::u_inv_t;
  using MatGSOInterface<ZT, FT>::mu;
  using MatGSOInterface<ZT, FT>::r;
  using MatGSOInterface<ZT, FT>::ztmp1;
  using MatGSOInterface<ZT, FT>::ztmp2;
  using MatGSOInterface<ZT, FT>::row_op_force_long;
  using MatGSOInterface<ZT, FT>::alloc_dim;
  using MatGSOInterface<ZT, FT>::get_mu;
  using MatGSOInterface<ZT, FT>::get_r;
  using MatGSOInterface<ZT, FT>::gptr;
  using MatGSOInterface<ZT, FT>::invalidate_gso_row;
  using MatGSOInterface<ZT, FT>::gf;
  using MatGSOInterface<ZT, FT>::bf;
  using MatGSOInterface<ZT, FT>::discover_all_rows;
  using MatGSOInterface<ZT, FT>::init_row_size;
  using MatGSOInterface<ZT, FT>::enable_row_expo;
  using MatGSOInterface<ZT, FT>::n_known_cols;
  using MatGSOInterface<ZT, FT>::tmp_col_expo;

  /**
   * Constructor.
   * The precision of FT can be set with GSOOptions::precision: 0 keeps the
   * precision that is currently configured for FP_NR<mpfr_t>, a positive value
   * is installed for the lifetime of the object and restored afterwards (in any
   * destruction order; see GSOOptions::precision).
   * @param arg_b  the matrix on which row operations are performed; it must not
   *               be empty
   * @param arg_u  transformation matrix (see MatGSOInterface)
   * @param arg_uinv_t  transposed inverse transformation matrix (see
   *                    MatGSOInterface)
   * @param options  see GSOOptions
   */
  MatGSO(Matrix<ZT> &arg_b, Matrix<ZT> &arg_u, Matrix<ZT> &arg_uinv_t, GSOOptions options = {})
      : MatGSOInterface<ZT, FT>(arg_u, arg_uinv_t, options), b(arg_b)
  {
    FPLLL_DEBUG_CHECK(!(enable_int_gram && enable_row_expo));
    d = b.get_rows();
    if (enable_row_expo)
    {
      tmp_col_expo.resize(b.get_cols());
    }
    if (enable_int_gram)
    {
      gptr = &g;
    }
    size_increased();
  }

  /**
   * Basis of the lattice.
   *
   * The matrix is owned by the caller, but it must not be modified directly:
   * every write has to go through the row operations of the GSO (inside a
   * row_operations() batch), otherwise the cached Gram matrix and Gram-Schmidt
   * coefficients become inconsistent with b.
   */
  Matrix<ZT> &b;
  /**
   * Integer Gram matrix of the lattice
   */
  Matrix<ZT> g;

  ZT &sqnorm_coordinates(ZT &sqnorm, const std::vector<ZT> &coordinates) override;

  [[nodiscard]] long get_max_exp_of_b() override;
  [[nodiscard]] bool b_row_is_zero(int i) override;
  [[nodiscard]] int get_cols_of_b() const override;
  [[nodiscard]] int get_rows_of_b() const override;
  void negate_row_of_b(int i) override;

  void create_rows(int n_new_rows) override;
  void remove_last_rows(int n_removed_rows) override;

  void move_row(int old_r, int new_r) override;

  /**
   * b[i] := b[i] + x * 2^expo_add * b[j].
   * Must be called inside a row_operations() batch (see RowOperations) that
   * covers row i; a debug build aborts otherwise.
   * Special cases |x| &lt;= 1 and |x| &lt;= LONG_MAX are optimized.
   * x should be an integer.
   * If row_op_force_long=true, x is always converted to (2^expo * long) instead
   * of (2^expo * ZT), which is faster if ZT=mpz_t but might lead to a loss of
   * precision (in LLL, more Babai iterations are needed).
   */
  void row_addmul_we(int i, int j, const FT &x, long expo_add) override;

  /**
   * b[i] := b[i] + b[j] and b[i] := b[i] - b[j] (i &gt; j).
   * Must be called inside a row_operations() batch (see RowOperations) that
   * covers row i; a debug build aborts otherwise.
   */
  void row_add(int i, int j) override;
  void row_sub(int i, int j) override;

  FT &get_gram(FT &f, int i, int j) override;

  ZT &get_int_gram(ZT &z, int i, int j) override;

  // b[i] <-> b[j] (i < j)
  void row_swap(int i, int j) override;

  /**
     Given a vector `w` wrt the canonical basis `ZZ^n` return a vector `v` wrt the Gram-Schmidt
     basis `B^*`.

     @param v          a tuple-like object of dimension ``M.B.ncols``
     @param start      only consider subbasis starting at ``start``
     @param dimension: only consider ``dimension`` vectors or all if ``-1``
   */
  void from_canonical(std::vector<FT> &v, const std::vector<FT> &w, long start = 0,
                      long dimension = -1);

  /**
   * Update`v` to `w` s.t. `‖w⋅B - v‖` is small using Babai's nearest plane algorithm.
   *
   * @param v         a vector
   * @param start     only consider subbasis starting at start
   * @param dimension only consider dimension vectors or all if -1
   * @param gso       if true then input `v` is considered to be expressed to `B*`
   */
  void babai(std::vector<ZT> &v, int start = 0, int dimension = -1, bool gso = false);

  /**
   * Return vector `w` s.t. `‖w⋅B - v‖` is small using Babai's nearest plane algorithm.
   *
   * @param w         a vector
   * @param v         a vector
   * @param start     only consider subbasis starting at start
   * @param dimension only consider dimension vectors or all if -1
   * @param gso       if true then input `v` is considered to be expressed wrt `B*`
   */
  void babai(std::vector<ZT> &w, const std::vector<FT> &v, int start = 0, int dimension = -1,
             bool gso = false);

private:
  // Bring the protected members of the dependent base class into scope; the
  // using-declarations are private so that the internal state and helpers stay
  // encapsulated.
  using MatGSOInterface<ZT, FT>::d;
  using MatGSOInterface<ZT, FT>::row_expo;
  using MatGSOInterface<ZT, FT>::sym_g;
  using MatGSOInterface<ZT, FT>::inside_row_operations;
  using MatGSOInterface<ZT, FT>::in_row_op_range;

  void size_increased() override;

  void discover_row() override;

  /* Upates the i-th row of bf. It does not invalidate anything, so the caller
     must take into account that it might change row_expo. */
  void update_bf(int i) override;
  /* Marks g(i, j) for all j <= i (but NOT for j > i) */
  void invalidate_gram_row(int i) override;

  // b[i] <- b[i] + x * b[j] (i > j)
  void row_addmul_si(int i, int j, long x) override;
  // b[i] <- b[i] + (2^expo * x) * b[j] (i > j)
  void row_addmul_si_2exp(int i, int j, long x, long expo);
  void row_addmul_2exp(int i, int j, const ZT &x, long expo);
};

template <class ZT, class FT>
inline ZT &MatGSO<ZT, FT>::sqnorm_coordinates(ZT &sqnorm, const std::vector<ZT> &coordinates)
{
  std::vector<ZT> tmpvec;
  ZT tmp;
  sqnorm = 0;
  vector_matrix_product(tmpvec, coordinates, b);
  for (std::size_t j = 0; j < tmpvec.size(); j++)
  {
    tmp.mul(tmpvec[j], tmpvec[j]);
    sqnorm.add(sqnorm, tmp);
  }
  return sqnorm;
}

template <class ZT, class FT> inline long MatGSO<ZT, FT>::get_max_exp_of_b()
{
  return b.get_max_exp();
}

template <class ZT, class FT> inline bool MatGSO<ZT, FT>::b_row_is_zero(int i)
{
  return b[i].is_zero();
}

template <class ZT, class FT> inline int MatGSO<ZT, FT>::get_cols_of_b() const
{
  return b.get_cols();
}

template <class ZT, class FT> inline int MatGSO<ZT, FT>::get_rows_of_b() const
{
  return b.get_rows();
}

template <class ZT, class FT> inline void MatGSO<ZT, FT>::negate_row_of_b(int i)
{
  for (int j = 0; j < get_cols_of_b(); j++)
  {
    b[i][j].neg(b[i][j]);
  }
  if (enable_int_gram)
  {
    for (int j = 0; j < get_rows_of_b(); j++)
    {
      if (j < i)
      {
        g(i, j).neg(g(i, j));
      }
      else if (j > i)
      {
        g(j, i).neg(g(j, i));
      }
    }
  }
}

template <class ZT, class FT> inline FT &MatGSO<ZT, FT>::get_gram(FT &f, int i, int j)
{
  FPLLL_DEBUG_CHECK(i >= 0 && i < n_known_rows && j >= 0 && j <= i && j < n_source_rows &&
                    !this->in_row_op_range(i));
  if (enable_int_gram)
  {
    f.set_z(g(i, j));
  }
  else
  {
    if (gf(i, j).is_nan())
    {
      bf[i].dot_product(gf(i, j), bf[j], n_known_cols);
    }
    f = gf(i, j);
  }
  return f;
}

template <class ZT, class FT> inline ZT &MatGSO<ZT, FT>::get_int_gram(ZT &z, int i, int j)
{
  FPLLL_DEBUG_CHECK(i >= 0 && i < n_known_rows && j >= 0 && j <= i && j < n_source_rows &&
                    !this->in_row_op_range(i));
  if (enable_int_gram)
  {
    z = g(i, j);
  }
  else
  {
    b[i].dot_product(z, b[j], n_known_cols);
  }
  return z;
}

template <class ZT, class FT> inline void MatGSO<ZT, FT>::create_rows(int n_new_rows)
{
  FPLLL_DEBUG_CHECK(!cols_locked);
  int old_d = d;
  d += n_new_rows;
  b.set_rows(d);
  for (int i = old_d; i < d; i++)
  {
    for (int j = 0; j < b.get_cols(); j++)
    {
      b[i][j] = 0;
    }
  }
  if (enable_transform)
  {
    u.set_rows(d);
    for (int i = old_d; i < d; i++)
    {
      for (int j = 0; j < u.get_cols(); j++)
      {
        u[i][j] = 0;
      }
    }
  }
  size_increased();
  if (n_known_rows == old_d)
  {
    discover_all_rows();
  }
}

template <class ZT, class FT> inline void MatGSO<ZT, FT>::remove_last_rows(int n_removed_rows)
{
  FPLLL_DEBUG_CHECK(!cols_locked && d >= n_removed_rows);
  d -= n_removed_rows;
  n_known_rows  = std::min(n_known_rows, d);
  n_source_rows = n_known_rows;
  b.set_rows(d);
  if (enable_transform)
  {
    u.set_rows(d);
  }
}

}  // namespace fplll

#endif
