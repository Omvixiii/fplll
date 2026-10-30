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
#include <fplll/enum/enumerate_base.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace fplll
{

void EnumerationBase::allocate(int dim)
{
  d                       = dim;
  const auto levels       = static_cast<std::size_t>(dim);
  const auto inner_levels = levels + 1;

  mut_.assign(levels * levels, 0.0);
  rdiag_.assign(levels, 0.0);
  partdistbounds_.assign(levels, 0.0);

  center_partsums_.assign(levels * inner_levels, 0.0);
  center_partsum_.assign(levels, 0.0);
  center_partsum_begin_.assign(inner_levels, 0);

  partdist_.assign(levels, 0.0);
  center_.assign(levels, 0.0);
  alpha_.assign(levels, 0.0);
  subsoldists_.assign(levels, 0.0);
  x_.assign(levels, 0);
  dx_.assign(levels, 0);
  ddx_.assign(levels, 0);

  node_counts_.assign(levels, 0);
}

template <int kk, int kk_start, bool dualenum, bool findsubsols, bool enable_reset>
inline void EnumerationBase::enumerate_recursive(
    EnumerationBase::opts<kk, kk_start, dualenum, findsubsols, enable_reset>)
{
  /* The per-level state lives in heap blocks allocated once per enumeration.
     Cache the data pointers of the arrays touched by this frame: nothing
     reallocates them during the search, so the per-node accesses stay as cheap
     as they were with fixed-size arrays.  Only the genuinely read-only arrays
     are restrict-qualified; the bound array is rewritten whenever a solution
     updates the enumeration radius. */
  enumf *partdist                 = partdist_.data();
  const enumf *__restrict__ rdiag = rdiag_.data();
  const enumf *bounds             = partdistbounds_.data();
  enumf *center                   = center_.data();
  enumf *alpha                    = alpha_.data();
  enumf *subsoldists              = subsoldists_.data();
  enumf *partsums                 = center_partsums_.data();
  int *partbegin                  = center_partsum_begin_.data();
  std::uint64_t *nodes            = node_counts_.data();
  int *x                          = x_.data();
  int *dx                         = dx_.data();
  int *ddx                        = ddx_.data();

  enumf alphak  = x[kk] - center[kk];
  enumf newdist = partdist[kk] + alphak * alphak * rdiag[kk];

  if (!(newdist <= bounds[kk]))
  {
    return;
  }
  ++nodes[kk];

  alpha[kk] = alphak;
  if (findsubsols && newdist < subsoldists[kk] && newdist != 0.0)
  {
    subsoldists[kk] = newdist;
    process_subsolution(kk, newdist);
  }

  if (kk == 0)
  {
    if (newdist > 0.0 || !is_svp)
    {
      process_solution(newdist);
    }
  }
  else if (enable_reset &&
           kk < reset_depth)  // in CVP, below the max GS vector, we reset the partial distance
  {
    reset(newdist, kk);
    return;
  }
  else
  {
    partdist[kk - 1] = newdist;
    {
      const enumf *__restrict__ mutrow = mut_.data() + static_cast<std::ptrdiff_t>(kk - 1) * d;
      if (dualenum)
      {
        for (int j = partbegin[kk]; j > kk - 1; --j)
        {
          partsums[(kk - 1) * (d + 1) + j] =
              partsums[(kk - 1) * (d + 1) + j + 1] - alpha[j] * mutrow[j];
        }
      }
      else
      {
        for (int j = partbegin[kk]; j > kk - 1; --j)
        {
          partsums[(kk - 1) * (d + 1) + j] =
              partsums[(kk - 1) * (d + 1) + j + 1] - x[j] * mutrow[j];
        }
      }
    }
    if (partbegin[kk] > partbegin[kk - 1])
    {
      partbegin[kk - 1] = partbegin[kk];
    }
    partbegin[kk]  = kk;
    center[kk - 1] = partsums[(kk - 1) * (d + 1) + kk];
    roundto(x[kk - 1], center[kk - 1]);
    dx[kk - 1] = ddx[kk - 1] = ((static_cast<int>(center[kk - 1] >= x[kk - 1]) & 1) << 1) - 1;
  }

  while (true)
  {
    enumerate_recursive(opts<kk - 1, kk_start, dualenum, findsubsols, enable_reset>());

    if (!is_svp || partdist[kk] != 0.0)
    {
      x[kk] += dx[kk];
      ddx[kk] = -ddx[kk];
      dx[kk]  = ddx[kk] - dx[kk];
    }
    else
    {
      ++x[kk];
    }

    enumf alphak2  = x[kk] - center[kk];
    enumf newdist2 = partdist[kk] + alphak2 * alphak2 * rdiag[kk];
    if (!(newdist2 <= bounds[kk]))
    {
      return;
    }
    ++nodes[kk];
    alpha[kk] = alphak2;
    if (kk == 0)
    {
      if (newdist2 > 0.0 || !is_svp)
      {
        process_solution(newdist2);
      }
    }
    else
    {
      partdist[kk - 1]                 = newdist2;
      const enumf *__restrict__ mutrow = mut_.data() + static_cast<std::ptrdiff_t>(kk - 1) * d;
      if (dualenum)
      {
        partsums[(kk - 1) * (d + 1) + kk] =
            partsums[(kk - 1) * (d + 1) + kk + 1] - alpha[kk] * mutrow[kk];
      }
      else
      {
        partsums[(kk - 1) * (d + 1) + kk] =
            partsums[(kk - 1) * (d + 1) + kk + 1] - x[kk] * mutrow[kk];
      }
      if (kk > partbegin[kk - 1])
      {
        partbegin[kk - 1] = kk;
      }
      center[kk - 1] = partsums[(kk - 1) * (d + 1) + kk];
      roundto(x[kk - 1], center[kk - 1]);
      dx[kk - 1] = ddx[kk - 1] = ((static_cast<int>(center[kk - 1] >= x[kk - 1]) & 1) << 1) - 1;
    }
  }
}

template <bool dualenum, bool findsubsols, bool enable_reset>
inline void EnumerationBase::enumerate_recursive_dispatch(int kk)
{
  using enum_recur_type = void (EnumerationBase::*)();

  /* Exactly one entry per level the recursive search can start at (0-based
     indices 0..maxdim-1).  The previous version always generated 2048 entries
     per instantiation, of which at most maxdim were reachable. */
  static constexpr auto lookup = []<std::size_t... I>(std::index_sequence<I...>)
  {
    return std::array<enum_recur_type, sizeof...(I)>{
        &EnumerationBase::enumerate_recursive_wrapper<static_cast<int>(I), dualenum, findsubsols,
                                                      enable_reset>...};
  }(std::make_index_sequence<static_cast<std::size_t>(maxdim)>{});

  (this->*lookup[static_cast<std::size_t>(kk)])();
}

template <bool dualenum, bool findsubsols, bool enable_reset> void EnumerationBase::enumerate_loop()
{
  if (k >= d)
  {
    return;
  }

  center_partsum_begin_[0] = 0;
  for (int i = 0; i < d; ++i)
  {
    center_partsum_begin_[i + 1]                                = d - 1;
    center_partsums_[static_cast<std::size_t>(i) * (d + 1) + d] = center_partsum_[i];
  }

  /* The next few lines provide compatibility between the old iterative and the
   * recursive enumerators.  The recursive enumerator starts one level down
   * from the last position, so it counts an extra node per level during the
   * initial descent.  This is the equivalent of a Babai lift from d-1 down to
   * k; the counts are adjusted here so that the total stays consistent.  See
   * https://github.com/fplll/fplll/pull/442.
   *
   * Note: this adjustment does no checks on the node counts.  It technically
   * wraps around when a count is 0, but adding to these later resets them, so
   * this does not matter in practice. */
  for (int i = k + 1; i < d; i++)
  {
    node_counts_[static_cast<std::size_t>(i)]--;
  }

  k = d - 1;

  enumerate_recursive_dispatch<dualenum, findsubsols, enable_reset>(k);
}

template void EnumerationBase::enumerate_loop<false, false, true>();
template void EnumerationBase::enumerate_loop<false, true, true>();
template void EnumerationBase::enumerate_loop<false, false, false>();
template void EnumerationBase::enumerate_loop<false, true, false>();
template void EnumerationBase::enumerate_loop<true, false, false>();
template void EnumerationBase::enumerate_loop<true, true, false>();

}  // namespace fplll
