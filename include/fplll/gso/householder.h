// SPDX-FileCopyrightText: 2005-2008 Damien Stehle
// SPDX-FileCopyrightText: 2007 David Cade
// SPDX-FileCopyrightText: 2011 Xavier Pujol
// SPDX-FileCopyrightText: 2017-2018 Laurent Grémy
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef FPLLL_GSO_HOUSEHOLDER_H
#define FPLLL_GSO_HOUSEHOLDER_H

#include <algorithm>
#include <vector>

#include "../defs.h"
#include "../nr/matrix.h"
#include "gso_interface.h"

namespace fplll
{

/** Options of the Householder R-factor computation.
 *
 * - `row_expo`: normalize each row of the basis by a power of two before
 *   converting it to floating-point, which avoids some overflows. It is
 *   mandatory (and automatic) with floating-point types whose exponent range is
 *   limited, and useless for FT=mpfr_t.
 * - `force_long`: affects the behaviour of row_addmul(_we); see the
 *   documentation of row_addmul.
 * - `precision`: MPFR precision used by the Householder object. 0 keeps the
 *   precision that is currently configured for FP_NR<mpfr_t>; a positive value
 *   is installed when the object is constructed and restored when the object is
 *   destroyed. Overlapping objects may use different precisions and may be
 *   destroyed in any order (see GSOOptions::precision); overlapping objects in
 *   different threads are not supported, because the MPFR default precision is
 *   process-wide state. Ignored for every floating-point type except
 *   FP_NR<mpfr_t>.
 */
struct HouseholderOptions
{
  bool row_expo   = false;
  bool force_long = false;
  int precision   = 0;
};

/**
 * MatHouseholder provides an interface for performing elementary operations on a basis
 * and computing its R matrix using Householder transformations.
 *
 * The row operations on the basis (row_add(), row_sub(), row_addmul_we() and the
 * specializations they dispatch to) are private: only size_reduce() may perform
 * them, because it is the only place that keeps R consistent with b afterwards.
 * A debug build (FPLLL_DEBUG_CHECKS) aborts when one of the write helpers runs
 * outside size_reduce(); HLLL goes through size_reduce() and is not affected.
 */
template <class ZT, class FT> class MatHouseholder
{
public:
  /**
   * Constructor.
   * The precision of FT can be set with HouseholderOptions::precision: 0 keeps
   * the precision that is currently configured for FP_NR<mpfr_t>, a positive
   * value is installed for the lifetime of the object and restored afterwards
   * (in any destruction order; see GSOOptions::precision).
   * @param arg_b  the matrix on which row operations are performed; it must not
   *               be empty
   * @param arg_u  transformation matrix (see MatGSOInterface)
   * @param arg_uinv_t  transposed inverse transformation matrix (see
   *                    MatGSOInterface)
   * @param options  see HouseholderOptions
   */
  MatHouseholder(Matrix<ZT> &arg_b, Matrix<ZT> &arg_u, Matrix<ZT> &arg_uinv_t,
                 HouseholderOptions options = {})
      : precision_guard(options.precision), b(arg_b), enable_row_expo(options.row_expo),
        enable_transform(arg_u.get_rows() > 0), u(arg_u),
        enable_inverse_transform(arg_uinv_t.get_rows() > 0), u_inv_t(arg_uinv_t),
        row_op_force_long(options.force_long)
  {
    // Get the dimensions of the lattice
    d = b.get_rows();
    n = b.get_cols();

    // No row is known yet
    n_known_rows = 0;
    n_known_cols = 0;

    // Initialize sigma and V used to compute R
    sigma.resize(d);
    R.resize(d, n);
    // TODO: V does not need to be a matrix, since V is a list of vector of different length
    V.resize(d, n);

    // Initialize bf
    bf.resize(d, n);

    // Initialize row_expo.
    row_expo.resize(d);
    std::ranges::fill(row_expo, 0);
    // Initialize row_size
    init_row_size.resize(d);
    for (int i = 0; i < d; i++)
    {
      // Capture the shape of b
      init_row_size[i] = std::max(b[i].size_nz(), 1);
    }

    // Initialize R_history and update_R
    R_history.resize(d);
    for (int i = 0; i < d; i++)
    {
      R_history[i].resize(n);
      for (int j = 0; j < n; j++)
      {
        R_history[i][j].resize(n);
      }
    }
    updated_R = false;

    // Initialize norm_square_b
    norm_square_b.resize(d);
    expo_norm_square_b.resize(d);
    /* fill row_expo with 0, since if enable_row_expo, it will be filled by real value, and
     * otherwise, we essentially 0 - 0 */
    std::ranges::fill(expo_norm_square_b, 0);

    if (enable_row_expo)
    {
      // Initialize tmp_col_expo
      tmp_col_expo.resize(n);
    }

#ifdef FPLLL_DEBUG_CHECKS
    col_kept.resize(d);
    std::ranges::fill(col_kept, false);
#endif  // FPLLL_DEBUG_CHECKS

    /* Initialize values for naively part of the computation
     * Used in is_hlll_reduced at least */
    n_known_rows_naively = 0;
    sigma_naively.resize(d);
    R_naively.resize(d, n);
    V_naively.resize(d, n);
    row_expo_naively.resize(d);
    std::ranges::fill(row_expo_naively, 0);

#ifdef FPLLL_DEBUG_CHECKS
    for (int i = 0; i < d; i++)
    {
      for (int j = 0; j < n; j++)
      {
        V(i, j).set_nan();
        V_naively(i, j).set_nan();
      }
    }
#endif  // FPLLL_DEBUG_CHECKS
  }

  ~MatHouseholder() noexcept = default;

  MatHouseholder(const MatHouseholder &)            = delete;
  MatHouseholder &operator=(const MatHouseholder &) = delete;

  /**
   * Returns f (* 2^expo if enable_row_expo) = R(i, j).
   *
   * Returns reference to `f` and expo.
   */
  inline void get_R(FT &f, int i, int j, long &expo);

  /**
   * Returns f (* 2^expo if enable_row_expo) = R(i, j).
   *
   * Returns reference to `f`.
   */
  inline void get_R(FT &f, int i, int j);

  /**
   * Returns the R matrix
   * expo is set to row_expo
   */
  [[nodiscard]] const Matrix<FT> &get_R(std::vector<long> &expo) const
  {
    expo = row_expo;
    return R;
  }

  /**
   * Apply Householder transformation on row i for columns [0, i).
   * If last_j, apply Householder transformation on row i, from cols [0, i].
   */
  void update_R(int i, bool last_j);

  /**
   * Apply Householder transformation on row i.
   */
  void update_R(int i);

  /**
   * Finalize Householder transformation on row i (especially after update_R(i, false))
   */
  void update_R_last(int i);

  /**
   * Full computation of the matrix R.
   */
  inline void update_R();

  /**
   * Return the dimensions of the lattice
   */
  [[nodiscard]] int get_d() const noexcept { return d; }
  [[nodiscard]] int get_n() const noexcept { return n; }

  /**
   * Compute the squared norm of b[k].
   */
  inline void norm_square_b_row(FT &f, int k, long &expo);

  /**
   * Truncated squared norm of R[k], with coefficients of R[k][beg..end-1].
   */
  inline void norm_square_R_row(FT &f, int k, int beg, int end, long &expo);

  /**
   * Truncated norm of R[k], with coefficients of R[k][beg..end-1].
   */
  inline void norm_R_row(FT &f, int k, int beg, int end, long &expo);

  /**
   * Size reduce b[k] with b[size_reduction_start..size_reduction_end-1].
   *
   * This is the only function that may perform row operations on b: after the
   * writes it invalidates the affected row of R. A debug build (FPLLL_DEBUG_CHECKS)
   * aborts if one of the private write helpers runs outside size_reduce().
   *
   * Not necessary to make transformation on bf, since basically, after each size_reduce, we call
   * refresh_R_bf, which set bf to the correct value from b directly.
   */
  bool size_reduce(int k, int size_reduction_end, int size_reduction_start = 0);

  /**
   * Swap row i and j of b, bf, R, V, u and u_inv_t
   * Swap element i and j in sigma, row_expo, norm_square_b, expo_norm_square_b, init_row_size and
   * R_history.
   */
  void swap(int i, int j);

  /**
   * Update n_known_rows to k.
   */
  inline void invalidate_row(int k);

  /**
   * Return values enable_row_expo
   */
  [[nodiscard]] bool is_enable_row_expo() const noexcept { return enable_row_expo; }

  /**
   * Set updated_R to false
   * updated_R is set to true in recover_R
   */
  void set_updated_R_false() noexcept { updated_R = false; }

  /*
   * Recover R[i] from the precomputed values of R stored in R_history, i.e., R[i] is correct for
   * the coefficients [0, i) and a call to update_R_last(i) will fully compute R[i].
   */
  inline void recover_R(int i);

  /**
   * Return row_expo[i]
   */
  [[nodiscard]] long get_row_expo(int i) const noexcept { return row_expo[i]; }

  /**
   * Returns the value of row_op_force_long
   */
  [[nodiscard]] bool is_row_op_force_long() const noexcept { return row_op_force_long; }

  /**
   * Set bf[i] and R[i] to b[i].
   * Precompute square norm of b[i].
   */
  void refresh_R_bf(int i);
  /**
   * Set bf and R to b.
   * Precompute squared norm of all the vectors of b
   */
  inline void refresh_R_bf();

  /**
   * Set R[i] to b[i].
   */
  void refresh_R(int i);

  /**
   * Set in f the precomputed squared norm of b[i] and in expo the exponent such that ||b[i]||^2 = f
   * * 2^{expo}
   */
  inline void get_norm_square_b(FT &f, int i, long &expo);

private:
  /* Must be the first data member so that every FT-valued member below is
     constructed at the precision requested by HouseholderOptions::precision.
     Only the side effects of its constructor and destructor matter. */
  [[maybe_unused]] [[no_unique_address]] detail::ScopedGsoPrecision<FT> precision_guard;

  /**
   * Number of rows of b (dimension of the lattice).
   */
  int d;

  /**
   * Number of columns of b.
   */
  int n;

  /**
   * Basis of the lattice
   */
  Matrix<ZT> &b;

  /**
   * b = R * q_householder (q_householder is not computed)
   * R is lower triangular and the diagonal coefficients are >= 0.
   */
  Matrix<FT> R;

  /**
   * Vector v following [MSV, ISSAC'09].
   */
  Matrix<FT> V;

  /**
   * Sigma values following [MSV, ISSAC'09].
   */
  std::vector<FT> sigma;

  /**
   * R[i] is invalid for i >= n_known_rows.
   */
  int n_known_rows;

  /** Normalization of each row of b by a power of 2. */
  const bool enable_row_expo;

  /**
   * When enable_row_expo=true, row_expo[i] is the smallest non-negative integer
   * such that b(i, j) &lt;= 2^row_expo[i] for all j. Otherwise this array is empty.
   */
  std::vector<long> row_expo;

  /* Used by update_R. Temporary variable. */
  std::vector<long> tmp_col_expo;

  // Temporary variables
  FT ftmp0, ftmp1, ftmp2, ftmp3;
  ZT ztmp0, ztmp1;

  // init_row_size[i] = (last non-zero column in the i-th row of b) + 1
  std::vector<int> init_row_size;
  // n_known_cols (last non-zero column of the discovered rows) + 1
  int n_known_cols;

  /**
   * b[i] := b[i] + x * 2^expo_add * b[j].
   * Only size_reduce() may call this (and its specializations below); a debug
   * build aborts otherwise.
   * Special cases |x| &lt;= 1 and |x| &lt;= LONG_MAX are optimized.
   * x should be a non-zero FT. x*2^expo_add must represent an integer (typically coming from
   * rnd_we).
   * If row_op_force_long=true, x is always converted to (2^expo * long) instead
   * of (2^expo * ZT), which is faster if ZT=mpz_t but might lead to a loss of
   * precision.
   */
  void row_addmul_we(int i, int j, const FT &x, long expo_add);
  /**
   * Special cases of row_addmul_we; they share its precondition.
   */
  void row_add(int i, int j);
  void row_sub(int i, int j);
  void row_addmul_si(int i, int j, long x);
  void row_addmul_si_2exp(int i, int j, long x, long expo);
  void row_addmul_2exp(int i, int j, const ZT &x, long expo);

  /* Used by the internal debug checks: true while size_reduce() may perform row
     operations. Always present so the class layout does not depend on the
     debug_checks configuration. */
  bool in_size_reduction = false;

  /**
   * Basis of the lattice (floating point)
   */
  Matrix<FT> bf;

  /**
   * R_history stores the history of the computation of R
   * Example: R[i][j][k] is the index k of R[i] when the coefficient j was known
   */
  std::vector<std::vector<std::vector<FT>>> R_history;

  // If updated_R, R[n_known_rows][0] to R[n_known_rows][n_known_rows - 1] is valid
  bool updated_R;

  // Compute the unimodular matrix u
  const bool enable_transform;

  Matrix<ZT> &u;  // Transform

  /**
   * Computation of the inverse transform matrix (transposed).
   * This works only if enable_transform=true.
   * This matrix has very large coefficients, computing it is slow.
   */
  const bool enable_inverse_transform;

  Matrix<ZT> &u_inv_t;  // Transposed inverse transform

  /**
   * Changes the behaviour of row_addmul(_we).
   * See the description of row_addmul.
   */
  const bool row_op_force_long;

  // Store the approximate norm of b[i].
  std::vector<FT> norm_square_b;
  // If enable_row_expo, ||b[i]||^2 = norm_square_b[i] * 2^{expo_norm_square_b}
  std::vector<long> expo_norm_square_b;

  // As in hplll. This variable is maintained only by the internal debug checks.
  // If col_kept[k] = true, b[k] has not changed since last time we use it.
  std::vector<bool> col_kept;

  /* Objects and methods for the naive computation of the R factor using Householder. */

public:
  /**
   * Full computation of the matrix R.
   */
  void update_R_naively();

  /**
   * Apply Householder transformation on row i.
   */
  void update_R_naively(int i);

  /**
   * Return R_naively(i, j) = f (* 2^expo, if enable_row_expo)
   */
  inline void get_R_naively(FT &f, int i, int j, long &expo);

private:
  /**
   * b = R * q_householder.
   * R is lower triangular and the diagonal coefficients are >= 0.
   */
  Matrix<FT> R_naively;

  /**
   * Vector v following [MSV, ISSAC'09].
   */
  Matrix<FT> V_naively;

  /**
   * Sigma values following [MSV, ISSAC'09].
   */
  std::vector<FT> sigma_naively;

  /**
   * When enable_row_expo=true, row_expo_naively[i] is the smallest non-negative integer
   * such that b(i, j) &lt;= 2^row_expo_naively[i] for all j. Otherwise this array is empty.
   */
  std::vector<long> row_expo_naively;

  /**
   * R[i] is invalid for i >= n_known_rows_naively.
   */
  int n_known_rows_naively;
};

template <class ZT, class FT>
inline void MatHouseholder<ZT, FT>::get_R(FT &f, int i, int j, long &expo)
{
  get_R(f, i, j);
  expo = row_expo[i];
}

template <class ZT, class FT> inline void MatHouseholder<ZT, FT>::get_R(FT &f, int i, int j)
{
  FPLLL_DEBUG_CHECK(i >= 0 && i < d && j >= 0 && j <= i);
  f = R(i, j);
}

template <class ZT, class FT> inline void MatHouseholder<ZT, FT>::update_R(int i)
{
  // last_j = true since we want the full computation of R[i]
  update_R(i, true);
}

template <class ZT, class FT> inline void MatHouseholder<ZT, FT>::update_R()
{
  for (int i = 0; i < d; i++)
  {
    update_R(i);
  }
}

template <class ZT, class FT>
inline void MatHouseholder<ZT, FT>::norm_square_b_row(FT &f, int k, long &expo)
{
  FPLLL_DEBUG_CHECK(k >= 0 && k < d);

  // TODO: maybe can be only dot_product(f, bf[k], n_known_rows). However, since the
  // FPLLL_DEBUG_CHECK is more constraint, fall back to this version of dot_product
  bf[k].dot_product(f, bf[k], 0, n_known_cols);

  expo = enable_row_expo ? 2 * row_expo[k] : 0;
}

template <class ZT, class FT>
inline void MatHouseholder<ZT, FT>::norm_square_R_row(FT &f, int k, int beg, int end, long &expo)
{
  FPLLL_DEBUG_CHECK(k >= 0 && k < d);
  FPLLL_DEBUG_CHECK(beg <= end);
  FPLLL_DEBUG_CHECK(end <= k);
  if (end == beg)
  {
    f = 0.0;
  }
  else
  {
    R[k].dot_product(f, R[k], beg, end);
  }

  expo = enable_row_expo ? 2 * row_expo[k] : 0;
}

template <class ZT, class FT>
inline void MatHouseholder<ZT, FT>::norm_R_row(FT &f, int k, int beg, int end, long &expo)
{
  FPLLL_DEBUG_CHECK(k >= 0 && k < d);
  FPLLL_DEBUG_CHECK(beg <= end);
  if (end == beg)
  {
    f = 0.0;
  }
  else
  {
    R[k].dot_product(f, R[k], beg, end);
    f.sqrt(f);
  }

  expo = enable_row_expo ? row_expo[k] : 0;
}

template <class ZT, class FT> inline void MatHouseholder<ZT, FT>::invalidate_row(int k)
{
  if (k < n_known_rows)
  {
    n_known_rows = k;
  }
}

template <class ZT, class FT> inline void MatHouseholder<ZT, FT>::recover_R(int i)
{
  FPLLL_DEBUG_CHECK(col_kept[i] == true);

  // TODO: these are row operations.
  for (int k = 0; k < i - 1; k++)
  {
    R(i, k) = R_history[i][k][k];
  }
  for (int k = i - 1; k < n; k++)
  {
    R(i, k) = R_history[i][i - 1][k];
  }

  updated_R = true;
}

template <class ZT, class FT> inline void MatHouseholder<ZT, FT>::refresh_R_bf()
{
  for (int i = 0; i < d; i++)
  {
    refresh_R_bf(i);
  }
}

template <class ZT, class FT>
inline void MatHouseholder<ZT, FT>::get_norm_square_b(FT &f, int i, long &expo)
{
  FPLLL_DEBUG_CHECK(i >= 0 && i < d);
  expo = expo_norm_square_b[i];
  f    = norm_square_b[i];
}

/* Objects and methods for the naive computation of the R factor using Householder. */

template <class ZT, class FT> inline void MatHouseholder<ZT, FT>::update_R_naively()
{
  for (int i = 0; i < d; i++)
  {
    update_R_naively(i);
  }
}

template <class ZT, class FT>
inline void MatHouseholder<ZT, FT>::get_R_naively(FT &f, int i, int j, long &expo)
{
  FPLLL_DEBUG_CHECK(i >= 0 && i < d && j >= 0 && j <= i);
  f    = R_naively(i, j);
  expo = row_expo_naively[i];
}

}  // namespace fplll

#endif  // FPLLL_GSO_HOUSEHOLDER_H
