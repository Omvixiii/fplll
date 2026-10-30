// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

// Maintainer-only generator for the compile-time ball-volume table in
// src/pruner/pruner.cpp (not built by Meson).
//
// The double backend of the pruner needs the volume of the unit ball in
// dimensions 0 .. 2046 with the exact values the runtime MPFR path used to
// produce.  C++23 has no constexpr <cmath>, and any independently implemented
// pi/Gamma would round differently, so the literals are produced once by
// replaying that path with 256-bit MPFR and printing the rounded doubles as
// hexadecimal floats (which round-trip exactly, unlike decimal strings).
//
// Usage:
//   c++ -std=c++23 -O2 scripts/gen_pruner_ball_volumes.cpp \
//       $(pkg-config --cflags --libs mpfr) -o /tmp/gen_pruner_ball_volumes
//   /tmp/gen_pruner_ball_volumes > /tmp/ball_volumes.inc
//
// Replace the initializer of `ball_vol_double` in src/pruner/pruner.cpp with
// the printed lines; the unit test compares all 2047 entries with the
// historical runtime path, so a changed MPFR behaviour cannot slip through.
#include <cstddef>
#include <print>
#include <sstream>

#include <mpfr.h>

namespace
{

/** Number of entries of the pruner tables; dimension 453 is the first one
    whose ball volume underflows to +0.0. */
constexpr std::size_t max_pruner_dim = 2047;

}  // namespace

int main()
{
  try
  {
    mpfr_t pi, value, gamma, tmp;
    mpfr_inits2(256, pi, value, gamma, tmp, static_cast<mpfr_ptr>(nullptr));
    mpfr_const_pi(pi, MPFR_RNDN);

    std::size_t emitted = 0;
    bool underflowed    = false;
    for (std::size_t i = 0; i < max_pruner_dim; ++i)
    {
      mpfr_set_ui(tmp, static_cast<unsigned long>(i), MPFR_RNDN);
      mpfr_div_2ui(tmp, tmp, 1, MPFR_RNDN);
      mpfr_pow(value, pi, tmp, MPFR_RNDN);
      mpfr_add_ui(tmp, tmp, 1, MPFR_RNDN);
      mpfr_gamma(gamma, tmp, MPFR_RNDN);
      mpfr_div(value, value, gamma, MPFR_RNDN);

      const double volume = mpfr_get_d(value, MPFR_RNDN);
      if (volume == 0.0)
      {
        // From here on every value stays +0.0; the array's aggregate
        // initialization supplies the rest.
        underflowed = true;
        for (std::size_t j = i + 1; j < max_pruner_dim; ++j)
        {
          mpfr_set_ui(tmp, static_cast<unsigned long>(j), MPFR_RNDN);
          mpfr_div_2ui(tmp, tmp, 1, MPFR_RNDN);
          mpfr_pow(value, pi, tmp, MPFR_RNDN);
          mpfr_add_ui(tmp, tmp, 1, MPFR_RNDN);
          mpfr_gamma(gamma, tmp, MPFR_RNDN);
          mpfr_div(value, value, gamma, MPFR_RNDN);
          if (mpfr_get_d(value, MPFR_RNDN) != 0.0)
          {
            std::println(stderr, "error: dimension {} is nonzero after underflow", j);
            mpfr_clears(pi, value, gamma, tmp, static_cast<mpfr_ptr>(nullptr));
            return 1;
          }
        }
        break;
      }
      // std::format's hexadecimal presentation is not available in every
      // standard library yet, so render through the iostreams' std::hexfloat,
      // which produces the same round-trippable "0x1.921fb54442d18p+1" form as
      // printf("%a").
      std::ostringstream literal;
      literal << std::hexfloat << volume;
      std::println("    {},", literal.str());
      ++emitted;
    }

    mpfr_clears(pi, value, gamma, tmp, static_cast<mpfr_ptr>(nullptr));
    mpfr_free_cache();
    if (!underflowed)
    {
      std::println(stderr, "error: the table never underflowed");
      return 1;
    }
    std::println(stderr, "emitted {} literals; the remaining entries are +0.0", emitted);
  }
  catch (...)
  {
    return 1;
  }
  return 0;
}
