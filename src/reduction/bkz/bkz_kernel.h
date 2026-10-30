// SPDX-FileCopyrightText: 2011 Xavier Pujol
// SPDX-FileCopyrightText: 2014-2016 Martin R. Albrecht
// SPDX-FileCopyrightText: 2016 Michael Walter
// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef FPLLL_REDUCTION_BKZ_KERNEL_H
#define FPLLL_REDUCTION_BKZ_KERNEL_H

#include "../lll/lll_kernel.h"

#include <fplll/defs.h>
#include <fplll/enum/evaluator.h>
#include <fplll/gso/gso_interface.h>
#include <fplll/pruner/pruner.h>
#include <fplll/reduction/bkz_param.h>

#include <chrono>
#include <format>
#include <iosfwd>
#include <limits>
#include <vector>

namespace fplll
{

/**
 * Parameters that drive a single block-reduction tour.
 *
 * The public `BKZParam` owns the strategy table.  Tours only need to know where
 * it is, plus the options that a recursive preprocessing tour overrides, so
 * this lightweight view is threaded through `BKZReduction` instead of copying
 * the (potentially large) strategy table for every preprocessing block.
 */
struct BKZRunParameters
{
  int block_size = 0;
  /** Strategy table; null or empty disables pruning and preprocessing. */
  const std::vector<Strategy> *strategies = nullptr;
  bool verbose                            = false;
  bool bounded_lll                        = false;
  bool gh_bound                           = false;
  double gh_factor                        = BKZ_DEF_GH_FACTOR;
  double min_success_probability          = BKZ_DEF_MIN_SUCCESS_PROBABILITY;
  int rerandomization_density             = BKZ_DEF_RERANDOMIZATION_DENSITY;
};

/**
 * Internal failure of a block-reduction run.
 *
 * `LLLReduction`/`MatGSOInterface` report numerical failures through their
 * status field; the block-reduction tours unwind with this exception and turn
 * it back into a `RedStatus` at the top level.
 */
class BKZFailure : public Error
{
public:
  explicit BKZFailure(RedStatus status)
      : Error(std::format("BKZ: LLL/size reduction failed with status '{}'", to_string(status))),
        status_(status)
  {
  }

  [[nodiscard]] RedStatus status() const noexcept { return status_; }

private:
  RedStatus status_;
};

/**
 * @brief Performs a heuristic check if BKZ can be terminated.
 *
 * Checks if the slope of the basis hasn't decreased in a while.
 */
template <class ZT, class FT> class BKZAutoAbort
{
public:
  /**
   * @brief Create an BKZAutoAbort object.
   *
   * @param m
   *    GSO object of the basis to be tested
   * @param num_rows
   *    the number of vectors to check
   * @param start_row
   *    the starting point of the vectors to check
   */
  BKZAutoAbort(MatGSOInterface<ZT, FT> &m, int num_rows, int start_row = 0)
      : m(m), old_slope(std::numeric_limits<double>::max()), num_rows(num_rows),
        start_row(start_row)
  {
  }

  /**
   * @brief Performs the check
   *
   * Performs the actual check by computing the new slope and checks if
   * it has decreased. Keeps track of the number of times it has not
   * decreased and returns true if that number is larger than maxNoDec.
   *
   * @param scale
   *    slack parameter on the slope (i.e. slope has to decrease by at
   *    at least a multiplicative factor of scale)
   * @param maxNoDec
   *    the number of successive non-decreases in the slope before true
   *    is returned
   *
   * @returns
   *    true, if the slope has not decreased for maxNoDec calls
   *    false otherwise
   */
  bool test_abort(double scale = 1.0, int maxNoDec = 5);

private:
  MatGSOInterface<ZT, FT> &m;
  double old_slope;
  int no_dec = -1;
  int num_rows;
  int start_row;
};

/**
 * @brief The class performing block reduction.
 *
 * This class implements BKZ, SD-BKZ and Slide Reduction. For this
 * it relies on the GSO, LLL, and Enumeration modules. It assumes
 * that the basis is LLL reduced.
 **/
template <class ZT, class FT> class BKZReduction
{
public:
  /**
   * @brief Create a BKZReduction object.
   *
   * @param m
   *    GSO object corresponding to the basis to be reduced
   * @param lll_obj
   *    LLL object associated to the same GSO object m
   * @param param
   *    parameter object (see bkz_param.h)
   */
  BKZReduction(MatGSOInterface<ZT, FT> &m, LLLReduction<ZT, FT> &lll_obj, const BKZParam &param);
  ~BKZReduction() noexcept;

  /**
   * @brief Preprocesses a block
   *
   * Preprocess a block using LLL or stronger recursive preprocessing.
   *
   * @param kappa
   *    start of the block
   * @param block_size
   *    size of the block
   * @param param
   *    parameter object for the current block size (the parameter object for the recursive
   *    calls will be created in this function using the information from this object)
   *@returns
   *    false if it modified the basis, true otherwise
   */
  bool svp_preprocessing(int kappa, unsigned int block_size, const BKZRunParameters &param);

  /**
   * @brief Inserts given (dual) vector into the basis
   *
   * @param kappa
   *    start of the block
   * @param block_size
   *    size of the block
   * @param solution
   *    the coefficients of the (dual) vector in the current (dual) basis
   * @param dual
   *    flag specifying if 'solution' is a dual or primal vector and to be
   *    inserted into the basis or its dual
   * @returns
   *    false if it made progress, true otherwise
   */
  bool svp_postprocessing(int kappa, int block_size, const std::vector<FT> &solution,
                          bool dual = false);

  /**
   * @brief (d)SVP-reduce a block.
   *
   * @param kappa
   *    start of the block
   * @param block_size
   *    size of the block
   * @param param
   *    parameter object (may be different from the one in the constructor)
   * @param dual
   *    flag specifying if the block is to be SVP or dual SVP reduced.
   * @returns
   *    false if it made progress, true otherwise
   */
  bool svp_reduction(int kappa, int block_size, const BKZRunParameters &param, bool dual = false);

  /**
   * @brief Runs a BKZ tour.
   *
   * @param loop
   *    counter indicating the iteration, only for reporting purposes
   * @param kappa_max
   *    the largest kappa s.t. the block from min_row to kappa is BKZ reduced, also
   *    only for reporting purposes
   * @param param
   *    parameter object
   * @param min_row
   *    start of the tour
   * @param max_row
   *    end of the tour
   * @return
   *    false if it made progress, true otherwise
   */
  bool tour(const int loop, int &kappa_max, const BKZRunParameters &param, int min_row,
            int max_row);

  /**
   * @brief Runs a SD-BKZ tour.
   *
   * @param loop
   *    counter indicating the iteration, only for reporting purposes
   * @param param
   *    parameter object
   * @param min_row
   *    start of the tour
   * @param max_row
   *    end of the tour
   * @return
   *    false if it made progress, true otherwise
   */
  bool sd_tour(const int loop, const BKZRunParameters &param, int min_row, int max_row);

  /**
   * @brief HKZ reduces a block.
   *
   * @param kappa_max
   *    the largest kappa s.t. the block from min_row to kappa is BKZ reduced, also
   *    only for reporting purposes
   * @param param
   *    parameter object
   * @param min_row
   *    start of the tour
   * @param max_row
   *    end of the tour
   * @return
   *    false if it made progress, true otherwise
   */
  bool hkz(int &kappa_max, const BKZRunParameters &param, int min_row, int max_row);

  /**
   * @brief Runs a tour of slide reduction.
   *
   * @param loop
   *    counter indicating the iteration, only for reporting purposes
   * @param param
   *    parameter object
   * @param min_row
   *    start of the tour
   * @param max_row
   *    end of the tour
   * @return
   *    false if it made progress, true otherwise
   */
  bool slide_tour(const int loop, const BKZRunParameters &param, int min_row, int max_row);

  /**
   * @brief Runs the main loop of block reduction.
   *
   * Top level function implementing block reduction by repeatedly
   * calling the corresponding tour and regularly checking terminating
   * conditions. Also performs some postprocessing.
   *
   * @return
   *    true if the reduction was successful, false otherwise.
   */
  bool bkz();

  /** Randomize basis between from ``min_row`` and ``max_row`` (exclusive)

      1. permute rows
      2. apply lower triangular matrix with coefficients in -1,0,1
      3. LLL reduce result

      @param min_row start in this row

      @param max_row stop at this row (exclusive)

      @param density number of non-zero coefficients in lower triangular
      transformation matrix
  **/

  void rerandomize_block(int min_row, int max_row, int density);

  /** Status of the reduction. */
  RedStatus status = RedStatus::success;

  /** Number of nodes visited during enumeration. */
  long nodes = 0;

  /** Number of tours that ran to completion. */
  long tours = 0;

private:
  void print_tour(const int loop, int min_row, int max_row);
  void print_params(const BKZRunParameters &param, std::ostream &out);

  bool set_status(RedStatus new_status);

  const PruningParams &get_pruning(int kappa, unsigned int block_size,
                                   const BKZRunParameters &param) const;

  // handles the general case of inserting a vector into the (dual) basis, i.e.
  // when none of the coefficients are \pm 1
  bool svp_postprocessing_generic(int kappa, int block_size, const std::vector<FT> &solution,
                                  bool dual = false);
  // a truncated tour: svp reducing from min_row to max_row without decreasing the window
  // size (simply returns when the last block is reduced)
  bool trunc_tour(int &kappa_max, const BKZRunParameters &param, int min_row, int max_row);
  // a truncated dual tour: dual svp reducing from max_row to min_row without decreasing
  // the window size (simply returns when the first block is reduced)
  bool trunc_dtour(const BKZRunParameters &param, int min_row, int max_row);

  const BKZParam &param;
  int num_rows;
  MatGSOInterface<ZT, FT> &m;
  LLLReduction<ZT, FT> &lll_obj;
  // evaluator passed to the enumeration object to handle solutions found
  FastEvaluator<FT> evaluator;
  // slack variable for svp reductions
  FT delta;

  // an acronym for the type of block reduction used, for reporting purposes
  const char *algorithm = nullptr;
  // current value of the potential function as defined in the slide reduction paper
  // used to reliably determine terminating condition during slide reduction
  FT sld_potential;

  std::chrono::milliseconds cputime_start{0};
};

}  // namespace fplll

#endif
