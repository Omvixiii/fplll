// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <fplll/fplll.h>
#include <fplll/nr/nr.h>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <vector>

namespace
{
void *(*original_allocate)(std::size_t);
void *(*original_reallocate)(void *, std::size_t, std::size_t);
void (*original_free)(void *, std::size_t);
long outstanding_allocations = 0;

void *count_allocate(std::size_t size)
{
  ++outstanding_allocations;
  return original_allocate(size);
}

void *count_reallocate(void *pointer, std::size_t old_size, std::size_t new_size)
{
  return original_reallocate(pointer, old_size, new_size);
}

void count_free(void *pointer, std::size_t size)
{
  --outstanding_allocations;
  original_free(pointer, size);
}

// Installs the counting allocation functions for its lifetime and restores the
// original ones afterwards, so that a throwing test cannot poison GMP.
struct gmp_allocation_counter
{
  gmp_allocation_counter()
  {
    mp_get_memory_functions(&original_allocate, &original_reallocate, &original_free);
    // These wrappers preserve the original allocator's pointer representation.
    mp_set_memory_functions(count_allocate, count_reallocate, count_free);
  }

  ~gmp_allocation_counter()
  {
    mp_set_memory_functions(original_allocate, original_reallocate, original_free);
  }

  gmp_allocation_counter(const gmp_allocation_counter &)            = delete;
  gmp_allocation_counter &operator=(const gmp_allocation_counter &) = delete;
};
}  // namespace

TEST_CASE("pruner construction releases its GMP allocations", "[pruner]")
{
  mpfr_free_cache();
  long leaked_allocations = 0;
  {
    gmp_allocation_counter counter;
    {
      fplll::Pruner<fplll::FP_NR<double>> pruner(4);
    }
    mpfr_free_cache();
    leaked_allocations = outstanding_allocations;
  }
  INFO("Pruner initialization bypassed the GMP free callback");
  CAPTURE(leaked_allocations);
  CHECK(leaked_allocations == 0);
}

// PRU-10: the pruning tables are halved to d = n / 2 and the expected-solutions
// formula indexes ipv[2 * d - 1], so a dimension below two is a precondition
// violation rather than a degenerate input.  It used to read out of bounds
// (BUG-02) or divide by zero depending on the metric.
TEST_CASE("pruner rejects a block dimension below two", "[pruner]")
{
  CHECK_THROWS_AS((fplll::Pruner<fplll::FP_NR<double>>(0)), fplll::Error);
  CHECK_THROWS_AS((fplll::Pruner<fplll::FP_NR<double>>(1)), fplll::Error);
  CHECK_THROWS_AS((fplll::svp_probability<fplll::FP_NR<double>>(std::vector<double>{1.0})),
                  fplll::Error);
  CHECK_NOTHROW(fplll::Pruner<fplll::FP_NR<double>>(2));
}
