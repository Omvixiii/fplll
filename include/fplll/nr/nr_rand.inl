// SPDX-FileCopyrightText: 2016 Damien Stehle
// SPDX-FileCopyrightText: 2016-2019 Martin R. Albrecht
// SPDX-FileCopyrightText: 2016 Shi Bai
// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

/* ---------------------------
   Random generator for mpz_t
   --------------------------- */

#ifndef FPLLL_NR_RAND_H
#define FPLLL_NR_RAND_H

#include <gmp.h>

#include <chrono>
#include <cstdint>
#include <mutex>
#include <random>

namespace fplll
{

/** Random generator for small integers (used where a bounded number of
    random bits is enough). Deterministic unless explicitly seeded. */
class RandGenInt
{
public:
  static void seed(std::uint64_t seed)
  {
    std::scoped_lock lock(mutex());
    engine().seed(seed);
  }

  static int get()
  {
    std::scoped_lock lock(mutex());
    return std::uniform_int_distribution<int>()(engine());
  }

  /** Returns -1 or 1 with equal probability. */
  static int get_bit()
  {
    std::scoped_lock lock(mutex());
    return std::uniform_int_distribution<int>(0, 1)(engine()) == 0 ? -1 : 1;
  }

private:
  static std::mutex &mutex()
  {
    static std::mutex state_mutex;
    return state_mutex;
  }

  static std::mt19937_64 &engine()
  {
    // Deliberate deterministic default seed; explicit seeding is available.
    // NOLINTNEXTLINE(bugprone-random-generator-seed)
    static std::mt19937_64 generator(0x9E3779B97F4A7C15ULL);
    return generator;
  }
};

/** Global GMP random state used for arbitrary-precision random numbers. */
class RandGen
{
public:
  static void init()
  {
    std::call_once(init_flag(), [] { gmp_randinit_default(gmp_state); });
  }

  static void init_with_seed(unsigned long seed)
  {
    init();
    std::scoped_lock lock(mutex());
    gmp_randseed_ui(gmp_state, seed);
    RandGenInt::seed(seed);
  }

  static void init_with_time()
  {
    std::random_device device;
    const std::uint64_t random_bits =
        (static_cast<std::uint64_t>(device()) << 32) ^ static_cast<std::uint64_t>(device());
    const std::uint64_t clock_bits = static_cast<std::uint64_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    init_with_seed(static_cast<unsigned long>(random_bits ^ clock_bits));
  }

  /* GMP randstates are not thread-safe; hold this mutex while touching
     get_gmp_state().  The Z_NR random helpers take it internally. */
  static std::mutex &mutex()
  {
    static std::mutex state_mutex;
    return state_mutex;
  }

  static gmp_randstate_t &get_gmp_state()
  {
    init();
    return gmp_state;
  }

  /* Defined in src/util.cpp: exactly one definition per program. */
  static gmp_randstate_t gmp_state;

private:
  static std::once_flag &init_flag()
  {
    static std::once_flag flag;
    return flag;
  }
};

}  // namespace fplll

#endif
