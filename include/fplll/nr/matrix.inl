// SPDX-FileCopyrightText: 2005-2008 Damien Stehle
// SPDX-FileCopyrightText: 2007 David Cade
// SPDX-FileCopyrightText: 2011 Xavier Pujol
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef FPLLL_MATRIX_CPP
#define FPLLL_MATRIX_CPP

#include "../defs.h"
#include "matrix.h"
#include "nr.h"
#include "numvect.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <ios>
#include <istream>
#include <limits>
#include <ostream>
#include <utility>
#include <vector>

namespace fplll
{

template <class T> void Matrix<T>::resize(int rows, int cols)
{
  const int old_size = static_cast<int>(matrix.size());
  if (old_size < rows)
  {
    std::vector<NumVect<T>> m2(static_cast<std::size_t>(std::max(old_size * 2, rows)));
    for (int i = 0; i < old_size; i++)
    {
      matrix[i].swap(m2[i]);
    }
    matrix.swap(m2);
  }
  for (int i = r; i < rows; i++)
  {
    matrix[i].resize(cols);
  }
  if (cols != c)
  {
    for (int i = std::min(r, rows) - 1; i >= 0; i--)
    {
      matrix[i].resize(cols);
    }
  }
  r = rows;
  c = cols;
}

template <class T> template <class U> void Matrix<T>::fill(U value)
{
  for (int i = 0; i < r; i++)
  {
    for (int j = 0; j < c; j++)
    {
      matrix[i][j] = value;
    }
  }
}

template <class T> void Matrix<T>::rotate_gram_left(int first, int last, int n_valid_rows)
{
  FPLLL_DEBUG_CHECK(0 <= first && first <= last && last < n_valid_rows && n_valid_rows <= r);
  matrix[first][first].swap(matrix[first][last]);
  for (int i = first; i < last; i++)
  {
    matrix[i + 1][first].swap(matrix[first][i]);
  }
  for (int i = first; i < n_valid_rows; i++)
  {
    matrix[i].rotate_left(first, std::min(last, i));  // most expensive step
  }
  rotate_left(first, last);
}

template <class T> void Matrix<T>::rotate_gram_right(int first, int last, int n_valid_rows)
{
  FPLLL_DEBUG_CHECK(0 <= first && first <= last && last < n_valid_rows && n_valid_rows <= r);
  rotate_right(first, last);
  for (int i = first; i < n_valid_rows; i++)
  {
    matrix[i].rotate_right(first, std::min(last, i));  // most expensive step
  }
  for (int i = first; i < last; i++)
  {
    matrix[i + 1][first].swap(matrix[first][i]);
  }
  matrix[first][first].swap(matrix[first][last]);
}

template <class T> void Matrix<T>::transpose()
{
  extend_vect(matrix, c);
  for (int i = 0; i < c; i++)
  {
    matrix[i].extend(r);
  }
  const int size = std::min(r, c);
  const int dim  = std::max(r, c);
  for (int i = 0; i < size; i++)
  {
    for (int j = i + 1; j < dim; j++)
    {
      matrix[i][j].swap(matrix[j][i]);
    }
    if (c > r)
    {
      matrix[i].resize(r);
    }
  }
  std::swap(r, c);
}

template <class T> T Matrix<T>::get_max()
{
  T m, a;
  m = 0.0;
  for (int i = 0; i < r; i++)
  {
    for (int j = 0; j < c; j++)
    {
      a.abs(matrix[i][j]);
      m = std::max(m, a);
    }
  }
  return m;
}

template <class T> long Matrix<T>::get_max_exp()
{
  long max_exp = 0;
  for (int i = 0; i < r; i++)
  {
    for (int j = 0; j < c; j++)
    {
      max_exp = std::max(max_exp, matrix[i][j].exponent());
    }
  }
  return max_exp;
}

template <class T> void Matrix<T>::print(std::ostream &os, int nrows, int ncols) const
{
  if (nrows < 0 || nrows > r)
  {
    nrows = r;
  }
  if (ncols < 0 || ncols > c)
  {
    ncols = c;
  }
  os << '[';
  for (int i = 0; i < nrows; i++)
  {
    if (i > 0)
    {
      os << '\n';
    }
    os << '[';
    for (int j = 0; j < ncols; j++)
    {
      if (j > 0)
      {
        os << ' ';
      }
      os << matrix[i][j];
    }
    if (print_mode == MatrixPrintMode::regular && ncols > 0)
    {
      os << ' ';
    }
    os << ']';
  }
  if (print_mode == MatrixPrintMode::regular && nrows > 0)
  {
    os << '\n';
  }
  os << ']';
}

template <class T> void Matrix<T>::read(std::istream &is)
{
  char ch;
  matrix.clear();
  if (!(is >> ch))
  {
    return;
  }
  if (ch != '[')
  {
    is.setstate(std::ios::failbit);
    return;
  }
  while (is >> ch && ch != ']')
  {
    is.putback(ch);
    matrix.resize(matrix.size() + 1);
    if (!(is >> matrix.back()))
    {
      matrix.pop_back();
      break;
    }
  }

  r = static_cast<int>(matrix.size());
  c = 0;
  for (int i = 0; i < r; i++)
  {
    c = std::max(c, matrix[i].size());
  }
  for (int i = 0; i < r; i++)
  {
    const int old_c = matrix[i].size();
    if (old_c < c)
    {
      matrix[i].resize(c);
      for (int j = old_c; j < c; j++)
      {
        matrix[i][j] = 0;
      }
    }
  }
}

template <class T> std::ostream &Matrix<T>::print_comma(std::ostream &os) const
{
  os << '[';
  // An empty matrix has no row, and a matrix with empty rows has no entry:
  // both arise from reading `[]` and `[[]]`, so they must not index
  // `matrix[r - 1][c - 1]`.
  if (r == 0 || c == 0)
  {
    for (int i = 0; i < r - 1; i++)
    {
      os << "[],\n";
    }
    if (r > 0)
    {
      os << "[]";
    }
    os << "]\n";
    return os;
  }
  for (int i = 0; i < r - 1; i++)
  {
    os << '[';
    for (int j = 0; j < c - 1; j++)
    {
      os << matrix[i][j] << ", ";
    }
    os << matrix[i][c - 1] << "],\n";
  }
  os << '[';
  for (int j = 0; j < c - 1; j++)
  {
    os << matrix[r - 1][j] << ", ";
  }
  os << matrix[r - 1][c - 1] << "]]\n";

  return os;
}

/* ZZ_mat */

template <class ZT> inline void ZZ_mat<ZT>::gen_intrel(int bits)
{
  if (c != r + 1)
  {
    FPLLL_ABORT("gen_intrel called on an ill-formed matrix");
  }
  if (bits <= 0)
  {
    FPLLL_ABORT("gen_intrel: bits must be positive");
  }
  int i, j;
  for (i = 0; i < r; i++)
  {
    matrix[i][0].randb(bits);
    for (j = 1; j <= i; j++)
    {
      matrix[i][j] = 0;
    }
    matrix[i][i + 1] = 1;
    for (j = i + 2; j < c; j++)
    {
      matrix[i][j] = 0;
    }
  }
}

template <class ZT> inline void ZZ_mat<ZT>::gen_simdioph(int bits, int bits2)
{
  if (c != r)
  {
    FPLLL_ABORT("gen_simdioph called on an ill-formed matrix");
  }
  if (bits <= 0 || bits2 <= 0)
  {
    FPLLL_ABORT("gen_simdioph: bits must be positive");
  }
  int i, j;

  matrix[0][0] = 1;
  matrix[0][0].mul_2si(matrix[0][0], bits2);
  for (i = 1; i < r; i++)
  {
    matrix[0][i].randb(bits);
  }
  for (i = 1; i < r; i++)
  {
    for (j = 1; j < i; j++)
    {
      matrix[j][i] = 0;
    }
    matrix[i][i] = 1;
    matrix[i][i].mul_2si(matrix[i][i], bits);
    for (j = i + 1; j < c; j++)
    {
      matrix[j][i] = 0;
    }
  }
}

template <class ZT> inline void ZZ_mat<ZT>::gen_uniform(int bits)
{
  if (c != r)
  {
    FPLLL_ABORT("gen_uniform called on an ill-formed matrix");
  }
  if (bits <= 0)
  {
    FPLLL_ABORT("gen_uniform: bits must be positive");
  }
  for (int i = 0; i < r; i++)
  {
    for (int j = 0; j < c; j++)
    {
      matrix[i][j].randb(bits);
    }
  }
}

template <class ZT> inline void ZZ_mat<ZT>::gen_ntrulike(const Z_NR<ZT> &q)
{
  // [A00 A01]
  // [A10 A11]

  int i, j, k;
  const int d = r / 2;
  if (c != r || c != 2 * d)
  {
    FPLLL_ABORT("gen_ntrulike called on an ill-formed matrix");
  }
  std::vector<Z_NR<ZT>> h(static_cast<std::size_t>(d));

  h[0] = 0;
  for (i = 1; i < d; i++)
  {
    h[i].randm(q);
    h[0].sub(h[0], h[i]);
    if (h[0].sgn() < 0)
    {
      h[0].add(h[0], q);
    }
    // set h0 such that h(1) = 0 mod q.
  }

  // I in A00
  for (i = 0; i < d; i++)
  {
    for (j = 0; j < i; j++)
    {
      matrix[i][j] = 0;
    }
    matrix[i][i] = 1;
    for (j = i + 1; j < d; j++)
    {
      matrix[i][j] = 0;
    }
  }

  // 0 in A10
  for (i = d; i < r; i++)
  {
    for (j = 0; j < d; j++)
    {
      matrix[i][j] = 0;
    }
  }
  // qI in A11
  for (i = d; i < r; i++)
  {
    for (j = d; j < i; j++)
    {
      matrix[i][j] = 0;
    }
    matrix[i][i] = q;
    for (j = i + 1; j < c; j++)
    {
      matrix[i][j] = 0;
    }
  }
  // H in A01
  for (i = 0; i < d; i++)
  {
    for (j = d; j < c; j++)
    {
      k = j - d - i;
      while (k < 0)
      {
        k += d;
      }
      matrix[i][j] = h[static_cast<std::size_t>(k)];
    }
  }
}

template <class ZT> inline void ZZ_mat<ZT>::gen_ntrulike2(const Z_NR<ZT> &q)
{
  int i, j, k;
  const int d = r / 2;
  if (c != r || c != 2 * d)
  {
    FPLLL_ABORT("gen_ntrulike2 called on an ill-formed matrix");
  }
  std::vector<Z_NR<ZT>> h(static_cast<std::size_t>(d));

  h[0] = 0;
  for (i = 1; i < d; i++)
  {
    h[i].randm(q);
    h[0].sub(h[0], h[i]);
    if (h[0].sgn() < 0)
    {
      h[0].add(h[0], q);
    }
    // set h0 such that h(1) = 0 mod q.
  }

  for (i = 0; i < d; i++)
  {
    for (j = 0; j < c; j++)
    {
      matrix[i][j] = 0;
    }
  }

  for (i = 0; i < d; i++)
  {
    matrix[i][i] = q;
  }
  for (i = d; i < r; i++)
  {
    for (j = d; j < c; j++)
    {
      matrix[i][j] = 0;
    }
  }
  for (i = d; i < c; i++)
  {
    matrix[i][i] = 1;
  }

  for (i = d; i < r; i++)
  {
    for (j = 0; j < d; j++)
    {
      k = i - d - j;
      while (k < 0)
      {
        k += d;
      }
      matrix[i][j] = h[static_cast<std::size_t>(k)];
    }
  }
}

template <class ZT> inline void ZZ_mat<ZT>::gen_qary(int k, const Z_NR<ZT> &q)
{
  int i, j;
  const int d = r;
  if (c != r || k < 0 || k > r)
  {
    FPLLL_ABORT("gen_qary called on an ill-formed matrix");
  }

  for (i = 0; i < d - k; i++)
  {
    for (j = 0; j < d - k; j++)
    {
      matrix[i][j] = 0;
    }
  }

  for (i = 0; i < d - k; i++)
  {
    matrix[i][i] = 1;
  }

  for (i = 0; i < d - k; i++)
  {
    for (j = d - k; j < d; j++)
    {
      matrix[i][j].randm(q);
    }
  }

  for (i = d - k; i < d; i++)
  {
    for (j = 0; j < d; j++)
    {
      matrix[i][j] = 0;
    }
  }

  for (i = d - k; i < d; i++)
  {
    matrix[i][i] = q;
  }
}

template <class ZT> inline void ZZ_mat<ZT>::gen_trg(double alpha)
{
  int i, j, bits;
  Z_NR<ZT> ztmp, ztmp2, zone, sign;

  ztmp2 = 0;
  zone  = 1;

  const int d = r;
  if (c != r)
  {
    FPLLL_ABORT("gen_trg called on an ill-formed matrix");
  }
  if (!std::isfinite(alpha) || alpha <= 0)
  {
    FPLLL_ABORT("gen_trg: alpha must be positive and finite");
  }

  for (i = 0; i < d; i++)
  {
    const double bits_f = std::pow(static_cast<double>(2 * d - i), alpha);
    if (!std::isfinite(bits_f) || bits_f > static_cast<double>(std::numeric_limits<int>::max()))
    {
      FPLLL_ABORT("gen_trg: coefficient exponent is out of range");
    }
    bits = static_cast<int>(bits_f);
    ztmp = 1;
    ztmp.mul_2si(ztmp, bits);
    ztmp.sub(ztmp, zone);
    matrix[i][i].randm(ztmp);
    matrix[i][i].add_ui(matrix[i][i], 2);
    ztmp.div_2si(matrix[i][i], 1);
    for (j = i + 1; j < d; j++)
    {
      matrix[j][i].randm(ztmp);
      sign.randb(1);
      if (sign == 1)
      {
        matrix[j][i].sub(ztmp2, matrix[j][i]);
      }
      matrix[i][j] = 0;
    }
  }
}

template <class ZT> inline void ZZ_mat<ZT>::gen_trg2(FP_NR<mpfr_t> *w)
{
  int i, j;
  Z_NR<ZT> ztmp, ztmp2;

  const int d = r;
  if (c != r)
  {
    FPLLL_ABORT("gen_trg2 called on an ill-formed matrix");
  }

  for (i = 0; i < d; i++)
  {
    matrix[i][i].set_f(w[i]);
    ztmp.div_2si(matrix[i][i], 1);
    ztmp2 = 1;
    ztmp.add(ztmp, ztmp2);
    for (j = i + 1; j < d; j++)
    {
      ztmp2 = 0;
      matrix[j][i].randm(ztmp);
      if (RandGenInt::get_bit() < 0)
      {
        matrix[j][i].sub(ztmp2, matrix[j][i]);
      }
      matrix[i][j] = 0;
    }
  }
}

template <class ZTto, class ZTfrom>
bool convert(ZZ_mat<ZTto> &Ato, const ZZ_mat<ZTfrom> &Afrom, int buffer)
{
  Ato.clear();
  const int r = Afrom.get_rows();
  const int c = Afrom.get_cols();
  Ato.resize(r, c);
  const long threshold = (1L << (std::numeric_limits<long>::digits - buffer - 1));
  Z_NR<ZTfrom> ztmp;
  for (int i = 0; i < r; ++i)
  {
    for (int j = 0; j < c; ++j)
    {
      ztmp.abs(Afrom[i][j]);
      if (ztmp > threshold)
      {
        return false;
      }
      Ato[i][j] = Afrom[i][j].get_si();
    }
  }

  return true;
}

}  // namespace fplll

#endif
