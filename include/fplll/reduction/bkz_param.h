#ifndef FPLLL_BKZ_PARAM_H
#define FPLLL_BKZ_PARAM_H

// SPDX-FileCopyrightText: 2014-2016 Martin Albrecht
// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "../defs.h"
#include "../pruner/pruning_params.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace fplll
{

/**
   A strategy covers pruning parameters and preprocessing block sizes.
*/

class Strategy
{
public:
  std::size_t block_size = 0;                          //< block size
  std::vector<PruningParams> pruning_parameters;       //< pruning parameters
  std::vector<std::size_t> preprocessing_block_sizes;  //< for each block size we run one tour

  /** Select the pruning parameters whose Gaussian-heuristic factor is
      closest to the ratio `radius / gh`.

     @param radius radius of the currently shortest vector
     @param gh Gaussian heuristic prediction for radius
   */

  [[nodiscard]] const PruningParams &get_pruning(double radius, double gh) const;
};

/** Block-reduction variants. */
enum class BKZVariant : std::uint8_t
{
  /** Standard BKZ. */
  bkz = 0,
  /** Dual BKZ: alternate primal and dual tours. */
  sd_bkz,
  /** Slide reduction. */
  slide,
  /** HKZ reduction of the whole basis. */
  hkz,
};

/**
   Parameters of a BKZ reduction.

   This is a plain aggregate: it does not validate its fields. `bkz_reduction`
   validates them before it modifies the basis and throws `fplll::Error` when
   they are inconsistent (block size too small, strategy table too short,
   variant-specific constraints, unsupported floating-point backend).

   An empty `strategies` vector disables both pruning and recursive
   preprocessing; a non-empty vector must cover every block size up to
   `block_size`.
*/

struct BKZParam
{
  /** Block size used for the enumeration. */
  int block_size = 0;

  /** Strategies (pruning parameters, preprocessing block sizes). */
  std::vector<Strategy> strategies;

  /** LLL parameter delta used before and between the tours. */
  double delta = LLL_DEF_DELTA;

  /** Block-reduction variant. */
  BKZVariant variant = BKZVariant::bkz;

  /** Print progress information to log_stream(). */
  bool verbose = false;

  /** Do not run LLL before the block reduction (use at your own risk). */
  bool no_lll = false;

  /** Only run LLL on the current block during SVP preprocessing. */
  bool bounded_lll = false;

  /** Heuristically stop the reduction when the slope stalls. */
  bool auto_abort = false;

  /** Abort when the slope has not decreased by this factor for
      `auto_abort_max_no_dec` tours. */
  double auto_abort_scale = BKZ_DEF_AUTO_ABORT_SCALE;

  /** Number of consecutive tours without a slope decrease before aborting. */
  int auto_abort_max_no_dec = BKZ_DEF_AUTO_ABORT_MAX_NO_DEC;

  /** Enumeration bound in multiples of the Gaussian heuristic. An unset
      optional disables the Gaussian-heuristic bound. */
  std::optional<double> gh_factor;

  /** Maximum number of tours. An unset optional disables the limit. */
  std::optional<int> max_loops;

  /** Maximum process CPU time spent in the block reduction.  The budget is
      checked at tour boundaries, so it is a soft limit: a single tour that
      has already started runs to completion.  An unset optional disables the
      limit. */
  std::optional<std::chrono::milliseconds> max_time;

  /** Minimum success probability of an SVP reduction when using pruning. */
  double min_success_probability = BKZ_DEF_MIN_SUCCESS_PROBABILITY;

  /** Density of the rerandomization operation when using extreme pruning. */
  int rerandomization_density = BKZ_DEF_RERANDOMIZATION_DENSITY;

  /** Floating-point backend.  `automatic` runs at `precision` when it is set
      (MPFR at that precision, as in LLL), and otherwise at double precision;
      an explicit precision selects MPFR. */
  FloatType float_type = FloatType::automatic;

  /** MPFR precision.  A non-zero value selects the MPFR backend at that
      precision, both with `automatic` and with an explicit
      `float_type == FloatType::mpfr`.  0 means "not specified": with
      `automatic` the run stays in double precision, and with an explicit
      `float_type == FloatType::mpfr` the current process default MPFR
      precision is used. */
  int precision = 0;
};

/**
   Load BKZ pruning and preprocessing strategies from a JSON file.

   The file is opened as given. A bare name that does not exist as a path is
   additionally looked up in the compiled-in strategy directory. All parameters
   of the file except pruning and preprocessing are ignored.

   @throws fplll::Error if the file cannot be opened or violates the expected
   JSON schema.
*/

std::vector<Strategy> load_strategies_json(std::string_view filename);

}  // namespace fplll

#endif /* FPLLL_BKZ_PARAM_H */
