// SPDX-FileCopyrightText: 2016 Marc Stevens
//
// SPDX-License-Identifier: MIT

#ifndef FPLLL_ENUM_PARALLEL_ENUMLIB_H
#define FPLLL_ENUM_PARALLEL_ENUMLIB_H

#include <cstdint>
#include <span>

namespace fplll::enumlib
{

/**
   Immutable, normalized description of one SVP enumeration problem.

   `mut` is the row-major d*d transposed Gram-Schmidt matrix: only the entries
   above the diagonal are read, with `mut[i * d + j] = mu_{j,i}` for j > i.
   `rdiag` holds the squared Gram-Schmidt norms, `pruning` the per-level
   pruning coefficients; both are normalized by a common power of two chosen by
   the caller.  An empty `pruning` span means no pruning (all coefficients are
   1).
 */
struct Shape
{
  int dim = 0;
  std::span<const double> mut;
  std::span<const double> rdiag;
  std::span<const double> pruning;
};

/**
   Receives the solutions found by the built-in parallel enumerator.

   The virtual calls happen once per solution, never per node, so they do not
   belong to the hot loop.  `on_solution` returns the new enumeration bound,
   exactly like the bound-update callbacks of the sequential evaluator.
 */
class Sink
{
public:
  Sink()                        = default;
  Sink(const Sink &)            = delete;
  Sink &operator=(const Sink &) = delete;
  Sink(Sink &&)                 = delete;
  Sink &operator=(Sink &&)      = delete;
  virtual ~Sink() noexcept      = default;

  virtual double on_solution(double dist, std::span<const double> coordinates)              = 0;
  virtual void on_subsolution(double dist, std::span<const double> coordinates, int offset) = 0;
};

/**
   Runs the built-in parallel enumerator on `shape`.

   Returns false without touching `sink` or `maxdist` when the problem is not
   handled by the parallel enumerator: dual enumeration, dimension <= 10,
   dimension above FPLLL_MAX_PARALLEL_ENUM_DIM, inconsistent shapes, or a
   `counts_out` buffer that is too small.  The caller then falls back to the
   sequential enumerator.

   On success `maxdist` holds the final enumeration bound and `counts_out`
   receives the per-level node counts (the first `shape.dim` entries).
 */
bool try_enumerate(const Shape &shape, double &maxdist, bool dual, bool findsubsols, Sink &sink,
                   std::span<std::uint64_t> counts_out);

}  // namespace fplll::enumlib

#endif
