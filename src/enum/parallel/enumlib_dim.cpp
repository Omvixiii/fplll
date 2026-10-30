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

/* Body of the generated per-dimension translation units: the parent file
   defines ENUMDIMENSION before including this file. */

// FPLLL_MAX_PARALLEL_ENUM_DIM only appears in the #if below, so
// include-cleaner cannot see that this include is needed.  Keep it: without
// it the condition evaluates to false, the generated per-dimension TUs become
// empty and libfplll fails to link.
#include <fplll/fplll_config.h>  // IWYU pragma: keep

#ifdef ENUMDIMENSION
#if ENUMDIMENSION <= FPLLL_MAX_PARALLEL_ENUM_DIM

#include "enumeration.h"
#include "enumlib.h"

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <span>
#include <string>

#include <fplll/defs.h>

namespace fplll::enumlib
{

template <int dimension> struct enumerate_traits
{
  static constexpr int SWIRLY = 1 + (dimension / 20);
};

template <int dimension, bool findsubsols>
bool enumerate_dim_detail(const Shape &shape, double &maxdist, Sink &sink,
                          std::span<std::uint64_t> counts)
{
  static_assert(FPLLL_MAX_ENUM_DIM > dimension,
                "The maximum enumeration dimension needs to be bigger than the parallel "
                "enumeration dimension");
  FPLLL_DEBUG_CHECK(shape.dim == dimension);

  static constexpr int SWIRLY = enumerate_traits<dimension>::SWIRLY;
  using lattice_type          = scratch_t<dimension, SWIRLY, findsubsols>;

  detail::EnumerationSession session;

  globals_t<dimension> globals;
  globals.A.store(maxdist, std::memory_order_relaxed);
  globals.sink = &sink;

  const shape_t<dimension> typed_shape{shape.mut, shape.rdiag, shape.pruning};
  lattice_type lat(typed_shape, globals);

  lat.run(session);

  if constexpr (findsubsols)
  {
    for (int j = 0; j < dimension; ++j)
    {
      if (lat._subsolL[j] < lat._risq[j])
      {
        sink.on_subsolution(
            lat._subsolL[j],
            std::span<const double>(lat._subsol[j].data(), static_cast<std::size_t>(dimension)), j);
      }
    }
  }

  for (int j = 0; j < dimension; ++j)
  {
    counts[static_cast<std::size_t>(j)] = lat._counts[j];
  }

  maxdist = globals.A.load(std::memory_order_relaxed);
  return true;
}

template <int dimension>
bool enumerate_dim(const Shape &shape, double &maxdist, bool findsubsols, Sink &sink,
                   std::span<std::uint64_t> counts)
{
  if (findsubsols)
  {
    return enumerate_dim_detail<dimension, true>(shape, maxdist, sink, counts);
  }
  return enumerate_dim_detail<dimension, false>(shape, maxdist, sink, counts);
}

#define DIMFUNCNAME(DIM) enumerate##DIM
#define DIMFUNC(DIM) DIMFUNCNAME(DIM)

bool DIMFUNC(ENUMDIMENSION)(const Shape &shape, double &maxdist, bool findsubsols, Sink &sink,
                            std::span<std::uint64_t> counts)
{
  switch (shape.dim)
  {
  case ENUMDIMENSION - 9:
    return enumerate_dim<ENUMDIMENSION - 9>(shape, maxdist, findsubsols, sink, counts);
  case ENUMDIMENSION - 8:
    return enumerate_dim<ENUMDIMENSION - 8>(shape, maxdist, findsubsols, sink, counts);
  case ENUMDIMENSION - 7:
    return enumerate_dim<ENUMDIMENSION - 7>(shape, maxdist, findsubsols, sink, counts);
  case ENUMDIMENSION - 6:
    return enumerate_dim<ENUMDIMENSION - 6>(shape, maxdist, findsubsols, sink, counts);
  case ENUMDIMENSION - 5:
    return enumerate_dim<ENUMDIMENSION - 5>(shape, maxdist, findsubsols, sink, counts);
  case ENUMDIMENSION - 4:
    return enumerate_dim<ENUMDIMENSION - 4>(shape, maxdist, findsubsols, sink, counts);
  case ENUMDIMENSION - 3:
    return enumerate_dim<ENUMDIMENSION - 3>(shape, maxdist, findsubsols, sink, counts);
  case ENUMDIMENSION - 2:
    return enumerate_dim<ENUMDIMENSION - 2>(shape, maxdist, findsubsols, sink, counts);
  case ENUMDIMENSION - 1:
    return enumerate_dim<ENUMDIMENSION - 1>(shape, maxdist, findsubsols, sink, counts);
  case ENUMDIMENSION:
    return enumerate_dim<ENUMDIMENSION>(shape, maxdist, findsubsols, sink, counts);
  default:
    // Dimensions outside [ENUMDIMENSION - 9, ENUMDIMENSION] fall through to the
    // diagnostic below, which is the historical behaviour of this dispatch.
    break;
  }

  log_stream() << std::format("[enumlib] dimension {} not handled by the {}-dim unit\n", shape.dim,
                              ENUMDIMENSION);
  return false;
}

}  // namespace fplll::enumlib

#endif
#endif
