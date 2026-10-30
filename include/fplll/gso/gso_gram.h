// SPDX-FileCopyrightText: 2005-2008 Damien Stehle
// SPDX-FileCopyrightText: 2007 David Cade
// SPDX-FileCopyrightText: 2011 Xavier Pujol
// SPDX-FileCopyrightText: 2019 Koen de Boer
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef FPLLL_GSO_GRAM_H
#define FPLLL_GSO_GRAM_H

#include <algorithm>
#include <vector>

#include "../defs.h"
#include "../nr/matrix.h"
#include "../util.h"
#include "gso_interface.h"

namespace fplll
{

/**
 * MatGSOGram provides an interface for performing elementary operations on a basis
 * and computing its Gram matrix and its Gram-Schmidt orthogonalization.
 * The Gram-Schmidt coefficients are computed on demand. The object keeps track
 * of which coefficients are valid after each row operation.
 */
template <class ZT, class FT> class MatGSOGram : public MatGSOInterface<ZT, FT>
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
  using MatGSOInterface<ZT, FT>::gptr;
  using MatGSOInterface<ZT, FT>::invalidate_gso_row;

  /**
   * Constructor.
   * The precision of FT can be set with GSOOptions::precision: 0 keeps the
   * precision that is currently configured for FP_NR<mpfr_t>, a positive value
   * is installed for the lifetime of the object and restored afterwards (in any
   * destruction order; see GSOOptions::precision).
   * @param arg_g  the Gram matrix of the basis; it must not be empty
   * @param arg_u  transformation matrix (see MatGSOInterface)
   * @param arg_uinv_t  transposed inverse transformation matrix (see
   *                    MatGSOInterface)
   * @param options  see GSOOptions; the Gram representation requires
   *                 options.int_gram == true and options.row_expo == false
   */
  MatGSOGram(Matrix<ZT> &arg_g, Matrix<ZT> &arg_u, Matrix<ZT> &arg_uinv_t,
             GSOOptions options = {.int_gram = true})
      : MatGSOInterface<ZT, FT>(arg_u, arg_uinv_t, options)
  {
    FPLLL_CHECK(options.int_gram, "MatGSOGram(): int_gram must be true");
    FPLLL_CHECK(!options.row_expo,
                "MatGSOGram(): row_expo is not supported by the Gram representation");
    gptr = &arg_g;
    d    = gptr->get_rows();
    size_increased();
  }

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
   */
  void row_addmul_we(int i, int j, const FT &x, long expo_add) override;

  /**
   * b[i] := b[i] + b[j] and b[i] := b[i] - b[j] (i &gt; j).
   * Must be called inside a row_operations() batch (see RowOperations) that
   * covers row i; a debug build aborts otherwise.
   */
  void row_add(int i, int j) override;
  void row_sub(int i, int j) override;

  ZT &sqnorm_coordinates(ZT &sqnorm, const std::vector<ZT> &coordinates) override;

  FT &get_gram(FT &f, int i, int j) override;

  ZT &get_int_gram(ZT &z, int i, int j) override;

  // b[i] <-> b[j] (i < j)
  void row_swap(int i, int j) override;

private:
  // Bring the protected members of the dependent base class into scope; the
  // using-declarations are private so that the internal state and helpers stay
  // encapsulated.
  using MatGSOInterface<ZT, FT>::d;
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
inline ZT &MatGSOGram<ZT, FT>::sqnorm_coordinates(ZT &sqnorm, const std::vector<ZT> &coordinates)
{
  std::vector<ZT> tmpvec;
  Matrix<ZT> &g = *gptr;
  vector_matrix_product(tmpvec, coordinates, g);

  sqnorm = 0;
  for (int i = 0; i < g.get_cols(); i++)
  {
    ztmp1.mul(tmpvec[i], coordinates[i]);
    sqnorm.add(sqnorm, ztmp1);
  }
  return sqnorm;
}

template <class ZT, class FT> inline long MatGSOGram<ZT, FT>::get_max_exp_of_b()
{
  // normally, this returns the maximum
  // exponent of b. We approximate this temporarily
  // by the half of the maximum exponent of g.
  // beware of errors, because maybe g is
  // zero in the upper half of the matrix.
  return gptr->get_max_exp() / 2;
}

template <class ZT, class FT> inline bool MatGSOGram<ZT, FT>::b_row_is_zero(int i)
{
  // normally this returns whether the
  // ith row of the basis is the zero row.
  // now, we just check whether g[i][i] is zero.
  return (*gptr)[i][i].is_zero();
}

template <class ZT, class FT> inline int MatGSOGram<ZT, FT>::get_cols_of_b() const
{
  // in this gram-matrix version, the number
  // of columns of b is the same as the
  // number of columns of g.
  return gptr->get_cols();
}

template <class ZT, class FT> inline int MatGSOGram<ZT, FT>::get_rows_of_b() const
{
  // in this gram-matrix version, the number
  // of rows of b is the same as the
  // number of rows of g.
  return gptr->get_rows();
}

template <class ZT, class FT> inline void MatGSOGram<ZT, FT>::negate_row_of_b(int i)
{
  if (enable_int_gram)
  {
    for (int j = 0; j < get_rows_of_b(); j++)
    {
      if (j != i)
      {
        sym_g(i, j).neg(sym_g(i, j));
      }
    }
  }
}

template <class ZT, class FT> inline FT &MatGSOGram<ZT, FT>::get_gram(FT &f, int i, int j)
{
  FPLLL_DEBUG_CHECK(i >= 0 && i < n_known_rows && j >= 0 && j <= i && j < n_source_rows &&
                    !this->in_row_op_range(i));
  if (enable_int_gram)
  {
    f.set_z((*gptr)(i, j));
  }
  return f;
}

template <class ZT, class FT> inline ZT &MatGSOGram<ZT, FT>::get_int_gram(ZT &z, int i, int j)
{
  FPLLL_DEBUG_CHECK(i >= 0 && i < n_known_rows && j >= 0 && j <= i && j < n_source_rows &&
                    !this->in_row_op_range(i));
  if (enable_int_gram)
  {
    z = (*gptr)[i][j];
  }
  return z;
}

template <class ZT, class FT> inline void MatGSOGram<ZT, FT>::remove_last_rows(int n_removed_rows)
{
  FPLLL_DEBUG_CHECK(!cols_locked && d >= n_removed_rows);
  d -= n_removed_rows;
  n_known_rows  = std::min(n_known_rows, d);
  n_source_rows = n_known_rows;
  if (enable_transform)
  {
    u.set_rows(d);
  }
}

template <class ZT, class FT> inline void MatGSOGram<ZT, FT>::create_rows(int n_new_rows)
{
  FPLLL_DEBUG_CHECK(!cols_locked);
  int old_d = d;
  d += n_new_rows;

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
    this->discover_all_rows();
  }
}

}  // namespace fplll

#endif
