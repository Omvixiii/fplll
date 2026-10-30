// SPDX-FileCopyrightText: 2005-2008 Damien Stehle
// SPDX-FileCopyrightText: 2007 David Cade
// SPDX-FileCopyrightText: 2011 Xavier Pujol
// SPDX-FileCopyrightText: 2019 Koen de Boer
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef FPLLL_GSO_INTERFACE_H
#define FPLLL_GSO_INTERFACE_H

#include <cstddef>
#include <mutex>
#include <utility>
#include <vector>

#include "../defs.h"
#include "../nr/matrix.h"
#include "../nr/nr.h"

namespace fplll
{

/** Options of the Gram-Schmidt computation.
 *
 * - `int_gram`: compute the coefficients of the Gram matrix with exact integer
 *   arithmetic (type ZT) instead of floating-point arithmetic (type FT). All
 *   coefficients of the known rows are then updated continuously; with
 *   floating-point arithmetic they are computed on demand. Cannot be combined
 *   with `row_expo`.
 * - `row_expo`: normalize each row of the basis by a power of two before
 *   converting it to floating-point, which avoids some overflows. It is
 *   useless for FT=mpfr_t and cannot be combined with `int_gram`.
 * - `force_long`: affects the behaviour of row_addmul(_we); see the
 *   documentation of row_addmul.
 * - `precision`: MPFR precision used by the GSO object. 0 keeps the precision
 *   that is currently configured for FP_NR<mpfr_t>; a positive value is
 *   installed when the object is constructed and restored when the object is
 *   destroyed. Overlapping GSO objects may use different precisions and may be
 *   destroyed in any order: the installed precisions are kept on a process-wide
 *   stack, so destroying the outermost object first keeps the precision of the
 *   objects that are still alive and the last object to be destroyed restores
 *   the default precision from before the first one. A later explicit
 *   FP_NR<mpfr_t>::set_prec() wins over the restoration. Overlapping objects in
 *   different threads are *not* supported, because the MPFR default precision
 *   is process-wide state. Ignored for every floating-point type except
 *   FP_NR<mpfr_t>.
 */
struct GSOOptions
{
  bool int_gram   = false;
  bool row_expo   = false;
  bool force_long = false;
  int precision   = 0;
};

namespace detail
{

/** One precision installed by a ScopedGsoPrecision guard. */
struct GsoPrecisionFrame
{
  const void *owner;       // address of the guard that installed the precision
  unsigned int saved_old;  // default precision in effect before the guard
  unsigned int installed;  // default precision installed by the guard
};

/**
 * Process-wide stack of the precisions installed by the GSO precision guards.
 *
 * The MPFR default precision is process state, so the guards have to cooperate:
 * a guard that is destroyed out of LIFO order cannot simply restore the value it
 * saved, it has to keep the precision of the guards that are still alive in
 * effect and make the last remaining guard restore the value from before the
 * first one.
 */
struct GsoPrecisionStack
{
  std::mutex mutex;
  std::vector<GsoPrecisionFrame> frames;
};

/**
 * Returns the process-wide precision stack.
 *
 * The stack is intentionally leaked instead of being a function-local static:
 * a global or static GSO object in a client translation unit may be destroyed
 * after this library's static data, and a destructor that still has to pop a
 * frame must not touch a destroyed stack.
 */
inline GsoPrecisionStack &gso_precision_stack()
{
  static auto *stack = new GsoPrecisionStack();
  return *stack;
}

/**
 * Installs an MPFR default precision for the lifetime of the guard and
 * restores the previous precision when the guard is destroyed.
 *
 * The guards may be destroyed in any order (see GSOOptions::precision); only
 * overlapping guards in different threads are unsupported, because the MPFR
 * default precision is process-wide state.
 *
 * The primary template is the no-op used by the fixed-precision backends; it is
 * an empty type so that `[[no_unique_address]]` keeps it free of storage.  The
 * FP_NR<mpfr_t> specialization holds the state and does the work.  A precision
 * of 0 (or a negative value) keeps the current default; it is meant for
 * GSOOptions::precision.
 */
template <class FT> class ScopedGsoPrecision
{
public:
  explicit ScopedGsoPrecision([[maybe_unused]] int precision) noexcept {}
  ~ScopedGsoPrecision() noexcept = default;

  ScopedGsoPrecision(const ScopedGsoPrecision &)            = delete;
  ScopedGsoPrecision &operator=(const ScopedGsoPrecision &) = delete;
  ScopedGsoPrecision(ScopedGsoPrecision &&)                 = delete;
  ScopedGsoPrecision &operator=(ScopedGsoPrecision &&)      = delete;
};

/** MPFR backend: sets GSOOptions::precision for the object's lifetime. */
template <> class ScopedGsoPrecision<FP_NR<mpfr_t>>
{
public:
  explicit ScopedGsoPrecision(int precision)
  {
    if (precision > 0)
    {
      installed_               = static_cast<unsigned int>(precision);
      GsoPrecisionStack &stack = gso_precision_stack();
      const std::scoped_lock lock(stack.mutex);
      const unsigned int saved = FP_NR<mpfr_t>::get_prec();
      stack.frames.push_back(
          GsoPrecisionFrame{.owner = this, .saved_old = saved, .installed = installed_});
      FP_NR<mpfr_t>::set_prec(installed_);
    }
  }

  ~ScopedGsoPrecision() noexcept
  {
    if (installed_ == 0)
    {
      return;
    }
    GsoPrecisionStack &stack = gso_precision_stack();
    const std::scoped_lock lock(stack.mutex);
    std::vector<GsoPrecisionFrame> &frames = stack.frames;

    // The guard is normally on top, but the caller may destroy overlapping GSO
    // objects in any order, so look for the frame instead of assuming LIFO.
    std::size_t pos = frames.size();
    while (pos > 0 && frames[pos - 1].owner != static_cast<const void *>(this))
    {
      pos--;
    }
    if (pos == 0)
    {
      return;  // no frame: this guard never installed a precision
    }
    pos--;

    if (pos + 1 < frames.size())
    {
      // Not the innermost guard: the precisions installed afterwards are still
      // in effect. Hand the saved value to the next frame so that the last
      // guard to be destroyed restores the value from before this one.
      frames[pos + 1].saved_old = frames[pos].saved_old;
      frames.erase(frames.begin() + static_cast<std::ptrdiff_t>(pos));
      return;
    }

    // Innermost guard: restore the previous precision, unless the global default
    // was changed behind the guards' back (an explicit change wins).
    if (FP_NR<mpfr_t>::get_prec() == frames[pos].installed)
    {
      FP_NR<mpfr_t>::set_prec(frames[pos].saved_old);
    }
    frames.pop_back();
  }

  ScopedGsoPrecision(const ScopedGsoPrecision &)            = delete;
  ScopedGsoPrecision &operator=(const ScopedGsoPrecision &) = delete;
  ScopedGsoPrecision(ScopedGsoPrecision &&)                 = delete;
  ScopedGsoPrecision &operator=(ScopedGsoPrecision &&)      = delete;

private:
  unsigned int installed_ = 0;
};

}  // namespace detail

/**
   @brief Use the Gaussian Heuristic to compute a bound on the length of the
   shortest vector.

   @param max_dist         output
   @param max_dist_expo    exponent of output
   @param block_size       block size
   @param root_det         root determinant of the lattice
   @param gh_factor        factor by which to multiply the bound

   @return The new bound if `gh_factor * GH` is shorter than `max_dist`;
   `max_dist` is unchanged otherwise.
*/

template <class FT>
void adjust_radius_to_gh_bound(FT &max_dist, long max_dist_expo, int block_size, const FT &root_det,
                               double gh_factor);

/**
 * MatGSOInterface provides an interface for performing elementary operations on a basis
 * and computing its Gram matrix and its Gram-Schmidt orthogonalization.
 * The Gram-Schmidt coefficients are computed on demand. The object keeps track
 * of which coefficients are valid after each row operation.
 *
 * The GSO owns the consistency of its cached data: write operations on the
 * basis (row_add(), row_sub(), row_addmul(), row_addmul_we()) must be grouped in
 * a RowOperations batch (see row_operations()) so that the Gram matrix and the
 * Gram-Schmidt coefficients are invalidated when the batch ends. A debug build
 * (FPLLL_DEBUG_CHECKS) aborts when a write operation is performed outside a batch
 * or on a row that the open batch does not cover; a release build does not
 * check, and such a write silently leaves the cached coefficients stale.
 * Read-only views of the cached coefficients (get_mu_matrix(), get_r_matrix(),
 * get_mu_exp(), get_r_exp()) are invalidated by the next row operation or GSO
 * update. The dimension and the row exponents are read-only through get_d() and
 * get_row_expo().
 */

template <class ZT, class FT> class MatGSOInterface
{
private:
  /* The precision guard must be the first data member: it has to be
     constructed before every FT-valued member so that they are allocated at
     the precision requested by GSOOptions::precision. Its only purpose is the
     side effect of its constructor and destructor. */
  [[maybe_unused]] [[no_unique_address]] detail::ScopedGsoPrecision<FT> precision_guard;

  /**
   * Marks the beginning of a batch of row operations on rows [first, last).
   * Use row_operations() instead of calling this directly.
   */
  void row_op_begin(int first, int last)
  {
#ifdef FPLLL_DEBUG_CHECKS
    FPLLL_DEBUG_CHECK(row_op_first == -1);
    row_op_first = first;
    row_op_last  = last;
#else
    (void)first;
    (void)last;
#endif
  }

  /**
   * Ends a batch of row operations started by row_op_begin() and invalidates
   * the affected GSO data. Use row_operations() instead of calling this
   * directly.
   */
  void row_op_end(int first, int last);

public:
  /**
   * Constructor.
   * The precision of FT can be set with GSOOptions::precision: 0 keeps the
   * precision that is currently configured for FP_NR<mpfr_t>, a positive value
   * is installed for the lifetime of the object and restored afterwards (in any
   * destruction order; see GSOOptions::precision).
   * @param arg_u
   *   If arg_u is not empty, operations on b are also done on u
   *   (in this case both must have the same number of rows).
   *   If u is initially the identity matrix, multiplying transform by the
   *   initial basis gives the current basis.
   *   In other words, u will contain the transformation matrix used on the basis b
   *   that was used to obtain the current basis.
   * @param arg_uinv_t
   *   Inverse transform (should be empty, which disables the computation, or
   *   initialized with identity matrix). It works only if u is not empty.
   * @param options
   *   See GSOOptions.
   */
  MatGSOInterface(Matrix<ZT> &arg_u, Matrix<ZT> &arg_uinv_t, GSOOptions options)
      : precision_guard(options.precision), enable_int_gram(options.int_gram),
        enable_row_expo(options.row_expo), enable_transform(arg_u.get_rows() > 0),
        enable_inverse_transform(arg_uinv_t.get_rows() > 0), row_op_force_long(options.force_long),
        u(arg_u), u_inv_t(arg_uinv_t), gptr(nullptr)
  {
    row_op_first = row_op_last = -1;
  }

  virtual ~MatGSOInterface() noexcept = default;

  MatGSOInterface(const MatGSOInterface &)            = delete;
  MatGSOInterface &operator=(const MatGSOInterface &) = delete;

  /**
   * RAII guard for a batch of row operations on the GSO.
   *
   * The constructor starts the batch (row_op_begin()); the destructor ends it
   * and invalidates every cached coefficient the operations may have changed
   * (row_op_end()). Batches must not be nested; a debug build asserts this.
   * Every write operation (row_add(), row_sub(), row_addmul(), row_addmul_we())
   * on a row of [first, last) must happen while the guard is alive: a debug
   * build (FPLLL_DEBUG_CHECKS) aborts when a write happens outside the batch or
   * on a row the batch does not cover, while a release build does not detect it
   * and the cached coefficients then silently go stale.
   * The guard is movable so that a batch can be handed over to another guard,
   * but it is not copyable: exactly one guard owns a batch.
   */
  class RowOperations
  {
  public:
    RowOperations(MatGSOInterface &gso, int first, int last)
        : gso_(&gso), first_(first), last_(last)
    {
      gso_->row_op_begin(first_, last_);
    }

    ~RowOperations() noexcept
    {
      if (gso_ != nullptr)
      {
        gso_->row_op_end(first_, last_);
      }
    }

    RowOperations(const RowOperations &)            = delete;
    RowOperations &operator=(const RowOperations &) = delete;

    RowOperations(RowOperations &&other) noexcept
        : gso_(std::exchange(other.gso_, nullptr)), first_(other.first_), last_(other.last_)
    {
    }

    RowOperations &operator=(RowOperations &&other) noexcept
    {
      if (this != &other)
      {
        if (gso_ != nullptr)
        {
          gso_->row_op_end(first_, last_);
        }
        gso_   = std::exchange(other.gso_, nullptr);
        first_ = other.first_;
        last_  = other.last_;
      }
      return *this;
    }

  private:
    MatGSOInterface *gso_ = nullptr;
    int first_            = 0;
    int last_             = 0;
  };

  /**
   * Starts a batch of row operations and returns its RAII guard. All row
   * operations on rows in [first, last) must happen while the guard is alive;
   * when the guard goes out of scope, the cached Gram matrix and the cached
   * Gram-Schmidt coefficients are invalidated.
   */
  [[nodiscard]] RowOperations row_operations(int first, int last)
  {
    return RowOperations(*this, first, last);
  }

  /**
   * Number of rows of b (dimension of the lattice).
   * Can be changed with create_row or remove_last_row.
   */
  [[nodiscard]] int get_d() const noexcept { return d; }

  /**
   * Returns || sum_i  x_i b_i ||^2 on vectorial input (x_i)_i
   * in the Gram version, it returns x^T G x
   */
  virtual ZT &sqnorm_coordinates(ZT &sqnorm, const std::vector<ZT> &coordinates) = 0;

  /**
   * Returns maximum exponent of b. In the gram
   * version it returns a half times the
   * maximum exponent of g.
   */
  [[nodiscard]] virtual long get_max_exp_of_b() = 0;

  /** Returns true if the ith row
   *  of b is zero. In the gram version
   * it returns true if g(i,i) is zero.
   */
  [[nodiscard]] virtual bool b_row_is_zero(int i) = 0;

  /** Returns number of columns of b. In
   *  the gram version it returns the number
   *  of columns of g.
   */
  [[nodiscard]] virtual int get_cols_of_b() const = 0;

  /** Returns number of rows of b. In
   *  the gram version it returns the number
   *  of of rows of g. This function is made
   *  to reduce code repetition (dump_mu/dump_r)
   */
  [[nodiscard]] virtual int get_rows_of_b() const = 0;

  /** Negates the ith row of b. Needed
   * by dbkz_postprocessing.
   */
  virtual void negate_row_of_b(int i) = 0;

  /**
   * When enable_row_expo=true, get_row_expo(i) is the smallest non-negative
   * integer such that b(i, j) &lt;= 2^row_expo[i] for all j.
   */
  [[nodiscard]] long get_row_expo(int i) const noexcept { return row_expo[i]; }

  /**
   * Returns true if the rows of b are normalized by powers of two
   * (see GSOOptions::row_expo).
   */
  [[nodiscard]] bool is_enable_row_expo() const noexcept { return enable_row_expo; }

  /**
   * Returns Gram matrix coefficients (0 &lt;= i &lt; n_known_rows and
   * 0 &lt;= j &lt;= i).
   * If enable_row_expo=false, returns the dot product (b[i], b[j]).
   * If enable_row_expo=true, returns
   * (b[i], b[j]) / 2 ^ (row_expo[i] + row_expo[j]).
   *
   * Returns reference to `f`.
   */
  virtual FT &get_gram(FT &f, int i, int j) = 0;

  /**
   * Returns *integer* Gram matrix coefficients (0 &lt;= i &lt; n_known_rows and
   * 0 &lt;= j &lt;= i).
   * If (enable_int_gram = true), it returns the i,j-th coordinate of the Gram matrix
   * else it computes the inner product of b_i and b_j.
   * Returns reference to `z`.
   */
  virtual ZT &get_int_gram(ZT &z, int i, int j) = 0;

  /**
   * Returns the mu matrix
   * Coefficients of the Gram Schmidt Orthogonalization
   * (lower triangular matrix)
   * mu(i, j) = r(i, j) / ||b*_j||^2.
   *
   * The returned reference is invalidated by the next row operation or by the
   * next GSO update.
   */
  [[nodiscard]] const Matrix<FT> &get_mu_matrix() const { return mu; }

  /**
   * Returns the r matrix
   * Coefficients of the Gram Schmidt Orthogonalization
   * (lower triangular matrix)
   *
   * The returned reference is invalidated by the next row operation or by the
   * next GSO update.
   */
  [[nodiscard]] const Matrix<FT> &get_r_matrix() const { return r; }

  /**
   * Returns the g matrix (Z_NR version of r)
   * Coefficients of the Gram Schmidt Orthogonalization
   * (lower triangular matrix)
   */
  [[nodiscard]] const Matrix<ZT> &get_g_matrix() const
  {
    FPLLL_CHECK(gptr != nullptr, "get_g_matrix(): no integer Gram matrix is available");
    return *gptr;
  }

  /**
   * Returns f = mu(i, j) and expo such that
   * f * 2^expo = (b_i, b*_j) / ||b*_j||^2.
   * If enable_row_expo=false, expo is always 0.
   * If enable_row_expo=true, expo = row_expo[i] - row_expo[j]
   * It is assumed that mu(i, j) is valid.
   * The returned reference is invalidated by the next row operation or by the
   * next GSO update.
   */
  [[nodiscard]] inline const FT &get_mu_exp(int i, int j, long &expo) const;
  [[nodiscard]] inline const FT &get_mu_exp(int i, int j) const;

  /**
   * Returns f = (b_i, b*_j) / ||b*_j||^2.
   *
   * Returns reference to `f`.
   */
  inline FT &get_mu(FT &f, int i, int j);

  /**
   * Returns f = r(i, j) and expo such that (b_i, b*_j) = f * 2^expo.
   * If enable_row_expo=false, expo is always 0.
   * If enable_row_expo=true, expo = row_expo[i] + row_expo[j]
   * It is assumed that r(i, j) is valid.
   * The returned reference is invalidated by the next row operation or by the
   * next GSO update.
   */
  [[nodiscard]] inline const FT &get_r_exp(int i, int j, long &expo) const;
  [[nodiscard]] inline const FT &get_r_exp(int i, int j) const;

  /**
   * Returns f = (b_i, b*_j).
   *
   * Returns reference to `f`.
   */
  inline FT &get_r(FT &f, int i, int j);

  /**
   * Returns expo such that mu(i, j) &lt; 2^expo for all j &lt; n_columns.
   * It is assumed that mu(i, j) is valid for all j &lt; n_columns.
   */
  [[nodiscard]] long get_max_mu_exp(int i, int n_columns);

  /**
   * Updates r(i, j) and mu(i, j) if needed for all j in [0, last_j].
   * All coefficients of r and mu above the i-th row in columns
   * [0, min(last_j, i - 1)] must be valid.
   * If i=n_known_rows, n_known_rows is increased by one.
   */
  bool update_gso_row(int i, int last_j);

  /**
   * Updates r(i, j) and mu(i, j) if needed for all j.
   * All coefficients of r and mu above the i-th row in columns
   * [0, min(last_j, i - 1)] must be valid.
   * If i=n_known_rows, n_known_rows is increased by one.
   */
  inline bool update_gso_row(int i);

  /**
   * Updates all GSO coefficients (mu and r).
   */
  inline bool update_gso();

  /**
   * Allows row_addmul(_we) for all rows even if the GSO has never been computed.
   */
  inline void discover_all_rows();

  /**
   * Sets the value of r(i, j). During the execution of LLL, some coefficients
   * are computed by the algorithm. They are set directly to avoid double
   * computation.
   */
  void set_r(int i, int j, FT &f);

  /**
   * Row old_r becomes row new_r and intermediate rows are shifted.
   * If new_r < old_r, then old_r must be < n_known_rows.
   */
  virtual void move_row(int old_r, int new_r) = 0;

  /**
   * b[i] := b[i] + x * b[j].
   * Must be called inside a row_operations() batch (see RowOperations) that
   * covers row i; a debug build aborts otherwise.
   * Special cases |x| &lt;= 1 and |x| &lt;= LONG_MAX are optimized.
   * x should be an integer.
   * If row_op_force_long=true, x is always converted to (2^expo * long) instead
   * of (2^expo * ZT), which is faster if ZT=mpz_t but might lead to a loss of
   * precision (in LLL, more Babai iterations are needed).
   */
  inline void row_addmul(int i, int j, const FT &x);

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
  virtual void row_addmul_we(int i, int j, const FT &x, long expo_add) = 0;

  /**
   * b[i] := b[i] + b[j] (i &gt; j).
   * Must be called inside a row_operations() batch (see RowOperations) that
   * covers row i; a debug build aborts otherwise.
   */
  virtual void row_add(int i, int j) = 0;

  /**
   * b[i] := b[i] - b[j] (i &gt; j).
   * Must be called inside a row_operations() batch (see RowOperations) that
   * covers row i; a debug build aborts otherwise.
   */
  virtual void row_sub(int i, int j) = 0;

  /**
   * Early reduction
   * Allowed when enable_int_gram=false,
   * n_known_cols large enough to compute all the g(i,j)
   */
  void lock_cols();
  void unlock_cols();

  /**
   * Adds a zero row to b (and to u if enable_transform=true). One or several
   * operations can be performed on this row inside a row_operations() batch
   * (see RowOperations).
   * Do not use if enable_inverse_transform=true.
   */
  inline void create_row();
  virtual void create_rows(int n_new_rows) = 0;

  /**
   * Removes the last row of b (and of u if enable_transform=true).
   * Do not use if enable_inverse_transform=true.
   */
  inline void remove_last_row();
  virtual void remove_last_rows(int n_removed_rows) = 0;

  /**
   * Dump the mu matrix to parameter `mu`.
   *
   * New entries are pushed to the end: existing entries are neither overwritten
   * nor cleared.
   *
   * @note No row discovery or update is performed prior to dumping the matrix.
   */
  inline void dump_mu_d(std::vector<double> &mu, int offset = 0, int block_size = -1);

  /**
   * Dump the r vector to parameter `r`.
   *
   * New entries are pushed to the end: existing entries are neither overwritten
   * nor cleared.
   *
   * @note No row discovery or update is performed prior to dumping the vector.
   */
  inline void dump_r_d(std::vector<double> &r, int offset = 0, int block_size = -1);

  /**
     @brief Return slope of the line fitted to the lengths of the vectors from
     `start_row` to `stop_row`.

     The line is found with the method of least squares and minimizes the sum
     of squared errors.

     The slope gives an indication of the quality of the basis.

     @param start_row start row (inclusive)
     @param stop_row  stop row (exclusive)
     @return
  */
  [[nodiscard]] double get_current_slope(int start_row, int stop_row);

  /**
     @brief Return (squared) root determinant of the basis.

     @param start_row start row (inclusive)
     @param end_row   stop row (exclusive)
  */
  [[nodiscard]] FT get_root_det(int start_row, int end_row);

  /**
     @brief Return log of the (squared) determinant of the basis.

     @param start_row start row (inclusive)
     @param end_row   stop row (exclusive)
  */
  [[nodiscard]] FT get_log_det(int start_row, int end_row);

  /**
     @brief Return slide potential of the basis

     @param start_row  start row (inclusive)
     @param end_row    stop row (exclusive)
     @param block_size block size
  */
  [[nodiscard]] FT get_slide_potential(int start_row, int end_row, int block_size);

  /**
   * Update `v` to `w` such that `‖w⋅B - v⋅B^*‖` is small using Babai's nearest plane algorithm.
   *
   * @param v         a vector, both input and output
   * @param start     only consider subbasis starting at start
   * @param dimension only consider dimension vectors or all if -1
   */
  void babai(std::vector<ZT> &v, int start = 0, int dimension = -1);

  /**
   * Return vector `w` such that `‖w⋅B - v⋅B^*‖` is small using Babai's nearest plane algorithm.
   *
   * @param w         a vector
   * @param v         a vector
   * @param start     only consider subbasis starting at start
   * @param dimension only consider dimension vectors or all if -1
   */
  void babai(std::vector<ZT> &w, const std::vector<FT> &v, int start = 0, int dimension = 1);

  /** Exact computation of dot products (i.e. with type ZT instead of FT) */
  const bool enable_int_gram;

  /** Normalization of each row of b by a power of 2. */
  const bool enable_row_expo;

  /** Computation of the transform matrix. */
  const bool enable_transform;

  /**
   * Computation of the inverse transform matrix (transposed).
   * This works only if enable_transform=true.
   * This matrix has very large coefficients, computing it is slow.
   */
  const bool enable_inverse_transform;

  /**
   * Changes the behaviour of row_addmul(_we).
   * See the description of row_addmul.
   */
  const bool row_op_force_long;

  // Made public for the reduction kernels (lll_kernel.cpp, bkz_kernel.cpp).
  /** Replaces the Gram matrix by its symmetrized version. */
  void symmetrize_g();

  // Made public for bkz_kernel.cpp (dbkz_postprocessing)
  // b[i] <-> b[j] (i < j)
  virtual void row_swap(int i, int j) = 0;

protected:
  /**
   * Number of rows of b (dimension of the lattice).
   * Derived classes may change it through create_rows/remove_last_rows; read it
   * through get_d().
   */
  int d;

  /**
   * When enable_row_expo=true, row_expo[i] is the smallest non-negative integer
   * such that b(i, j) &lt;= 2^row_expo[i] for all j; otherwise the array is
   * empty. Read it through get_row_expo().
   */
  std::vector<long> row_expo;

  /** Allocates matrices and arrays whose size depends on d (all but tmp_col_expo).
   * When enable_int_gram=false, initializes bf.
   */
  virtual void size_increased() = 0;

  /* Increases known rows and invalidates the last
   * gram row (gf) when enable_int_gram is false.
   * When enable_int_gram is true, it computes
   * the new inner products for the last gram
   * row (g).
   */
  virtual void discover_row() = 0;

  // Marks mu(i, j) and r(i, j) as invalid for j >= new_valid_cols
  void invalidate_gso_row(int i, int new_valid_cols = 0);
  /*  Upates the i-th row of bf. It does not invalidate anything, so the caller
   * must take into account that it might change row_expo.
   */
  virtual void update_bf(int i) = 0;
  /* Marks g(i, j) for all j <= i (but NOT for j > i) */
  virtual void invalidate_gram_row(int i) = 0;

  // b[i] <- b[i] + x * b[j] (i > j)
  virtual void row_addmul_si(int i, int j, long x) = 0;

  /**
   * Returns the (i, j)-th entry of the integer Gram matrix, reading the lower
   * triangle symmetrically.
   *
   * Internal helper of the integer-Gram row operations: `gptr` is non-null
   * whenever `enable_int_gram` is set (established by the derived constructors),
   * so this is a hot-path accessor with a debug-only check and no throwing
   * edge.  Use get_g_matrix() or symmetrize_g() for the checked public entry
   * points.
   */
  inline ZT &sym_g(int i, int j) noexcept;

  /* Floating-point representation of the basis. It is used when
     enable_int_gram=false. */
  Matrix<FT> bf;

  Matrix<ZT> &u;        // Transform
  Matrix<ZT> &u_inv_t;  // Transposed inverse transform

  // init_row_size[i] = (last non-zero column in the i-th row of b) + 1
  std::vector<int> init_row_size;

  // bf[i], g[i], gf[i], mu[i] and r[i] are invalid for i >= n_known_rows
  int n_known_rows{0};
  int n_source_rows{0};  // Known rows before the beginning of early reduction
  int n_known_cols{0};
  bool cols_locked{false};
  int alloc_dim{0};

  /**
   * Coefficients of the Gram-Schmidt orthogonalization
   * (lower triangular matrix).
   *
   * If enable_row_expo=false,
   * mu(i, j) = (b_i, b*_j) / ||b*_j||^2.
   * If enable_row_expo=true,
   * mu(i, j) = (b_i, b*_j) / ||b*_j||^2  /  2 ^ (row_expo[i] - row_expo[j]).
   *
   * mu(i, j) is valid if 0 &lt;= i &lt; n_known_rows (&lt;= d) and
   * 0 &lt;= j &lt; min(gso_valid_cols[i], i)
   */
  Matrix<FT> mu;

  /**
   * Coefficients of the Gram-Schmidt orthogonalization
   * (lower triangular matrix).
   *
   * If enable_row_expo=false,
   * r(i, j) = (b_i, b*_j).
   * If enable_row_expo=true,
   * r(i, j) = (b_i, b*_j)  /  2 ^ (row_expo[i] + row_expo[j]).
   *
   * r(i, j) is valid if 0 &lt;= i &lt; n_known_rows (&lt;= d) and
   * 0 &lt;= j &lt; gso_valid_cols[i] (&lt;= i + 1).
   */
  Matrix<FT> r;

  /** Gram matrix (dot products of basis vectors, lower triangular matrix).
   * Replaced by a pointer so that MatGSO can point it at its own member `g`
   * while MatGSOGram points it at the user-supplied matrix.
   * g(i, j) is valid if 0 <= i < n_known_rows and j <= i */
  Matrix<ZT> *gptr;

  Matrix<FT> gf;

  /* Number of valid columns of the i-th row of mu and r.
     Valid only for 0 <= i < n_known_rows */
  std::vector<int> gso_valid_cols;

  /* Used by update_gso_row (+ update_gso), get_max_mu_exp and row_addmul_we. */
  FT ftmp1, ftmp2;
  /* Used by row_add, row_sub, row_addmul_si_2exp, row_addmul_2exp and
     indirectly by row_addmul. */
  ZT ztmp1;
  /* Used by row_addmul. */
  ZT ztmp2;
  /* Used by update_bf. */
  std::vector<long> tmp_col_expo;

  /* Used by the internal debug checks: the range of rows touched by the
     current row operation.  Always present so the class layout does not depend
     on the debug_checks configuration. */
  int row_op_first, row_op_last;

  /**
   * Returns true while a row_operations() batch is open.
   *
   * The write operations (row_add(), row_sub(), row_addmul(), row_addmul_we())
   * require it; a debug build checks it with FPLLL_DEBUG_CHECK and aborts
   * otherwise.  Release builds do not maintain the batch bookkeeping and return
   * true, so the write paths pay nothing for the check.
   */
  [[nodiscard]] bool inside_row_operations() const noexcept
  {
#ifdef FPLLL_DEBUG_CHECKS
    return row_op_first != -1;
#else
    return true;
#endif
  }

  /**
   * Returns true when the row i is covered by the open row_operations() batch.
   * Only meaningful in a debug build; the write paths check it together with
   * inside_row_operations().
   */
  [[nodiscard]] bool in_row_op_range(int i) const { return i >= row_op_first && i < row_op_last; }
};

template <class ZT, class FT> inline void MatGSOInterface<ZT, FT>::symmetrize_g()
{
  FPLLL_CHECK(gptr != nullptr, "symmetrize_g(): no integer Gram matrix is available");
  Matrix<ZT> &gr = *gptr;
  for (int i = 0; i < d; i++)
  {
    for (int j = 0; j < d; j++)
    {
      gr(i, j) = sym_g(i, j);
    }
  }
}

template <class ZT, class FT> inline ZT &MatGSOInterface<ZT, FT>::sym_g(int i, int j) noexcept
{
  FPLLL_DEBUG_CHECK(gptr != nullptr);
  Matrix<ZT> &gr = *gptr;
  return (i >= j) ? gr(i, j) : gr(j, i);
}

template <class ZT, class FT>
inline const FT &MatGSOInterface<ZT, FT>::get_mu_exp(int i, int j, long &expo) const
{
  FPLLL_DEBUG_CHECK(i >= 0 && i < n_known_rows && j >= 0 && j < i && j < gso_valid_cols[i] &&
                    !in_row_op_range(i));
  expo = enable_row_expo ? row_expo[i] - row_expo[j] : 0;
  return mu(i, j);
}

template <class ZT, class FT>
inline const FT &MatGSOInterface<ZT, FT>::get_mu_exp(int i, int j) const
{
  FPLLL_DEBUG_CHECK(i >= 0 && i < n_known_rows && j >= 0 && j < i && j < gso_valid_cols[i] &&
                    !in_row_op_range(i));
  return mu(i, j);
}

template <class ZT, class FT> inline FT &MatGSOInterface<ZT, FT>::get_mu(FT &f, int i, int j)
{
  FPLLL_DEBUG_CHECK(i >= 0 && i < n_known_rows && j >= 0 && j < i && j < gso_valid_cols[i] &&
                    !in_row_op_range(i));
  f = mu(i, j);
  if (enable_row_expo)
  {
    f.mul_2si(f, row_expo[i] - row_expo[j]);
  }
  return f;
}

template <class ZT, class FT>
inline const FT &MatGSOInterface<ZT, FT>::get_r_exp(int i, int j, long &expo) const
{
  FPLLL_DEBUG_CHECK(i >= 0 && i < n_known_rows && j >= 0 && j < gso_valid_cols[i] &&
                    !in_row_op_range(i));
  expo = enable_row_expo ? row_expo[i] + row_expo[j] : 0;
  return r(i, j);
}

template <class ZT, class FT>
inline const FT &MatGSOInterface<ZT, FT>::get_r_exp(int i, int j) const
{
  FPLLL_DEBUG_CHECK(i >= 0 && i < n_known_rows && j >= 0 && j < gso_valid_cols[i] &&
                    !in_row_op_range(i));
  return r(i, j);
}

template <class ZT, class FT> inline FT &MatGSOInterface<ZT, FT>::get_r(FT &f, int i, int j)
{
  FPLLL_DEBUG_CHECK(i >= 0 && i < n_known_rows && j >= 0 && j < gso_valid_cols[i] &&
                    !in_row_op_range(i));
  f = r(i, j);
  if (enable_row_expo)
  {
    f.mul_2si(f, row_expo[i] + row_expo[j]);
  }
  return f;
}

template <class ZT, class FT> inline bool MatGSOInterface<ZT, FT>::update_gso_row(int i)
{
  return update_gso_row(i, i);
}

template <class ZT, class FT> inline void MatGSOInterface<ZT, FT>::set_r(int i, int j, FT &f)
{
  FPLLL_DEBUG_CHECK(i >= 0 && i < n_known_rows && gso_valid_cols[i] >= j && j >= 0 &&
                    j < n_source_rows);
  r(i, j) = f;
  if (gso_valid_cols[i] == j)
  {
    gso_valid_cols[i]++;
  }
}

template <class ZT, class FT>
inline void MatGSOInterface<ZT, FT>::row_addmul(int i, int j, const FT &x)
{
  // The non-virtual entry point checks the batch contract itself so that the
  // failure is reported against the public call, not the dispatcher below.
  FPLLL_DEBUG_CHECK(inside_row_operations() && in_row_op_range(i));
  row_addmul_we(i, j, x, 0);
}

template <class ZT, class FT> inline void MatGSOInterface<ZT, FT>::create_row() { create_rows(1); }

template <class ZT, class FT> inline void MatGSOInterface<ZT, FT>::remove_last_row()
{
  remove_last_rows(1);
}

template <class ZT, class FT> inline void MatGSOInterface<ZT, FT>::discover_all_rows()
{
  while (n_known_rows < d)
  {
    discover_row();
  }
}

template <class ZT, class FT> inline bool MatGSOInterface<ZT, FT>::update_gso()
{
  for (int i = 0; i < d; i++)
  {
    if (!update_gso_row(i))
    {
      return false;
    }
  }
  return true;
}

template <class ZT, class FT>
inline void MatGSOInterface<ZT, FT>::dump_mu_d(std::vector<double> &mu_out, int offset,
                                               int block_size)
{
  FT e;
  if (block_size <= 0)
  {
    block_size = get_rows_of_b();
  }

  mu_out.reserve(mu_out.size() + static_cast<std::size_t>(block_size) * block_size);
  for (int i = 0; i < block_size; ++i)
  {
    for (int j = 0; j < block_size; ++j)
    {
      get_mu(e, offset + i, offset + j);
      mu_out.push_back(e.get_d());
    }
  }
}

template <class ZT, class FT>
inline void MatGSOInterface<ZT, FT>::dump_r_d(std::vector<double> &r_out, int offset,
                                              int block_size)
{
  FT e;
  if (block_size <= 0)
  {
    block_size = get_rows_of_b();
  }

  r_out.reserve(r_out.size() + static_cast<std::size_t>(block_size));
  for (int i = 0; i < block_size; ++i)
  {
    get_r(e, offset + i, offset + i);
    r_out.push_back(e.get_d());
  }
}

}  // namespace fplll

#endif
