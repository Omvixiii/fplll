// SPDX-FileCopyrightText: 2015-2017 Martin Albrecht, Leo Ducas
// SPDX-FileCopyrightText: 2018 Shi Bai
//
// SPDX-License-Identifier: LGPL-2.1-or-later
/** \file pruner.h
    Pruning parameters, cost/probability estimates and the pruning optimizer. */

#ifndef FPLLL_PRUNER_H
#define FPLLL_PRUNER_H

#include "../defs.h"
#include "../nr/matrix.h"
#include "../nr/nr.h"
#include "pruning_params.h"  // IWYU pragma: export

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace fplll
{

/** Largest block dimension the pruner supports.  The tabulated constants are
    arrays of this size, so a block dimension must be strictly smaller. */
inline constexpr std::size_t max_pruner_dim = 2047;

/** Linear-descent pruning coefficients: the first `block_size - level`
    coefficients are 1, the remaining ones decrease linearly with slope
    `-1 / block_size`.

    @param block_size dimension of the pruned block
    @param level number of levels in linear descent
*/
[[nodiscard]] PruningParams linear_pruning_params(std::size_t block_size, std::size_t level);

/** Descent algorithm used by the pruning optimizer. */
enum class PrunerStrategy : std::uint8_t
{
  gradient = 0,
  nelder_mead,
};

/** Options of the pruning optimizer.  `run_pruner()` uses every field; the
    lower-level `prune<FT>()` uses the optimization knobs only (`strategy`,
    `start_from_input`, `cvp`, `half`, `single` and `verbose`). */
struct PrunerOptions
{
  /** Floating-point backend used for the GSO computations. */
  FloatType float_type = FloatType::automatic;
  /** Precision when `float_type == FloatType::mpfr`; must be positive then. */
  int precision = 0;
  /** First index of the pruned block. */
  int start = 0;
  /** One past the last index of the pruned block; `std::nullopt` means the
      number of rows. */
  std::optional<int> end;
  /** Preprocessing budget in enumeration nodes. */
  double pre_nodes = 1e6;
  /** Target expected number of solutions; `std::nullopt` keeps the default. */
  std::optional<double> min_probability;
  /** Enumeration radius factor: the radius is min(GH * gh_factor, |b_0^*|). */
  double gh_factor = 1.0;
  /** Descent algorithm. */
  PrunerStrategy strategy = PrunerStrategy::gradient;
  /** Start the descent from the input coefficients instead of a greedy guess. */
  bool start_from_input = false;
  /** Account for CVP symmetry (1 instead of 1/2) in the cost estimate. */
  bool cvp = false;
  /** Optimize even pruning coefficients only (pr_i == pr_{i+1}). */
  bool half = false;
  /** Minimize the single-enumeration cost instead of the repeated cost.
      `run_pruner()` enables this automatically whenever `min_probability` is
      set, so that the requested target is actually reached. */
  bool single = false;
  /** Print the progress of the descent. */
  bool verbose = false;
};

/** Optimizer of pruning parameters.

    The class is templated by a floating-point type used for the internal
    computations.  It implements the numerically optimized pruning strategy of
    [Gama, Nguyen, Regev 2010], following the simplifications of [Chen 2013]:
    the bounds at indices 2i and 2i+1 are kept equal, which allows estimating
    volumes by symbolic integration.

    Inside this code, b, pv and r are stored in reversed order to conform with
    the algorithm description of [Chen 2013]; the load and save methods handle
    the reversal.

    Naming conventions:
    - b is for bound (squared)
    - ipv is for inverse partial volumes (NOT squared)
    - r is for Gram-Schmidt length (squared), automatically renormalized to
      avoid overflowing partial volumes
    - p is for polynomial
    - n is the dimension of the basis to prune
    - d is floor(n / 2); odd n are dealt with by ignoring the first component
*/
template <class FT> class Pruner
{
public:
#if defined(FPLLL_TESTING)
  class TestPruner;
  friend class TestPruner;
#endif

  // Following typedefs are given for readability
  using vec  = std::vector<FT>;  ///< Vectors of dimension n
  using evec = std::vector<FT>;  ///< Vectors of dimension d = n/2
  using poly = std::vector<FT>;  ///< Polynomials of degree at most d

  /** Sets up an empty optimizer, only usable for svp_probability().
      @param dimension dimension of the pruned block */
  explicit Pruner(int dimension, const PrunerOptions &options = {});

  /** Sets up the optimizer for the given basis shape.
      @param enumeration_radius radius of the desired enumeration
      @param preproc_cost cost of preprocessing (additive cost of a retry)
      @param gso_r Gram-Schmidt length (squared) of the enumerated block
      @param target desired target success probability/expected number of solutions
      @param metric optimized objective
      @param options optimizer options */
  Pruner(const FT &enumeration_radius, const FT &preproc_cost, const std::vector<double> &gso_r,
         const FT &target = FT(0.9), PrunerMetric metric = PrunerMetric::probability_of_shortest,
         const PrunerOptions &options = {});

  /** Sets up the optimizer averaging the cost over several bases.
      @param enumeration_radius radius of the desired enumeration
      @param preproc_cost cost of preprocessing (additive cost of a retry)
      @param gso_rs Gram-Schmidt lengths (squared) of the enumerated block over several bases
      @param target desired target success probability/expected number of solutions
      @param metric optimized objective
      @param options optimizer options */
  Pruner(const FT &enumeration_radius, const FT &preproc_cost,
         const std::vector<std::vector<double>> &gso_rs, const FT &target = FT(0.9),
         PrunerMetric metric          = PrunerMetric::probability_of_shortest,
         const PrunerOptions &options = {});

  /** Optimizes the pruning coefficients (dispatches to the fixed-probability or
      varying-probability optimizer depending on the `single` option). */
  void optimize_coefficients(std::vector<double> &pr);

  /** Computes the cost of a single enumeration. */
  [[nodiscard]] double single_enum_cost(const std::vector<double> &pr,
                                        std::vector<double> *detailed_cost = nullptr);

  /** Returns either the SVP probability or the expected number of solutions,
      depending on the metric. */
  [[nodiscard]] double measure_metric(const std::vector<double> &pr);

  /** Gaussian heuristic for the shortest vector length of the loaded basis.
      @note If several bases were loaded, this is the heuristic of the first. */
  [[nodiscard]] FT gaussian_heuristic();

private:
  FT enumeration_radius = FT(1.0);
  FT preproc_cost       = FT(0.0);
  FT target             = FT(0.9);
  PrunerMetric metric   = PrunerMetric::probability_of_shortest;
  PrunerOptions options;

  int n             = 0;      ///< Dimension of the block to be enumerated
  int d             = 0;      ///< Half dimension: most functions use only the half dimension
  bool shape_loaded = false;  ///< Whether a basis shape was loaded
  bool opt_single   = false;  ///< Minimize the single-enumeration cost

  std::vector<FT> min_pruning_coefficients;  ///< Lower bounds used by enforce()

  FT epsilon         = std::pow(2., -7);  ///< Epsilon for numerical differentiation
  FT min_step        = std::pow(2., -6);  ///< Minimal step in a given direction
  FT min_cf_decrease = .995;  ///< Maximal ratio of two consecutive cost factors in the descent
  FT step_factor     = std::pow(2, .5);  ///< Increment factor for steps in a given direction
  FT shell_ratio     = .995;             ///< Shell thickness ratio when evaluating SVP probability
  FT symmetry_factor = .5;               ///< Symmetry in costs: 0.5 for SVP, 1 for CVP

  /* normalized by 1/det^(1/n) */
  vec r;    ///< Gram-Schmidt length (squared, reversed order)
  vec ipv;  ///< Inverse partial volumes of projected sublattices (not squared)
  FT normalization_factor = FT(1.0);  ///< Internal renormalization factor
  FT normalized_radius    = FT(1.0);  ///< Renormalized enumeration radius
  FT logvol               = FT(0.0);

  /** @brief Imports the tabulated constants once over all instances. */
  static void set_tabulated_consts();
  static std::once_flag tabulated_values_flag;                ///< Guards the table initialization
  static std::array<FT, max_pruner_dim> tabulated_factorial;  ///< i! per dimension
  static std::array<FT, max_pruner_dim> tabulated_ball_vol;   ///< Unit-ball volume per dimension

  /** @brief Validates the options and initializes the dimension-dependent state. */
  void initialize();

  /** @brief Loads the shape of a basis from a vector of GSO lengths.
      @param gso_r vector of GSO coefficients
      @param reset_normalization whether to recompute the renormalization factor */
  void load_basis_shape(const std::vector<double> &gso_r, bool reset_normalization = true);

  /** @brief Loads the shapes of several bases; costs are averaged over all of
      them.  Normalization follows the first basis. */
  void load_basis_shapes(const std::vector<std::vector<double>> &gso_rs);

  /** @brief Converts pruning coefficients from external to internal format:
      converts the type, reverses the ordering and selects even positions. */
  void load_coefficients(evec &b, const std::vector<double> &pr);

  /** @brief Converts pruning coefficients from internal to external format:
      converts the type, reverses the ordering and repeats every coefficient. */
  void save_coefficients(std::vector<double> &pr, const evec &b);

  /** @brief Enforces the constraints on pruning coefficients: terminates with 1,
      decreases, and stays clear of 0 for numerical stability.
      @param b input/output
      @param j keeps index j unchanged when possible
      @return whether a constraint was violated */
  bool enforce(vec &b, int j = 0);

  /** @brief Evaluates the polynomial `p` of degree `ld` at `x`. */
  FT eval_poly(int ld, const poly &p, const FT &x);

  /** @brief Integrates the polynomial `p` of degree `ld` in place. */
  void integrate_poly(int ld, poly &p);

  /** @brief Computes the relative volume of a cylinder intersection to the unit
      sphere (of a given subdimension), implicitly renormalized as if b[rd-1] = 1. */
  FT relative_volume(int rd, const evec &b);

  /** @brief Computes the cost of a pruned enumeration.
      @param b pruning bounds
      @param detailed_cost optional storage for the nodes per level
      @return cost in enumeration nodes */
  FT single_enum_cost(const vec &b, std::vector<double> *detailed_cost = nullptr);
  FT single_enum_cost_evec(const evec &b, std::vector<double> *detailed_cost = nullptr);
  FT single_enum_cost_lower(const vec &b, std::vector<double> *detailed_cost = nullptr);
  FT single_enum_cost_upper(const vec &b, std::vector<double> *detailed_cost = nullptr);

  /** @brief Computes the success probability for SVP/CVP of a single enumeration. */
  FT svp_probability(const vec &b);
  FT svp_probability_evec(const evec &b);
  FT svp_probability_lower(const vec &b);
  FT svp_probability_upper(const vec &b);

  /** @brief Computes the expected number of solutions of a single enumeration. */
  FT expected_solutions(const vec &b);
  FT expected_solutions_evec(const evec &b);
  FT expected_solutions_lower(const vec &b);
  FT expected_solutions_upper(const vec &b);

  /** @brief Links to svp_probability() or expected_solutions() depending on the metric. */
  FT measure_metric(const vec &b);

  /** @brief Computes the target function to be optimized: either the cost of a
      single enumeration or the cost of repeating enumeration and preprocessing
      until reaching the target. */
  FT target_function(const evec &b);

  /** @brief Computes the gradient of the target function numerically. */
  void target_function_gradient(const vec &b, vec &res);

  /** @brief Do some preparation work for the optimization process: a greedy
      guess unless starting from the input, and the minimum pruning bounds. */
  void optimize_coefficients_preparation(std::vector<double> &pr);

  /** @brief Runs the optimization using only even coefficients. */
  void optimize_coefficients_evec_core(std::vector<double> &pr);

  /** @brief Runs the optimization using all coefficients. */
  void optimize_coefficients_full_core(std::vector<double> &pr);

  /** @brief Main interface to optimize the overall enumeration cost. */
  void optimize_coefficients_cost_vary_prob(std::vector<double> &pr);

  /** @brief Main interface to optimize the single enumeration cost under a
      fixed success probability. */
  void optimize_coefficients_cost_fixed_prob(std::vector<double> &pr);

  /** @brief Greedy heuristic producing a flat enumeration tree.
      @note Also used to lower-bound the pruning coefficients during the descent. */
  void greedy(evec &b);

  /** @brief Performs one step of the gradient descent, in place. */
  int gradient_descent_step(vec &b);

  /** @brief Performs several steps of the gradient descent, in place. */
  int gradient_descent(vec &b);

  /** @brief Runs one Nelder-Mead optimization round.
      @return whether progress was made */
  int nelder_mead_step(evec &b);

  /** @brief Tunes the coefficients to reduce the single-enumeration time. */
  void optimize_coefficients_local_adjust_decr_single(std::vector<double> &pr);

  /** @brief Tunes the coefficients to increase the success probability. */
  void optimize_coefficients_local_adjust_incr_prob(std::vector<double> &pr);

  /** @brief Smooths discontinuities between consecutive coefficients. */
  void optimize_coefficients_local_adjust_smooth(std::vector<double> &pr);

  /** @brief Increases the success probability towards the target (may be
      impossible if the radius is too small). */
  void optimize_coefficients_incr_prob(std::vector<double> &pr);

  /** @brief Decreases the success probability towards the target. */
  void optimize_coefficients_decr_prob(std::vector<double> &pr);

  /** @brief Fine-tunes the success probability until it is close to the target. */
  void optimize_coefficients_local_adjust_prob(std::vector<double> &pr);
};

// The supported backends are instantiated by the library.  This keeps the
// tabulated constants (static data members) in one object file and lets the
// white-box tests in tests/pruner read them without instantiating the class.
extern template class Pruner<FP_NR<double>>;
extern template class Pruner<FP_NR<mpfr_t>>;
#ifdef FPLLL_WITH_QD
extern template class Pruner<FP_NR<dd_real>>;
extern template class Pruner<FP_NR<qd_real>>;
#endif

/** Runs the pruning optimizer on the basis `b` and prints the result to
    `log_stream()`.

    When `options.min_probability` is set, the optimizer fixes the expected
    number of solutions to that target (the historical behaviour of the
    high-level entry point).  Otherwise it minimizes the repeated-enumeration
    cost with the default target of 0.9 expected solutions.

    @param b basis of the lattice to be pruned
    @param options optimizer options; `precision` is required for MPFR
    @return `RedStatus::success`
*/
[[nodiscard]] RedStatus run_pruner(ZZ_mat<mpz_t> &b, const PrunerOptions &options = {});

/** Searches for optimal pruning parameters.

    Calling M the metric function (success probability or expected number of
    solutions), C the cost function of the enumeration and P the preprocessing
    cost, this function essentially minimizes (C(p) + P) / M(p) over pruning
    vectors p.  The pruning vector is expressed as relative (squared) lengths
    with respect to the enumeration radius: its entries are in (0, 1) and are
    decreasing.

    @param pruning output; also the starting point when `start_from_input` is set
    @param enumeration_radius radius of the desired enumeration
    @param preproc_cost cost of preprocessing (i.e. additive cost for retrying an enumeration)
    @param gso_r Gram-Schmidt length (squared) of the enumerated block
    @param target desired target success probability/expected solutions after all retries
    @param metric optimized objective
    @param options optimizer options
*/
template <class FT>
void prune(PruningParams &pruning, double enumeration_radius, double preproc_cost,
           const std::vector<double> &gso_r, double target = 0.9,
           PrunerMetric metric          = PrunerMetric::probability_of_shortest,
           const PrunerOptions &options = {});

/** Same as prune(), but averaging the cost over several bases (without
    slowing down the search).

    @param pruning output; also the starting point when `start_from_input` is set
    @param enumeration_radius radius of the desired enumeration
    @param preproc_cost cost of preprocessing (i.e. additive cost for retrying an enumeration)
    @param gso_rs Gram-Schmidt lengths (squared) of the enumerated block over several bases
    @param target desired target success probability/expected solutions after all retries
    @param metric optimized objective
    @param options optimizer options
*/
template <class FT>
void prune(PruningParams &pruning, double enumeration_radius, double preproc_cost,
           const std::vector<std::vector<double>> &gso_rs, double target = 0.9,
           PrunerMetric metric          = PrunerMetric::probability_of_shortest,
           const PrunerOptions &options = {});

/** Probability that a pruned enumeration returns the shortest vector (or the
    expected number of solutions, depending on `pruning.metric`).

    Only the coefficient field of `pruning` is used; the coefficients must
    start with two 1s, be decreasing and lie in (0, 1).
*/
template <class FT> FT svp_probability(const PruningParams &pruning);

/** Probability that a pruned enumeration returns the shortest vector.

    @param pr pruning bounds (must start with two 1s, be decreasing and lie in (0, 1))
*/
template <class FT> FT svp_probability(const std::vector<double> &pr);

/**
    Enforces (and possibly fixes) the pruning coefficients for half/full
    coefficients.
*/
template <class FT> inline bool Pruner<FT>::enforce(/*io*/ vec &b, const int j)
{
  const std::size_t dn = b.size();
  if (dn == 0)
  {
    return false;
  }
  const std::size_t c = std::cmp_equal(dn, d) ? 1 : 2;
  bool status         = false;

  // the last coefficient must be 1
  if ((b[dn - 1] < .999) && (static_cast<std::size_t>(j) != dn - 1))
  {
    status    = true;
    b[dn - 1] = 1.;
  }

  for (std::size_t i = 0; i < dn; ++i)
  {
    status |= (b[i] > 1.0001);
    b[i] = b[i] > 1 ? 1. : b[i];

    // note min_pruning_coefficients always has length d
    if (i / c < static_cast<std::size_t>(d) && b[i] <= min_pruning_coefficients[i / c])
    {
      b[i] = min_pruning_coefficients[i / c];
    }
  }

  for (auto i = static_cast<std::size_t>(j); i + 1 < dn; ++i)
  {
    if (b[i + 1] < b[i])
    {
      status |= (b[i + 1] + .000001 < b[i]);
      b[i + 1] = b[i];
    }
  }

  for (int i = std::min(j - 1, static_cast<int>(dn) - 2); i >= 0; --i)
  {
    if (b[i + 1] < b[i])
    {
      status |= (b[i + 1] + .000001 < b[i]);
      b[i] = b[i + 1];
    }
  }
  return status;
}

// Kept in a separate file: these helpers are inline but belong to the volume
// computation rather than to the class interface.
#include "pruner_simplex.inl"  // IWYU pragma: export

}  // namespace fplll

#endif /* FPLLL_PRUNER_H */
