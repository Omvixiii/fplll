// SPDX-FileCopyrightText: 2008-2011 Xavier Pujol
// SPDX-FileCopyrightText: 2015 Michael Walter
// SPDX-FileCopyrightText: 2016 Marc Stevens
// SPDX-FileCopyrightText: 2016 Guillaume Bonnoron
//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Generic enumeration improvements, auxiliary solutions and subsolutions by
// Marc Stevens; CVP improvements by Guillaume Bonnoron.

#ifndef FPLLL_ENUMERATE_BASE_H
#define FPLLL_ENUMERATE_BASE_H

#include "../defs.h"
#include <fplll/fplll_config.h>

#include <cfenv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <span>
#include <vector>

namespace fplll
{

/** Rounds `src` to the nearest integer and stores the result in `dest`. */
inline void roundto(int &dest, double src) { dest = static_cast<int>(std::lround(src)); }

/**
   Installs the default rounding mode (`FE_TONEAREST`) for the lifetime of the
   guard and restores the rounding mode of the calling thread when the guard is
   destroyed.

   The sequential enumeration relies on round-to-nearest, so it runs under this
   guard.  Restoring in the destructor keeps the caller's rounding mode intact
   even when the search exits by throwing, for example when an error-bounded
   evaluator rejects a solution.  Rounding modes are per thread: the guard only
   affects the thread that runs the search.
 */
class ScopedRoundingMode
{
public:
  ScopedRoundingMode() noexcept : previous_(std::fegetround()) { std::fesetround(FE_TONEAREST); }

  ~ScopedRoundingMode() noexcept { std::fesetround(previous_); }

  ScopedRoundingMode(const ScopedRoundingMode &)            = delete;
  ScopedRoundingMode &operator=(const ScopedRoundingMode &) = delete;

private:
  int previous_;
};

/**
   Shared state of the depth-first enumeration loop.

   The search state is allocated once per enumeration, sized from the actual
   dimension: the old implementation kept >1 MB of fixed-size buffers per
   enumerator regardless of the dimension.  The recursive search itself is
   unrolled by the compiler up to `maxdim` levels, so `maxdim` stays a
   compile-time bound of the dispatch tables while the memory scales with `d`.
 */
class EnumerationBase
{
public:
  /** Compile-time bound on the enumeration dimension, used by the recursive
      template dispatch; the effective limit is `d <= maxdim`. */
  static constexpr int maxdim = FPLLL_MAX_ENUM_DIM;

  /** Number of nodes visited at a given level, or in total when level < 0. */
  [[nodiscard]] std::uint64_t get_nodes(int level = -1) const noexcept
  {
    if (level < 0)
    {
      return std::accumulate(node_counts_.cbegin(), node_counts_.cend(), std::uint64_t{0});
    }
    if (static_cast<std::size_t>(level) >= node_counts_.size())
    {
      return 0;
    }
    return node_counts_[static_cast<std::size_t>(level)];
  }

  /** Per-level node counts of the last enumeration, sized from the dimension. */
  [[nodiscard]] std::span<const std::uint64_t> nodes() const noexcept { return node_counts_; }

  virtual ~EnumerationBase() noexcept = default;

protected:
  /* configuration */
  bool dual      = false;
  bool is_svp    = false;  // only for SVP: exploit symmetry in enumeration to halve search space
  bool resetflag = false;

  /* enumeration input */
  std::vector<enumf> mut_;             // d*d row-major, transposed GSO coefficients
  std::vector<enumf> rdiag_;           // d, normalized squared Gram-Schmidt norms
  std::vector<enumf> partdistbounds_;  // d
  int d = 0;                           // dimension

  /* partial sum cache */
  std::vector<enumf> center_partsums_;     // d*(d+1) row-major, one spare column per row
  std::vector<enumf> center_partsum_;      // d
  std::vector<int> center_partsum_begin_;  // d+1

  /* enumeration data for each level */
  std::vector<enumf> partdist_;
  std::vector<enumf> center_;
  std::vector<enumf> alpha_;
  std::vector<enumf> subsoldists_;
  std::vector<int> x_;
  std::vector<int> dx_;
  std::vector<int> ddx_;

  /* CVP reset information */
  std::vector<int> _max_indices;
  int reset_depth = 0;

  int k = 0;

  /* node counts */
  std::vector<std::uint64_t> node_counts_;

  template <int kk, int kk_start, bool dualenum, bool findsubsols, bool enable_reset> struct opts
  {
  };

  /* need templated function argument for support of integer specialization for kk==-1 */
  template <int kk, int kk_start, bool dualenum, bool findsubsols, bool enable_reset>
  inline void enumerate_recursive(opts<kk, kk_start, dualenum, findsubsols, enable_reset>);
  template <int kk_start, bool dualenum, bool findsubsols, bool enable_reset>
  void enumerate_recursive(opts<-1, kk_start, dualenum, findsubsols, enable_reset>)
  {
  }

  /* simple wrapper with no function argument as helper for dispatcher */
  template <int kk, bool dualenum, bool findsubsols, bool enable_reset>
  void enumerate_recursive_wrapper()
  {
    // kk < maxdim: the dispatch table has exactly maxdim entries (see
    // enumerate_recursive_dispatch()), and enumerate() rejects d > maxdim.
    enumerate_recursive(opts<(kk < maxdim ? kk : -1), 0, dualenum, findsubsols, enable_reset>());
  }

  template <bool dualenum, bool findsubsols, bool enable_reset>
  inline void enumerate_recursive_dispatch(int kk);

  template <bool dualenum, bool findsubsols, bool enable_reset> void enumerate_loop();

  virtual void reset(enumf, int)                              = 0;
  virtual void process_solution(enumf newmaxdist)             = 0;
  virtual void process_subsolution(int offset, enumf newdist) = 0;

  /* Allocates the per-level state for an enumeration of dimension `dim`. */
  void allocate(int dim);
};

}  // namespace fplll

#endif
