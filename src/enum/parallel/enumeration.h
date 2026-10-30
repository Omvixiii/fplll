// SPDX-FileCopyrightText: 2016 Marc Stevens
//
// SPDX-License-Identifier: MIT

/*
MIT License

Copyright (c) 2016 Marc Stevens

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef FPLLL_ENUM_PARALLEL_ENUMERATION_H
#define FPLLL_ENUM_PARALLEL_ENUMERATION_H

#include "../../detail/worker_pool.h"
#include "enumlib.h"

#include <fplll/enum/enumerate_base.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>
#include <vector>

namespace fplll::enumlib
{

using mutex_type = std::mutex;
using lock_type  = std::scoped_lock<std::mutex>;

/* One deferred top-level branch of the search: the coefficients fixed by the
   prefix, the partial norm at the branch point, and a lower bound for every
   completion of that prefix.  The batch dispatcher orders branches by the
   lower bound, smallest first. */
template <int N> struct swirl_item
{
  std::array<int, N> coefficients;
  double partial_norm;
  double lower_bound;
};

/* State shared by all workers of one enumeration. */
template <int N> struct globals_t
{
  globals_t() : A(0.0) {}

  globals_t(const globals_t &)            = delete;
  globals_t &operator=(const globals_t &) = delete;

  mutex_type mutex;
  std::atomic<double> A;  // set by the caller before the enumeration starts
  Sink *sink = nullptr;
  std::vector<std::vector<swirl_item<N>>> swirlys;
};

/* The immutable problem description, viewed with the compile-time dimension
   N.  All arrays are shared read-only by the workers; only the bounds in
   `globals_t` are published through the atomic above. */
template <int N> struct shape_t
{
  std::span<const double> mu;       // N*N transposed GSO matrix
  std::span<const double> rdiag;    // N squared Gram-Schmidt norms
  std::span<const double> pruning;  // N coefficients, empty => no pruning
};

/* Per-worker search state.  Everything that the search writes lives here, so
   copying a worker slot does not copy the N*N problem description. */
template <int N, int SWIRLY, bool findsubsols> struct scratch_t
{
  using fltrow_t = std::array<double, N>;
  using introw_t = std::array<int, N>;

  static constexpr int swirly_buffer = 1 << 10;

  scratch_t(const shape_t<N> &shape, globals_t<N> &globals) : shape_(shape), globals_(globals)
  {
    for (int k = 0; k < N; ++k)
    {
      _risq[k] = shape_.rdiag[static_cast<std::size_t>(k)];
      _pr[k]   = shape_.pruning.empty() ? 1.0 : shape_.pruning[static_cast<std::size_t>(k)];
    }
  }

  /* inputs (read-only during the search; copied once per worker slot) */
  const shape_t<N> &shape_;
  globals_t<N> &globals_;
  fltrow_t _risq;
  fltrow_t _pr;

  /* search state */
  double _a;     // overall enumeration bound
  fltrow_t _aa;  // pruning bounds for the current _a
  introw_t _x, _dx, _d2x;
  fltrow_t _sol;  // solution passed to the sink
  fltrow_t _c;
  introw_t _r;
  std::array<double, N + 1> _l;
  std::array<std::uint64_t, N> _counts;
  double _sigT[N][N];

  fltrow_t _subsolL;
  std::array<fltrow_t, N> _subsol;

  /* Adopt the shared bound if it moved.  The bound is published with the
     evaluator under globals_.mutex; only the value and the pruning bounds
     depend on it, so no per-thread bookkeeping is needed. */
  void refresh_bound()
  {
    const double bound = globals_.A.load(std::memory_order_relaxed);
    if (bound != _a)
    {
      _a = bound;
      _update_aa();
    }
  }

  void _update_aa()
  {
    for (int k = 0; k < N; ++k)
    {
      _aa[k] = _a * _pr[k];
    }
  }

  // compile time parameters for enumerate_recur (without any runtime overhead)
  // allows specialization for certain specific cases, e.g., i=0, or i=swirl
  template <int i, int swirl, int swirlid> struct i_tag
  {
  };

  template <int i, int swirl, int swirlid> void enumerate_recur(i_tag<i, swirl, swirlid>)
  {
    if (_r[i] > _r[i - 1])
    {
      _r[i - 1] = _r[i];
    }
    double ci = _sigT[i][i];
    double yi = std::round(ci);
    int xi    = static_cast<int>(yi);
    yi        = ci - yi;
    double li = _l[i + 1] + (yi * yi * _risq[i]);
    ++_counts[i];

    if (findsubsols && li < _subsolL[i] && li != 0.0)
    {
      _subsolL[i]   = li;
      _subsol[i][i] = xi;
      for (int j = i + 1; j < N; ++j)
      {
        _subsol[i][j] = _x[j];
      }
    }
    if (li > _aa[i])
    {
      return;
    }

    _dx[i] = _d2x[i] = ((static_cast<int>(yi >= 0) & 1) << 1) - 1;
    _c[i]            = ci;
    _x[i]            = xi;
    _l[i]            = li;

    for (int j = _r[i - 1]; j > i - 1; --j)
    {
      _sigT[i - 1][j - 1] =
          _sigT[i - 1][j] - _x[j] * shape_.mu[static_cast<std::size_t>(i - 1) * N + j];
    }

    refresh_bound();

    while (true)
    {
      enumerate_recur(i_tag<i - 1, swirl, swirlid>());

      if (_l[i + 1] == 0.0)
      {
        ++_x[i];
        _r[i - 1]  = i;
        double yi2 = _c[i] - _x[i];
        double li2 = _l[i + 1] + (yi2 * yi2 * _risq[i]);
        if (li2 > _aa[i])
        {
          return;
        }
        _l[i] = li2;
        _sigT[i - 1][i - 1] =
            _sigT[i - 1][i] - _x[i] * shape_.mu[static_cast<std::size_t>(i - 1) * N + i];
      }
      else
      {
        _x[i] += _dx[i];
        _d2x[i]    = -_d2x[i];
        _dx[i]     = _d2x[i] - _dx[i];
        _r[i - 1]  = i;
        double yi2 = _c[i] - _x[i];
        double li2 = _l[i + 1] + (yi2 * yi2 * _risq[i]);
        if (li2 > _aa[i])
        {
          return;
        }
        _l[i] = li2;
        _sigT[i - 1][i - 1] =
            _sigT[i - 1][i] - _x[i] * shape_.mu[static_cast<std::size_t>(i - 1) * N + i];
      }
    }
  }

  template <int swirl, int swirlid> void enumerate_recur(i_tag<0, swirl, swirlid>)
  {
    constexpr int i = 0;
    double ci       = _sigT[i][i];
    double yi       = std::round(ci);
    int xi          = static_cast<int>(yi);
    yi              = ci - yi;
    double li       = _l[i + 1] + (yi * yi * _risq[i]);
    ++_counts[i];

    if (findsubsols && li < _subsolL[i] && li != 0.0)
    {
      _subsolL[i]   = li;
      _subsol[i][i] = xi;
      for (int j = i + 1; j < N; ++j)
      {
        _subsol[i][j] = _x[j];
      }
    }
    if (li > _aa[i])
    {
      return;
    }

    _dx[i] = _d2x[i] = ((static_cast<int>(yi >= 0) & 1) << 1) - 1;
    _c[i]            = ci;
    _x[i]            = xi;
    _l[i]            = li;

    refresh_bound();

    while (true)
    {
      enumerate_recur(i_tag<i - 1, swirl, swirlid>());

      if (_l[i + 1] == 0.0)
      {
        ++_x[i];
        double yi2 = _c[i] - _x[i];
        double li2 = _l[i + 1] + (yi2 * yi2 * _risq[i]);
        if (li2 > _aa[i])
        {
          return;
        }
        _l[i] = li2;
      }
      else
      {
        _x[i] += _dx[i];
        _d2x[i]    = -_d2x[i];
        _dx[i]     = _d2x[i] - _dx[i];
        double yi2 = _c[i] - _x[i];
        double li2 = _l[i + 1] + (yi2 * yi2 * _risq[i]);
        if (li2 > _aa[i])
        {
          return;
        }
        _l[i] = li2;
      }
    }
  }

  template <int swirl, int swirlid> void enumerate_recur(i_tag<-1, swirl, swirlid>)
  {
    if (_l[0] > _aa[0] || _l[0] == 0.0)
    {
      return;
    }

    lock_type lock(globals_.mutex);

    for (int j = 0; j < N; ++j)
    {
      _sol[j] = _x[j];
    }
    globals_.A.store(globals_.sink->on_solution(
                         _l[0], std::span<const double>(_sol.data(), static_cast<std::size_t>(N))),
                     std::memory_order_relaxed);

    // publish the bound and adopt it for the rest of this search
    refresh_bound();
  }

  /* Collect the branches at the top of the tree instead of recursing into
     them, so the dispatcher can hand them to the workers in bound order. */
  template <int i, int swirlid> void enumerate_recur(i_tag<i, i, swirlid>)
  {
    if (_r[i] > _r[i - 1])
    {
      _r[i - 1] = _r[i];
    }

    double ci = _sigT[i][i];
    double yi = std::round(ci);
    int xi    = static_cast<int>(yi);
    yi        = ci - yi;
    double li = _l[i + 1] + (yi * yi * _risq[i]);
    ++_counts[i];

    if (findsubsols && li < _subsolL[i] && li != 0.0)
    {
      _subsolL[i]   = li;
      _subsol[i][i] = xi;
      for (int j = i + 1; j < N; ++j)
      {
        _subsol[i][j] = _x[j];
      }
    }
    if (li > _aa[i])
    {
      return;
    }
    _c[i]  = ci;
    _x[i]  = xi;
    _l[i]  = li;
    _dx[i] = _d2x[i] = ((static_cast<int>(yi >= 0) & 1) << 1) - 1;

    for (int j = _r[i - 1]; j > i - 1; --j)
    {
      _sigT[i - 1][j - 1] =
          _sigT[i - 1][j] - _x[j] * shape_.mu[static_cast<std::size_t>(i - 1) * N + j];
    }

    while (true)
    {
      const double ci2         = _sigT[i - 1][i - 1];
      const int xi2            = static_cast<int>(std::lround(ci2));
      const double yi2         = ci2 - xi2;
      const double bound_below = _l[i] + (yi2 * yi2 * _risq[i - 1]);

      globals_.swirlys[swirlid].emplace_back();
      swirl_item<N> &item = globals_.swirlys[swirlid].back();
      for (int j = i; j < N; ++j)
      {
        item.coefficients[j] = _x[j];
      }
      item.partial_norm = _l[i];
      item.lower_bound  = bound_below;

      if (_l[i + 1] == 0.0)
      {
        ++_x[i];
        _r[i - 1]  = i;
        double yi3 = _c[i] - _x[i];
        double li3 = _l[i + 1] + (yi3 * yi3 * _risq[i]);
        if (li3 > _aa[i])
        {
          return;
        }
        _l[i] = li3;
        _sigT[i - 1][i - 1] =
            _sigT[i - 1][i] - _x[i] * shape_.mu[static_cast<std::size_t>(i - 1) * N + i];
      }
      else
      {
        _x[i] += _dx[i];
        _d2x[i]    = -_d2x[i];
        _dx[i]     = _d2x[i] - _dx[i];
        _r[i - 1]  = i;
        double yi3 = _c[i] - _x[i];
        double li3 = _l[i + 1] + (yi3 * yi3 * _risq[i]);
        if (li3 > _aa[i])
        {
          return;
        }
        _l[i] = li3;
        _sigT[i - 1][i - 1] =
            _sigT[i - 1][i] - _x[i] * shape_.mu[static_cast<std::size_t>(i - 1) * N + i];
      }
    }
  }

  /* Runs the whole enumeration of one session.  Must be called on the thread
     that owns the session; it is the only thread touching this object. */
  void run(detail::EnumerationSession &session)
  {
    _a = globals_.A.load(std::memory_order_relaxed);
    _update_aa();

    for (int j = 0; j < N; ++j)
    {
      _x[j] = _dx[j] = 0;
      _d2x[j]        = -1;
      _sol[j]        = 0;
      _c[j] = _l[j] = 0.0;
      _subsolL[j]   = _risq[j];
      for (int k = 0; k < N; ++k)
      {
        _sigT[j][k]   = 0.0;
        _subsol[j][k] = 0;
      }
      _r[j]      = N - 1;
      _counts[j] = 0;
    }
    _l[N] = 0.0;

    auto &swirlys = globals_.swirlys;
    swirlys.resize(2);
    swirlys[0].clear();
    enumerate_recur(i_tag<N - 1, N - SWIRLY, 0>());

    /* One stable snapshot of the enumeration state for the whole call.  Each
       worker slot gets its own copy before any worker starts, so during a
       batch nobody reads the scratch state that the calling thread is using.
       The N*N problem description is shared read-only. */
    const int workers = session.concurrency();
    std::vector<scratch_t> locals;
    locals.reserve(static_cast<std::size_t>(workers));
    for (int worker = 0; worker < workers; ++worker)
    {
      locals.emplace_back(*this);
    }

    std::size_t swirly0idx = 0;
    swirlys[1].clear();
    while (swirly0idx < swirlys[0].size())
    {
      while (swirly0idx < swirlys[0].size() &&
             swirlys[1].size() < static_cast<std::size_t>(swirly_buffer))
      {
        constexpr int i = N - SWIRLY;
        _x              = swirlys[0][swirly0idx].coefficients;
        _l[i]           = swirlys[0][swirly0idx].partial_norm;
        for (int j = 0; j < N; ++j)
        {
          _r[j] = N - 1;
        }
        for (int j = N - 1; j > i - 1; --j)
        {
          _sigT[i - 1][j - 1] =
              _sigT[i - 1][j] - _x[j] * shape_.mu[static_cast<std::size_t>(i - 1) * N + j];
        }

        enumerate_recur(i_tag<N - SWIRLY - 1, N - 2 * SWIRLY, 1>());

        ++swirly0idx;
      }

      /* The search is heavy-tailed: hand the branches with the smallest
         completion bound to the workers first, so a good solution tightens the
         shared bound before the expensive branches are entered.  This only
         pays off once several workers share the batch; one or two threads keep
         the original depth-first order, where the sort can cost a few percent. */
      if (session.concurrency() >= 4 && swirlys[1].size() > 1)
      {
        std::sort(swirlys[1].begin(), swirlys[1].end(),
                  [](const swirl_item<N> &lhs, const swirl_item<N> &rhs)
                  { return lhs.lower_bound < rhs.lower_bound; });
      }

      const std::size_t swirly1end = swirlys[1].size();

      auto &swirly_ref = swirlys[1];
      std::atomic<std::size_t> swirly_i(0);
      session.run_workers(
          [&locals, &swirly_ref, &swirly_i, swirly1end](int worker)
          {
            /* The search assumes round-to-nearest on every thread.  The pool
               threads are reused across sessions, so the mode is installed per
               task body rather than once when the pool is created; the guard
               also restores the thread's previous mode when it leaves. */
            const ScopedRoundingMode rounding;
            scratch_t &local = locals[static_cast<std::size_t>(worker)];
            for (int j = 0; j < N - SWIRLY; ++j)
            {
              local._counts[j] = 0;
            }
            while (true)
            {
              std::size_t idx = swirly_i++;
              if (idx >= swirly1end)
              {
                break;
              }

              constexpr int i = N - 2 * SWIRLY;
              local._x        = swirly_ref[idx].coefficients;
              local._l[i]     = swirly_ref[idx].partial_norm;
              for (int j = 0; j < N; ++j)
              {
                local._r[j] = N - 1;
              }
              for (int j = N - 1; j > i - 1; --j)
              {
                local._sigT[i - 1][j - 1] =
                    local._sigT[i - 1][j] -
                    local._x[j] * local.shape_.mu[static_cast<std::size_t>(i - 1) * N + j];
              }

              local.refresh_bound();

              local.enumerate_recur(i_tag<N - 2 * SWIRLY - 1, -2, -1>());
            }
          });

      /* All copies have been joined; merge them in slot order on the calling
         thread, which is the only thread touching this object again.  When
         subsolutions are collected, hand the merged best bounds back to every
         slot so the next batch prunes with the same bounds a fresh copy would
         have started from. */
      for (scratch_t &local : locals)
      {
        for (int j = 0; j < N - SWIRLY; ++j)
        {
          _counts[j] += local._counts[j];
        }
        for (int j = 0; j < N; ++j)
        {
          if (local._subsolL[j] < _subsolL[j])
          {
            _subsolL[j] = local._subsolL[j];
            _subsol[j]  = local._subsol[j];
          }
        }
      }
      if (findsubsols)
      {
        for (scratch_t &local : locals)
        {
          local._subsolL = _subsolL;
          local._subsol  = _subsol;
        }
      }

      swirlys[1].clear();
    }
  }
};

}  // namespace fplll::enumlib

#endif
