/* ---------------------------
   Random generator for mpz_t
   --------------------------- */

#include <chrono>
#include <cstdint>
#include <mutex>
#include <random>

#ifndef FPLLL_NR_RAND_H
#define FPLLL_NR_RAND_H

FPLLL_BEGIN_NAMESPACE

class RandGenInt
{
public:
  static void seed(std::uint64_t seed)
  {
    std::lock_guard<std::mutex> lock(mutex());
    engine().seed(seed);
  }

  static int get()
  {
    std::lock_guard<std::mutex> lock(mutex());
    return std::uniform_int_distribution<int>()(engine());
  }

  static int get_bit()
  {
    std::lock_guard<std::mutex> lock(mutex());
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
    // Deterministic default, matching the historical unseeded rand() stream.
    static std::mt19937_64 generator(0x9E3779B97F4A7C15ULL);
    return generator;
  }
};

class RandGen
{
public:
  static void init()
  {
    initialized = true;
    gmp_randinit_default(gmp_state);
  }
  static void init_with_seed(unsigned long seed)
  {
    init();
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
  static gmp_randstate_t &get_gmp_state()
  {
    if (!initialized)
      init();
    return gmp_state;
  }
  static gmp_randstate_t gmp_state;

private:
  static bool initialized;
};

FPLLL_END_NAMESPACE

#endif
