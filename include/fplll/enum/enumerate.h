// SPDX-FileCopyrightText: 2008-2011 Xavier Pujol
// SPDX-FileCopyrightText: 2015 Michael Walter
// SPDX-FileCopyrightText: 2016 Marc Stevens
// SPDX-FileCopyrightText: 2016 Guillaume Bonnoron
//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Generic enumeration improvements, auxiliary solutions and subsolutions by
// Marc Stevens; CVP improvements by Guillaume Bonnoron.

#ifndef FPLLL_ENUMERATE_H
#define FPLLL_ENUMERATE_H

#include "../defs.h"
#include "../gso/gso_interface.h"
#include "enumerate_base.h"
#include "evaluator.h"

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace fplll
{

/**
   Sequential enumeration of the lattice vectors of a GSO basis.

   The level state is sized from the dimension and allocated once per call.  A
   `subtree` to start the search from is not supported: the exact CVP reset
   (see reset()) re-solves its subproblem with a nested enumeration instead.
 */
template <typename ZT, typename FT> class EnumerationDyn : public EnumerationBase
{
public:
  EnumerationDyn(MatGSOInterface<ZT, FT> &gso, Evaluator<FT> &evaluator,
                 const std::vector<int> &max_indices = std::vector<int>())
      : _gso(gso), _evaluator(evaluator), _max_indices(max_indices)
  {
  }

  /**
     Enumerates the lattice vectors whose coordinates are in [first, last).

     @param fmaxdist       in: initial enumeration bound; out: bound after the search
     @param fmaxdistexpo   binary exponent of `fmaxdist`
     @param target_coord   CVP target in GSO coordinates; empty for SVP
     @param pruning        per-level pruning coefficients; empty for no pruning
     @param dual           dual SVP enumeration
   */
  void enumerate(int first, int last, FT &fmaxdist, long fmaxdistexpo,
                 const std::vector<FT> &target_coord = std::vector<FT>(),
                 const std::vector<enumf> &pruning = std::vector<enumf>(), bool dual = false);

private:
  template <typename, typename> friend class Enumeration;

  /** Extracts and normalizes the GSO data for one enumeration and allocates
      the level state; no search is run. */
  void prepare(int first, int last, FT &fmaxdist, long fmaxdistexpo,
               const std::vector<FT> &target_coord, const std::vector<enumf> &pruning, bool dual);

  /** Runs the sequential depth-first search after prepare(). */
  void run_sequential();

  /** Converts the final bound back to the caller's representation and undoes
      the dual coordinate reversal. */
  void finish(FT &fmaxdist, long fmaxdistexpo);

  void prepare_enumeration();
  void do_enumerate();
  void set_bounds();
  void reset(enumf cur_dist, int cur_depth) override;
  void process_solution(enumf newmaxdist) override;
  void process_subsolution(int offset, enumf newdist) override;

  [[nodiscard]] int dim() const noexcept { return d; }
  [[nodiscard]] enumf maxdist() const noexcept { return maxdist_; }
  void set_maxdist(enumf bound) noexcept { maxdist_ = bound; }
  [[nodiscard]] std::span<const enumf> mut() const noexcept { return mut_; }
  [[nodiscard]] std::span<const enumf> rdiag() const noexcept { return rdiag_; }
  [[nodiscard]] std::span<const enumf> pruning() const noexcept { return pruning_; }
  [[nodiscard]] std::span<std::uint64_t> node_counts_buffer() noexcept { return node_counts_; }

  MatGSOInterface<ZT, FT> &_gso;
  Evaluator<FT> &_evaluator;
  std::vector<int> _max_indices;
  std::vector<FT> target_;
  std::vector<enumf> pruning_;
  std::vector<FT> fx_;
  long normexp_  = 0;
  enumf maxdist_ = 0.0;
};

/**
   Enumerator used by the solvers and the reductions.

   For SVP problems whose dimension is in the range of the built-in parallel
   enumerator it calls `enumlib` directly; every other problem (CVP, dual SVP,
   dimensions outside the parallel range) is solved by the sequential
   EnumerationDyn.  The parallel enumerator is ordinary internal code: there is
   no external-enumerator hook or function-pointer indirection.
 */
template <typename ZT, typename FT> class Enumeration
{
public:
  Enumeration(MatGSOInterface<ZT, FT> &gso, Evaluator<FT> &evaluator,
              const std::vector<int> &max_indices = std::vector<int>())
      : _gso(gso), _evaluator(evaluator), _max_indices(max_indices)
  {
  }

  void enumerate(int first, int last, FT &fmaxdist, long fmaxdistexpo,
                 const std::vector<FT> &target_coord = std::vector<FT>(),
                 const std::vector<enumf> &pruning = std::vector<enumf>(), bool dual = false);

  /** Number of nodes visited at a given level, or in total when level < 0. */
  [[nodiscard]] std::uint64_t get_nodes(int level = -1) const noexcept
  {
    return enumdyn != nullptr ? enumdyn->get_nodes(level) : 0;
  }

  /** Per-level node counts of the last enumeration, sized from the dimension. */
  [[nodiscard]] std::span<const std::uint64_t> nodes() const noexcept
  {
    return enumdyn != nullptr ? enumdyn->nodes() : std::span<const std::uint64_t>{};
  }

private:
  MatGSOInterface<ZT, FT> &_gso;
  Evaluator<FT> &_evaluator;
  std::vector<int> _max_indices;
  std::unique_ptr<EnumerationDyn<ZT, FT>> enumdyn;
};

}  // namespace fplll

#endif
