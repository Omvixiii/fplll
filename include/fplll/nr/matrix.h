// SPDX-FileCopyrightText: 2005-2008 Damien Stehle
// SPDX-FileCopyrightText: 2007 David Cade
// SPDX-FileCopyrightText: 2011 Xavier Pujol
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef FPLLL_MATRIX_H
#define FPLLL_MATRIX_H

#include <iosfwd>
#include <type_traits>
#include <utility>
#include <vector>

#include "../defs.h"
#include "nr.h"
#include "numvect.h"

namespace fplll
{

/** Output format of Matrix::print and of the stream operator. */
// Public enum: the int underlying type is kept for ABI stability.
// NOLINTNEXTLINE(performance-enum-size)
enum class MatrixPrintMode
{
  compact,
  regular
};

template <class T> class Matrix;
template <class T> class ConstMatrixRow;

/** MatrixRow stores a reference to a writable row of a Matrix. It supports a
    subset of operations available on vectors. Rows of constant matrices are
    exposed as ConstMatrixRow objects instead, which are read-only. */
template <class T> class MatrixRow
{
public:
  /** Returns a reference to the i-th element of this row. */
  T &operator[](int i) { return row[i]; }
  /** Returns a const reference to the i-th element of this row on constant
      objects. */
  const T &operator[](int i) const { return row[i]; }

  bool is_zero(int from = 0) const { return row.is_zero(from); }
  int size_nz() const { return row.size_nz(); }
  void fill(long value) { row.fill(value); }
  void add(const MatrixRow<T> &v) { row.add(v.row); }
  void add(const MatrixRow<T> &v, int n) { row.add(v.row, n); }
  void sub(const MatrixRow<T> &v) { row.sub(v.row); }
  void sub(const MatrixRow<T> &v, int n) { row.sub(v.row, n); }
  // Kept by value: part of the public template interface.
  // NOLINTBEGIN(performance-unnecessary-value-param)
  void div(const MatrixRow<T> &v, int b, int n, T x) { row.div(v.row, b, n, x); }
  void addmul(const MatrixRow<T> &v, T x, int n) { row.addmul(v.row, x, n); }
  void addmul(const MatrixRow<T> &v, T x, int b, int n) { row.addmul(v.row, x, b, n); }
  // NOLINTEND(performance-unnecessary-value-param)
  void addmul_2exp(const MatrixRow<T> &v, const T &x, long expo, T &tmp)
  {
    row.addmul_2exp(v.row, x, expo, tmp);
  }
  void addmul_2exp(const MatrixRow<T> &v, const T &x, long expo, int n, T &tmp)
  {
    row.addmul_2exp(v.row, x, expo, n, tmp);
  }
  void addmul_si(const MatrixRow<T> &v, long x) { row.addmul_si(v.row, x); }
  void addmul_si(const MatrixRow<T> &v, long x, int n) { row.addmul_si(v.row, x, n); }
  void addmul_si_2exp(const MatrixRow<T> &v, long x, long expo, T &tmp)
  {
    row.addmul_si_2exp(v.row, x, expo, tmp);
  }
  void addmul_si_2exp(const MatrixRow<T> &v, long x, long expo, int n, T &tmp)
  {
    row.addmul_si_2exp(v.row, x, expo, n, tmp);
  }

  /** Computes the truncated dot product between two rows of a Matrix.
   * Constraint: n > beg.
   */
  void dot_product(T &result, const MatrixRow<T> &v0, int beg, int n) const
  {
    fplll::dot_product(result, this->row, v0.row, beg, n);
  }

  /** Computes the dot product between two rows of a Matrix, up to index n. */
  void dot_product(T &result, const MatrixRow<T> &v0, int n) const
  {
    fplll::dot_product(result, this->row, v0.row, n);
  }

  friend class Matrix<T>;

private:
  MatrixRow(NumVect<T> &row) : row(row) {}
  NumVect<T> &row;
};

/** ConstMatrixRow stores a reference to a read-only row of a constant Matrix.
    It exposes only the operations that do not modify the row, so a constant
    matrix cannot be written through it. */
template <class T> class ConstMatrixRow
{
public:
  /** Returns a const reference to the i-th element of this row. */
  const T &operator[](int i) const noexcept { return row[i]; }

  bool is_zero(int from = 0) const { return row.is_zero(from); }
  int size_nz() const { return row.size_nz(); }

  /** Computes the truncated dot product between two rows of a Matrix.
   * Constraint: n > beg.
   */
  void dot_product(T &result, const ConstMatrixRow<T> &v0, int beg, int n) const
  {
    fplll::dot_product(result, this->row, v0.row, beg, n);
  }

  /** Computes the dot product between two rows of a Matrix, up to index n. */
  void dot_product(T &result, const ConstMatrixRow<T> &v0, int n) const
  {
    fplll::dot_product(result, this->row, v0.row, n);
  }

  friend class Matrix<T>;

private:
  ConstMatrixRow(const NumVect<T> &row) noexcept : row(row) {}
  const NumVect<T> &row;
};

/** Matrix is a two-dimensional container. Read and write operations on single
    elements are in constant time. The amortized complexity of resizing the
    matrix is proportional to the number of added/removed elements. All indices
    are 0-based. */
template <class T> class Matrix
{
public:
  /** Creates an empty matrix (0 x 0). */
  Matrix() : r(0), c(0) {}
  /** Creates a matrix of dimensions rows x cols. All elements are
      initialized with the default constructor of T. */
  Matrix(int rows, int cols) : r(0), c(0) { resize(rows, cols); }

  /** Sets number of rows and the number of columns to 0. */
  void clear()
  {
    r = c = 0;
    matrix.clear();
  }
  /** Returns true if the matrix has 0 rows, false otherwise. */
  bool empty() const { return r == 0; }
  /** Sets the dimensions of this matrix, preserving as much as possible of the
      content. The value of new elements is undefined. */
  void resize(int rows, int cols);
  /** Sets the number of rows. Content is not erased except for deleted rows.
      The value of new elements is undefined. */
  void set_rows(int rows) { resize(rows, c); }
  /** Sets the number of columns. Content is not erased except for deleted
      columns. The value of new elements is undefined. */
  void set_cols(int cols) { resize(r, cols); }
  /** Fills this matrix with a given value. */
  template <class U> void fill(U value);
  /** Efficiently swaps two matrices. */
  void swap(Matrix<T> &m)
  {
    matrix.swap(m.matrix);
    std::swap(r, m.r);
    std::swap(c, m.c);
  }

  /** Returns the number of rows */
  int get_rows() const { return r; }
  /** Returns the number of columns */
  int get_cols() const { return c; }
  /** Returns a reference to the element (i, j). */
  T &operator()(int i, int j)
  {
    FPLLL_DEBUG_CHECK(i >= 0 && i < r && j >= 0 && j < c);
    return matrix[i][j];
  }
  /** Returns a constant reference to the element (i, j) on constant objects. */
  const T &operator()(int i, int j) const
  {
    FPLLL_DEBUG_CHECK(i >= 0 && i < r && j >= 0 && j < c);
    return matrix[i][j];
  }
  /** Returns a MatrixRow object pointing to the i-th row of this matrix. On
      constant objects it returns a read-only ConstMatrixRow instead. */
  template <class Self> auto operator[](this Self &&self, int i)
  {
    FPLLL_DEBUG_CHECK(i >= 0 && i < self.r);
    if constexpr (std::is_const_v<std::remove_reference_t<Self>>)
    {
      return ConstMatrixRow<T>(self.matrix[i]);
    }
    else
    {
      return MatrixRow<T>(self.matrix[i]);
    }
  }
  /** Rows swap. */
  void swap_rows(int r1, int r2) { matrix[r1].swap(matrix[r2]); }
  /** Rows permutation.
      (m[first],...,m[last]) becomes (m[first+1],...,m[last],m[first]) */
  void rotate_left(int first, int last) { rotate_left_by_swap(matrix, first, last); }
  /** Rows permutation.
      (m[first],...,m[last]) becomes (m[last],m[first],...,m[last-1]) */
  void rotate_right(int first, int last) { rotate_right_by_swap(matrix, first, last); }
  /** Rows permutation.
      (m[first],...,m[middle-1],m[middle],m[last]) becomes
      (m[middle],...,m[last],m[first],...,m[middle-1]) */
  void rotate(int first, int middle, int last) { rotate_by_swap(matrix, first, middle, last); }
  /** Transformation needed to update the lower triangular Gram matrix when
     rotate_left(first, last) is done on the basis of the lattice. */
  void rotate_gram_left(int first, int last, int n_valid_rows);
  /** Transformation needed to update the lower triangular Gram matrix when
      rotate_right(first, last) is done on the basis of the lattice. */
  void rotate_gram_right(int first, int last, int n_valid_rows);
  /** Transpose. */
  void transpose();
  T get_max();
  long get_max_exp();
  /** Prints this matrix. No end-of-line is printed after the last line.
      @param os      output stream
      @param nrows   maximum number of rows to display (optional)
      @param ncols   maximum number of columns to display (optional) */
  void print(std::ostream &os, int nrows = -1, int ncols = -1) const;
  /** Reads this matrix from a stream. */
  void read(std::istream &is);
  /** Print the matrix, sage format style */
  std::ostream &print_comma(std::ostream &os) const;
  /** Changes the output format style of Matrix and returns the previous
      mode. */
  static MatrixPrintMode set_print_mode(MatrixPrintMode new_print_mode)
  {
    const MatrixPrintMode old_mode = print_mode;
    print_mode                     = new_print_mode;
    return old_mode;
  }

protected:
  int r, c;
  std::vector<NumVect<T>> matrix;

  static inline MatrixPrintMode print_mode = MatrixPrintMode::compact;
};

template <class T> std::ostream &operator<<(std::ostream &os, const Matrix<T> &m)
{
  m.print(os);
  return os;
}

template <class T> std::istream &operator>>(std::istream &is, Matrix<T> &m)
{
  m.read(is);
  return is;
}

/** ZZ_mat is a matrix of integers. */
template <class ZT> class ZZ_mat : public Matrix<Z_NR<ZT>>
{
public:
  using T = Z_NR<ZT>;
  using Matrix<T>::r;
  using Matrix<T>::c;
  using Matrix<T>::matrix;
  using Matrix<T>::resize;
  using Matrix<T>::get_cols;
  using Matrix<T>::get_rows;

  /** Creates an empty matrix (0 x 0). */
  ZZ_mat() : Matrix<T>() {}
  /** Creates a matrix of dimensions rows x cols. All elements are
      initialized with the default constructor of Z_NR&lt;T&gt;. */
  ZZ_mat(int rows, int cols) : Matrix<T>(rows, cols) {}

  /** Generate a zero matrix */
  void gen_zero(int d, int n)
  {
    resize(d, n);
    for (int i = 0; i < d; i++)
    {
      matrix[i].fill(0);
    }
  }
  /** Generate an identity matrix */
  void gen_identity(int d)
  {
    gen_zero(d, d);
    for (int i = 0; i < d; i++)
    {
      matrix[i][i] = 1;
    }
  }
  /** Generate an augmented matrix of random coefficients for the first column */
  void gen_intrel(int bits);
  /** Generate a matrix with coefficients of the first row and the diagonal */
  void gen_simdioph(int bits, int bits2);
  /** Generate a random matrix of uniform distribution. */
  void gen_uniform(int bits);

  /** Generate a value for q in the interval [2^{bits-1}, 2^bits - 1],
     uniformly at random, and store the result in q.
  */
  static void gen_q(int bits, Z_NR<ZT> &q)
  {
    if (bits < 1)
    {
      FPLLL_ABORT("gen_q: bits must be positive");
    }
    Z_NR<ZT> pow2;
    pow2 = 1;
    pow2.mul_2si(pow2, bits - 1);
    q.randb(bits - 1);
    q.add(q, pow2);
  }

  /** Construct a matrix `[[I,H],[0,qI]]` where `H` is constructed from rotations of a vector ``h``.

     @note The constructed matrix will not come with a guarantee of unusually short vectors.
  */

  void gen_ntrulike(const Z_NR<ZT> &q);

  void gen_ntrulike_bits(int bits)
  {
    Z_NR<ZT> q;
    gen_q(bits, q);
    gen_ntrulike(q);
  }

  /** Construct a matrix ``[[qI,0],[H,I]]`` where ``H`` is constructed from rotations of a vector
    ``h``.

    @note The constructed matrix will not come with a guarantee of unusually short vectors.
  */

  void gen_ntrulike2(const Z_NR<ZT> &q);

  void gen_ntrulike2_bits(int bits)
  {
    Z_NR<ZT> q;
    gen_q(bits, q);
    gen_ntrulike2(q);
  }

  /** Construct a matrix ``[[I,H],[0,Iq]]`` where ``H`` is uniform mod q, of dimensions (n-k) x k.
   */

  void gen_qary(int k, const Z_NR<ZT> &q);

  void gen_qary_bits(int k, int bits)
  {
    Z_NR<ZT> q;
    gen_q(bits, q);
    gen_qary(k, q);
  }

  void gen_qary_prime(int k, int bits)
  {
    Z_NR<ZT> q;
    gen_q(bits, q);
    q.nextprime(q);
    gen_qary(k, q);
  }

  /** Construct a lower triangular matrices with specified diagonal coefficients
and random sub-diagonal coefficients.
  */

  void gen_trg(double alpha);
  void gen_trg2(FP_NR<mpfr_t> *w);
};

/**
 * Converts integer matrices. Conversion is coing through get_si(), so ONLY use for
 * small numbers that fit into longs. More specifically, Afrom is converted to Ato
 * (and true is returned) if all numbers in Afrom fit into
 * numeric_limits<long>::digits - buffer bits. Otherwise, conversion is aborted and
 * false is returned.
 */
template <class ZTto, class ZTfrom>
bool convert(ZZ_mat<ZTto> &Ato, const ZZ_mat<ZTfrom> &Afrom, int buffer = 0);

/** FP_mat is a matrix of floating-point numbers. */
template <class FT> class FP_mat : public Matrix<FP_NR<FT>>
{
public:
  using T = FP_NR<FT>;
  /** Creates an empty matrix (0 x 0). */
  FP_mat() : Matrix<T>() {}
  /** Creates a matrix of dimensions rows x cols. All elements are
      initialized with the default constructor of FP_NR&lt;T&gt;. */
  FP_mat(int rows, int cols) : Matrix<T>(rows, cols) {}
};

}  // namespace fplll

// The inline definitions live in matrix.inl; keep it part of this public header
// for include-cleaner so that including <fplll/nr/matrix.h> also covers the
// free functions implemented there (e.g. convert()).
#include "matrix.inl"  // IWYU pragma: export

#endif
