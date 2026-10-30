// SPDX-FileCopyrightText: 2008-2011 Xavier Pujol
// SPDX-FileCopyrightText: 2015 Michael Walter
// SPDX-FileCopyrightText: 2016 Marc Stevens
// SPDX-FileCopyrightText: 2016 Guillaume Bonnoron
//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Generic enumeration improvements, auxiliary solutions and subsolutions by
// Marc Stevens; CVP improvements by Guillaume Bonnoron.

#include <fplll/defs.h>
#include <fplll/enum/enumerate.h>
#include <fplll/enum/enumerate_base.h>
#include <fplll/enum/evaluator.h>
#include <fplll/fplll_config.h>
#include <fplll/nr/nr.h>

#include <fplll/nr/numvect.h>

#if FPLLL_MAX_PARALLEL_ENUM_DIM != 0
#include "parallel/enumlib.h"
#endif

#include <algorithm>
#include <cstddef>
#include <gmp.h>
#include <memory>
#include <span>
#include <vector>

namespace fplll
{

namespace
{

#if FPLLL_MAX_PARALLEL_ENUM_DIM != 0
/* Adapts an Evaluator to the parallel enumerator's solution sink.  There is
   one virtual call per solution (not per node); the normalized double
   coordinates are converted back to FT and the bound updates go through the
   evaluator, which keeps the multimap solution API intact. */
template <class FT> class EvaluatorSink final : public enumlib::Sink
{
public:
  EvaluatorSink(Evaluator<FT> &evaluator, int dim, enumf initial_bound)
      : evaluator_(evaluator), fx_(static_cast<std::size_t>(dim)), bound_(initial_bound)
  {
  }

  enumf on_solution(double dist, std::span<const double> coordinates) override
  {
    for (std::size_t i = 0; i < coordinates.size(); ++i)
    {
      fx_[i] = coordinates[i];
    }
    evaluator_.eval_sol(fx_, dist, bound_);
    return bound_;
  }

  void on_subsolution(double dist, std::span<const double> coordinates, int offset) override
  {
    for (std::size_t i = 0; i < coordinates.size(); ++i)
    {
      fx_[i] = coordinates[i];
    }
    evaluator_.eval_sub_sol(offset, fx_, dist);
  }

private:
  Evaluator<FT> &evaluator_;
  std::vector<FT> fx_;
  enumf bound_;
};
#endif

}  // namespace

template <typename ZT, typename FT>
void Enumeration<ZT, FT>::enumerate(int first, int last, FT &fmaxdist, long fmaxdistexpo,
                                    const std::vector<FT> &target_coord,
                                    const std::vector<enumf> &pruning, bool dual)
{
  if (enumdyn == nullptr)
  {
    enumdyn = std::make_unique<EnumerationDyn<ZT, FT>>(_gso, _evaluator, _max_indices);
  }

  enumdyn->prepare(first, last, fmaxdist, fmaxdistexpo, target_coord, pruning, dual);

#if FPLLL_MAX_PARALLEL_ENUM_DIM != 0
  /* The built-in parallel enumerator is the default for SVP problems inside
     its dimension range; it decides on its own how many threads to use, so
     this does not depend on the runtime thread count.  There is no external
     hook and no function-pointer indirection: the call below is a direct
     internal call. */
  if (target_coord.empty() && !dual && enumdyn->dim() <= FPLLL_MAX_PARALLEL_ENUM_DIM)
  {
    enumlib::Shape shape;
    shape.dim     = enumdyn->dim();
    shape.mut     = enumdyn->mut();
    shape.rdiag   = enumdyn->rdiag();
    shape.pruning = enumdyn->pruning();

    enumf bound = enumdyn->maxdist();
    bool solved = false;
    {
      /* The parallel search relies on round-to-nearest on the calling thread
         as well as on every worker thread.  The workers install the mode at the
         start of each task body because the pool is reused; this guard covers
         the calling thread (preprocessing, the slot it runs itself and the
         sink callbacks) and restores its previous mode on every exit path,
         including an evaluator exception.  finish() runs outside the guard,
         exactly like the sequential path. */
      const ScopedRoundingMode rounding;
      EvaluatorSink<FT> sink(_evaluator, shape.dim, bound);
      solved = enumlib::try_enumerate(shape, bound, /*dual=*/false, _evaluator.findsubsols, sink,
                                      enumdyn->node_counts_buffer());
      if (solved)
      {
        enumdyn->set_maxdist(bound);
      }
    }
    if (solved)
    {
      enumdyn->finish(fmaxdist, fmaxdistexpo);
      return;
    }
  }
#endif

  enumdyn->run_sequential();
  enumdyn->finish(fmaxdist, fmaxdistexpo);
}

template <typename ZT, typename FT>
void EnumerationDyn<ZT, FT>::enumerate(int first, int last, FT &fmaxdist, long fmaxdistexpo,
                                       const std::vector<FT> &target_coord,
                                       const std::vector<enumf> &pruning, bool dual)
{
  prepare(first, last, fmaxdist, fmaxdistexpo, target_coord, pruning, dual);
  run_sequential();
  finish(fmaxdist, fmaxdistexpo);
}

template <typename ZT, typename FT>
void EnumerationDyn<ZT, FT>::prepare(int first, int last, FT &fmaxdist, long fmaxdistexpo,
                                     const std::vector<FT> &target_coord,
                                     const std::vector<enumf> &pruning, bool _dual)
{
  if (last == -1)
  {
    last = _gso.get_d();
  }

  const bool solvingsvp = target_coord.empty();
  dual                  = _dual;
  target_               = target_coord;
  pruning_              = pruning;

  const int new_dim = last - first;
  FPLLL_CHECK(new_dim > 0, "enumerate: empty enumeration dimension");
  FPLLL_CHECK(new_dim <= maxdim,
              "enumerate: dimension {} exceeds the maximum enumeration dimension {}", new_dim,
              maxdim);
  FPLLL_CHECK(solvingsvp || !dual, "enumerate: CVP enumeration is not implemented for dual SVP");
  /* `_max_indices` is the reset schedule of the exact CVP search: it is only
     produced by the CVPM_PROVED path, and the reset() sub-enumeration is a CVP
     search.  A dual enumeration has no use for it, and the historical 6-branch
     dispatcher silently ran no search at all for this combination.  Reject it
     before any state is allocated or overwritten. */
  FPLLL_CHECK(_max_indices.empty() || !dual,
              "enumerate: max_indices (the exact-CVP reset schedule) is not supported for dual "
              "enumeration");
  const int pruning_dim = static_cast<int>(pruning_.size());
  FPLLL_CHECK(pruning_.empty() || pruning_dim == new_dim,
              "enumerate: non-empty pruning vector dimension does not match the enumeration "
              "dimension");
  allocate(new_dim);

  resetflag = !_max_indices.empty();
  if (resetflag)
  {
    FPLLL_CHECK(last - 1 < static_cast<int>(_max_indices.size()),
                "enumerate: max_indices does not cover the enumeration dimension");
    reset_depth = _max_indices[static_cast<std::size_t>(last - 1)];
  }

  if (solvingsvp)
  {
    std::fill(center_partsum_.begin(), center_partsum_.end(), 0.0);
  }
  else
  {
    for (int i = 0; i < d; ++i)
    {
      center_partsum_[i] = target_coord[i + first].get_d();
    }
  }

  FT fr, fmu, fmaxdistnorm;
  long rexpo;
  long normexp = -1;
  for (int i = 0; i < d; ++i)
  {
    fr      = _gso.get_r_exp(i + first, i + first, rexpo);
    normexp = std::max(normexp, rexpo + fr.exponent());
  }
  // normalization is multiplication by 2^(-normexp). in case of dual, we are normalizing
  // the inverse of r, so we negate the normalizing exponent
  if (dual)
  {
    normexp *= -1;
  }
  normexp_ = normexp;
  fmaxdistnorm.mul_2si(fmaxdist, fmaxdistexpo - normexp);
  maxdist_ = fmaxdistnorm.get_d(GMP_RNDU);
  _evaluator.set_normexp(normexp);

  fx_.resize(static_cast<std::size_t>(d));

  if (dual)
  {
    for (int i = 0; i < d; ++i)
    {
      fr = _gso.get_r_exp(i + first, i + first, rexpo);
      fr.mul_2si(fr, rexpo + normexp);
      rdiag_[static_cast<std::size_t>(d - i - 1)] = static_cast<enumf>(1.0) / fr.get_d();
    }
    for (int i = 0; i < d; ++i)
    {
      for (int j = i + 1; j < d; ++j)
      {
        _gso.get_mu(fmu, j + first, i + first);
        mut_[static_cast<std::size_t>(d - j - 1) * static_cast<std::size_t>(d) +
             static_cast<std::size_t>(d - i - 1)] = -fmu.get_d();
      }
    }
  }
  else
  {
    for (int i = 0; i < d; ++i)
    {
      fr = _gso.get_r_exp(i + first, i + first, rexpo);
      fr.mul_2si(fr, rexpo - normexp);
      rdiag_[static_cast<std::size_t>(i)] = fr.get_d();
    }
    for (int i = 0; i < d; ++i)
    {
      for (int j = i + 1; j < d; ++j)
      {
        _gso.get_mu(fmu, j + first, i + first);
        mut_[static_cast<std::size_t>(i) * static_cast<std::size_t>(d) +
             static_cast<std::size_t>(j)] = fmu.get_d();
      }
    }
  }

  subsoldists_ = rdiag_;
}

template <typename ZT, typename FT> void EnumerationDyn<ZT, FT>::run_sequential()
{
  // The search relies on round-to-nearest.  The guard restores the caller's
  // rounding mode on every exit path, including exceptions thrown by the
  // evaluator inside the depth-first search.
  const ScopedRoundingMode rounding;
  prepare_enumeration();
  do_enumerate();
}

template <typename ZT, typename FT>
void EnumerationDyn<ZT, FT>::finish(FT &fmaxdist, long fmaxdistexpo)
{
  FT fmaxdistnorm;
  fmaxdistnorm = maxdist_;  // Exact
  fmaxdist.mul_2si(fmaxdistnorm, normexp_ - fmaxdistexpo);

  if (dual && !_evaluator.empty())
  {
    for (auto it = _evaluator.begin(), itend = _evaluator.end(); it != itend; ++it)
    {
      reverse_by_swap(it->second, 0, d - 1);
    }
  }
}

template <typename ZT, typename FT> void EnumerationDyn<ZT, FT>::prepare_enumeration()
{
  is_svp = target_.empty();

  enumf newdist = 0.0;
  for (k = d - 1; k >= 0 && newdist <= maxdist_; --k)
  {
    enumf newcenter = center_partsum_[static_cast<std::size_t>(k)];
    if (dual)
    {
      for (int j = k + 1; j < d; ++j)
      {
        newcenter -= alpha_[static_cast<std::size_t>(j)] *
                     mut_[static_cast<std::size_t>(k) * static_cast<std::size_t>(d) +
                          static_cast<std::size_t>(j)];
      }
    }
    else
    {
      for (int j = k + 1; j < d; ++j)
      {
        newcenter -= x_[static_cast<std::size_t>(j)] *
                     mut_[static_cast<std::size_t>(k) * static_cast<std::size_t>(d) +
                          static_cast<std::size_t>(j)];
      }
    }
    roundto(x_[static_cast<std::size_t>(k)], newcenter);  // newX = rint(newCenter)
    center_[static_cast<std::size_t>(k)]   = newcenter;
    partdist_[static_cast<std::size_t>(k)] = newdist;
    dx_[static_cast<std::size_t>(k)]       = ddx_[static_cast<std::size_t>(k)] =
        ((static_cast<int>(newcenter >= x_[static_cast<std::size_t>(k)]) & 1) << 1) - 1;
    alpha_[static_cast<std::size_t>(k)] = x_[static_cast<std::size_t>(k)] - newcenter;
    newdist += alpha_[static_cast<std::size_t>(k)] * alpha_[static_cast<std::size_t>(k)] *
               rdiag_[static_cast<std::size_t>(k)];
  }
  if (is_svp)
  {
    x_[0] = 1;  // Excludes (0,...,0) from the enumeration
  }
  ++k;
}

template <typename ZT, typename FT> void EnumerationDyn<ZT, FT>::set_bounds()
{
  if (pruning_.empty())
  {
    std::fill(partdistbounds_.begin(), partdistbounds_.end(), maxdist_);
  }
  else
  {
    for (int i = 0; i < d; ++i)
    {
      partdistbounds_[static_cast<std::size_t>(i)] =
          pruning_[static_cast<std::size_t>(i)] * maxdist_;
    }
  }
}

template <typename ZT, typename FT> void EnumerationDyn<ZT, FT>::process_solution(enumf newmaxdist)
{
  for (int j = 0; j < d; ++j)
  {
    fx_[static_cast<std::size_t>(j)] = x_[static_cast<std::size_t>(j)];
  }
  _evaluator.eval_sol(fx_, newmaxdist, maxdist_);

  set_bounds();
}

template <typename ZT, typename FT>
void EnumerationDyn<ZT, FT>::process_subsolution(int offset, enumf newdist)
{
  for (int j = 0; j < offset; ++j)
  {
    fx_[static_cast<std::size_t>(j)] = 0.0;
  }
  for (int j = offset; j < d; ++j)
  {
    fx_[static_cast<std::size_t>(j)] = x_[static_cast<std::size_t>(j)];
  }
  _evaluator.eval_sub_sol(offset, fx_, newdist);
}

template <typename ZT, typename FT> void EnumerationDyn<ZT, FT>::do_enumerate()
{
  std::fill(node_counts_.begin(), node_counts_.end(), 0);

  set_bounds();

  /* Every (dual, findsubsols, resetflag) combination has an explicit arm here.
     The two dual && resetflag combinations are rejected before the search
     (prepare()): `max_indices` is the exact-CVP reset schedule, which only the
     sequential CVP search uses.  They must raise an error instead of falling
     through to an empty search, which is what the historical 6-branch chain
     did.  The final else is unreachable but keeps the dispatcher total. */
  if (dual && resetflag)
  {
    FPLLL_CHECK(false, "enumerate: max_indices (the exact-CVP reset schedule) is not supported "
                       "for dual enumeration");
  }
  else if (dual && _evaluator.findsubsols)
  {
    enumerate_loop<true, true, false>();
  }
  else if (dual)
  {
    enumerate_loop<true, false, false>();
  }
  else if (resetflag && _evaluator.findsubsols)
  {
    enumerate_loop<false, true, true>();
  }
  else if (resetflag)
  {
    enumerate_loop<false, false, true>();
  }
  else if (_evaluator.findsubsols)
  {
    enumerate_loop<false, true, false>();
  }
  else if (!_evaluator.findsubsols)
  {
    enumerate_loop<false, false, false>();
  }
  else
  {
    FPLLL_CHECK(false, "enumerate: unreachable enumeration dispatch state");
  }
}

template <typename ZT, typename FT>
void EnumerationDyn<ZT, FT>::reset(enumf cur_dist, int cur_depth)
{
  const int new_dim = cur_depth + 1;

  /* Re-solve the CVP subproblem for the coordinates fixed above `cur_depth`:
     subtract their contribution from the target and enumerate the first
     new_dim coordinates from scratch with the same bound.  This replaces the
     former nested enumeration that fixed a subtree through the public API. */
  std::vector<FT> adjusted_target(static_cast<std::size_t>(new_dim));
  for (int j = 0; j < new_dim; ++j)
  {
    enumf center = center_partsum_[static_cast<std::size_t>(j)];
    for (int k = new_dim; k < d; ++k)
    {
      center -= static_cast<enumf>(x_[static_cast<std::size_t>(k)]) *
                mut_[static_cast<std::size_t>(j) * static_cast<std::size_t>(d) +
                     static_cast<std::size_t>(k)];
    }
    adjusted_target[static_cast<std::size_t>(j)] = center;
  }

  FT new_dist = 0.0;
  for (int i = 0; i < new_dim; i++)
  {
    new_dist.add(new_dist, _gso.get_r_exp(i, i));
  }

  std::vector<enumf> sub_pruning;
  if (!pruning_.empty())
  {
    sub_pruning.assign(pruning_.begin(), pruning_.begin() + new_dim);
  }

  FastEvaluator<FT> new_evaluator;
  Enumeration<ZT, FT> enumobj(_gso, new_evaluator, _max_indices);
  enumobj.enumerate(0, new_dim, new_dist, 0, adjusted_target, sub_pruning, false);

  if (!new_evaluator.empty())
  {
    FT sol_dist2 = new_evaluator.begin()->first;
    sol_dist2.mul_2si(sol_dist2, -new_evaluator.normExp);
    enumf sol_dist = sol_dist2.get_d();

    if (sol_dist + cur_dist < partdistbounds_[0])
    {
      for (int i = 0; i < new_dim; ++i)
      {
        x_[static_cast<std::size_t>(i)] =
            static_cast<int>(new_evaluator.begin()->second[static_cast<std::size_t>(i)].get_d());
      }
      process_solution(sol_dist + cur_dist);
    }
  }
}

template class Enumeration<Z_NR<mpz_t>, FP_NR<double>>;
template class EnumerationDyn<Z_NR<mpz_t>, FP_NR<double>>;

#ifdef FPLLL_WITH_QD
template class Enumeration<Z_NR<mpz_t>, FP_NR<dd_real>>;
template class EnumerationDyn<Z_NR<mpz_t>, FP_NR<dd_real>>;

template class Enumeration<Z_NR<mpz_t>, FP_NR<qd_real>>;
template class EnumerationDyn<Z_NR<mpz_t>, FP_NR<qd_real>>;
#endif

template class Enumeration<Z_NR<mpz_t>, FP_NR<mpfr_t>>;
template class EnumerationDyn<Z_NR<mpz_t>, FP_NR<mpfr_t>>;

template class Enumeration<Z_NR<long>, FP_NR<double>>;
template class EnumerationDyn<Z_NR<long>, FP_NR<double>>;

#ifdef FPLLL_WITH_QD
template class Enumeration<Z_NR<long>, FP_NR<dd_real>>;
template class EnumerationDyn<Z_NR<long>, FP_NR<dd_real>>;

template class Enumeration<Z_NR<long>, FP_NR<qd_real>>;
template class EnumerationDyn<Z_NR<long>, FP_NR<qd_real>>;
#endif

template class Enumeration<Z_NR<long>, FP_NR<mpfr_t>>;
template class EnumerationDyn<Z_NR<long>, FP_NR<mpfr_t>>;

}  // namespace fplll
