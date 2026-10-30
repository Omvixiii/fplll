// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

/* Unit tests for the enumeration search state.  Including the parallel
   enumerator's header directly keeps these checks deterministic: they do not
   depend on a lattice or on a particular search path.  This is an internal
   test seam: src/enum/parallel/enumeration.h is not an installed header. */

#include "../src/enum/parallel/enumeration.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <span>

namespace
{

constexpr int N = 8;

using shape_type   = fplll::enumlib::shape_t<N>;
using globals_type = fplll::enumlib::globals_t<N>;
using scratch_type = fplll::enumlib::scratch_t<N, 1, false>;

/* Weights with exact binary representations, so products with the bounds are
   computed and compared without rounding. */
constexpr double weights[N] = {0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.0, 2.25};

std::array<double, N> mu{};
std::array<double, N> rdiag{};
std::array<double, N> pruning{};

shape_type make_shape()
{
  for (int k = 0; k < N; ++k)
  {
    pruning[static_cast<std::size_t>(k)] = weights[k];
  }
  return shape_type{.mu      = std::span<const double>(mu),
                    .rdiag   = std::span<const double>(rdiag),
                    .pruning = std::span<const double>(pruning)};
}

void check_pruning_bounds(const scratch_type &lat, double bound)
{
  REQUIRE(lat._a == bound);
  for (int k = 0; k < N; ++k)
  {
    REQUIRE(lat._aa[static_cast<std::size_t>(k)] == bound * weights[k]);
  }
}

}  // namespace

TEST_CASE("refreshing the shared bound rebuilds the pruning bounds", "[enum][bound]")
{
  globals_type globals;
  scratch_type lat(make_shape(), globals);

  globals.A.store(0.0);
  lat._a = 0.0;
  lat._update_aa();

  /* The bound moved: the search state adopts it and rebuilds the bounds. */
  globals.A.store(100.0);
  lat.refresh_bound();
  check_pruning_bounds(lat, 100.0);

  /* A tighter bound replaces the previous one in place. */
  globals.A.store(25.0);
  lat.refresh_bound();
  check_pruning_bounds(lat, 25.0);

  /* An unchanged bound leaves the cached state alone. */
  const scratch_type::fltrow_t cached_aa = lat._aa;
  globals.A.store(25.0);
  lat.refresh_bound();
  CHECK(lat._aa == cached_aa);
}

TEST_CASE("refresh reads the bound with the same atomic publication as evaluation", "[enum][bound]")
{
  globals_type globals;
  scratch_type lat(make_shape(), globals);

  globals.A.store(4.0);
  lat._a = 4.0;
  lat._update_aa();
  check_pruning_bounds(lat, 4.0);

  /* Publishing through the same atomic also updates the search state. */
  globals.A.store(9.0);
  lat.refresh_bound();
  check_pruning_bounds(lat, 9.0);
}
