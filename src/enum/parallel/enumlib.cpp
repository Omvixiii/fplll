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

#include "enumlib.h"

#include <cstddef>
#include <cstdint>
#include <span>

#include <fplll/fplll_config.h>

namespace fplll::enumlib
{

/* Entry points of the generated per-dimension translation units; each one
   handles the ten dimensions ending at its own constant (e.g. enumerate20
   dispatches 11..20). */
#define ENUMFUNCNAME(DIM)                                                                          \
  bool enumerate##DIM(const Shape &, double &, bool, Sink &, std::span<std::uint64_t>);

#if 20 <= FPLLL_MAX_PARALLEL_ENUM_DIM
ENUMFUNCNAME(20)
#endif
#if 30 <= FPLLL_MAX_PARALLEL_ENUM_DIM
ENUMFUNCNAME(30)
#endif
#if 40 <= FPLLL_MAX_PARALLEL_ENUM_DIM
ENUMFUNCNAME(40)
#endif
#if 50 <= FPLLL_MAX_PARALLEL_ENUM_DIM
ENUMFUNCNAME(50)
#endif
#if 60 <= FPLLL_MAX_PARALLEL_ENUM_DIM
ENUMFUNCNAME(60)
#endif
#if 70 <= FPLLL_MAX_PARALLEL_ENUM_DIM
ENUMFUNCNAME(70)
#endif
#if 80 <= FPLLL_MAX_PARALLEL_ENUM_DIM
ENUMFUNCNAME(80)
#endif
#if 90 <= FPLLL_MAX_PARALLEL_ENUM_DIM
ENUMFUNCNAME(90)
#endif
#if 100 <= FPLLL_MAX_PARALLEL_ENUM_DIM
ENUMFUNCNAME(100)
#endif
#if 110 <= FPLLL_MAX_PARALLEL_ENUM_DIM
ENUMFUNCNAME(110)
#endif
#if 120 <= FPLLL_MAX_PARALLEL_ENUM_DIM
ENUMFUNCNAME(120)
#endif
#if 130 <= FPLLL_MAX_PARALLEL_ENUM_DIM
ENUMFUNCNAME(130)
#endif
#if 140 <= FPLLL_MAX_PARALLEL_ENUM_DIM
ENUMFUNCNAME(140)
#endif
#if 150 <= FPLLL_MAX_PARALLEL_ENUM_DIM
ENUMFUNCNAME(150)
#endif
#if 160 <= FPLLL_MAX_PARALLEL_ENUM_DIM
ENUMFUNCNAME(160)
#endif

bool try_enumerate(const Shape &shape, double &maxdist, bool dual, bool findsubsols, Sink &sink,
                   std::span<std::uint64_t> counts_out)
{
  // dual SVP enumeration is not supported by the parallel enumerator
  if (dual)
  {
    return false;
  }

#if FPLLL_MAX_PARALLEL_ENUM_DIM < 20
  // No dimension is handled by the parallel enumerator: 11..19 need at least
  // the dimension-20 dispatch unit.
  (void)shape;
  (void)maxdist;
  (void)findsubsols;
  (void)sink;
  (void)counts_out;
  return false;
#else
  const int dim = shape.dim;
  if (dim <= 10 || dim > FPLLL_MAX_PARALLEL_ENUM_DIM)
  {
    return false;
  }

  const auto udim = static_cast<std::size_t>(dim);
  if (shape.mut.size() != udim * udim || shape.rdiag.size() != udim ||
      (!shape.pruning.empty() && shape.pruning.size() != udim) || counts_out.size() < udim)
  {
    return false;
  }

  const auto run = [&](auto entry_point)
  {
    double bound = maxdist;
    if (!entry_point(shape, bound, findsubsols, sink, counts_out))
    {
      return false;
    }
    maxdist = bound;
    return true;
  };

#if 20 <= FPLLL_MAX_PARALLEL_ENUM_DIM
  if (dim <= 20)
  {
    return run(enumerate20);
  }
#endif
#if 30 <= FPLLL_MAX_PARALLEL_ENUM_DIM
  if (dim <= 30)
  {
    return run(enumerate30);
  }
#endif
#if 40 <= FPLLL_MAX_PARALLEL_ENUM_DIM
  if (dim <= 40)
  {
    return run(enumerate40);
  }
#endif
#if 50 <= FPLLL_MAX_PARALLEL_ENUM_DIM
  if (dim <= 50)
  {
    return run(enumerate50);
  }
#endif
#if 60 <= FPLLL_MAX_PARALLEL_ENUM_DIM
  if (dim <= 60)
  {
    return run(enumerate60);
  }
#endif
#if 70 <= FPLLL_MAX_PARALLEL_ENUM_DIM
  if (dim <= 70)
  {
    return run(enumerate70);
  }
#endif
#if 80 <= FPLLL_MAX_PARALLEL_ENUM_DIM
  if (dim <= 80)
  {
    return run(enumerate80);
  }
#endif
#if 90 <= FPLLL_MAX_PARALLEL_ENUM_DIM
  if (dim <= 90)
  {
    return run(enumerate90);
  }
#endif
#if 100 <= FPLLL_MAX_PARALLEL_ENUM_DIM
  if (dim <= 100)
  {
    return run(enumerate100);
  }
#endif
#if 110 <= FPLLL_MAX_PARALLEL_ENUM_DIM
  if (dim <= 110)
  {
    return run(enumerate110);
  }
#endif
#if 120 <= FPLLL_MAX_PARALLEL_ENUM_DIM
  if (dim <= 120)
  {
    return run(enumerate120);
  }
#endif
#if 130 <= FPLLL_MAX_PARALLEL_ENUM_DIM
  if (dim <= 130)
  {
    return run(enumerate130);
  }
#endif
#if 140 <= FPLLL_MAX_PARALLEL_ENUM_DIM
  if (dim <= 140)
  {
    return run(enumerate140);
  }
#endif
#if 150 <= FPLLL_MAX_PARALLEL_ENUM_DIM
  if (dim <= 150)
  {
    return run(enumerate150);
  }
#endif
#if 160 <= FPLLL_MAX_PARALLEL_ENUM_DIM
  if (dim <= 160)
  {
    return run(enumerate160);
  }
#endif
  return false;
#endif  // FPLLL_MAX_PARALLEL_ENUM_DIM < 20
}

}  // namespace fplll::enumlib
