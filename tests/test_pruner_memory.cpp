#include <fplll/fplll.h>

#include <iostream>

namespace
{
void *(*original_allocate)(size_t);
void *(*original_reallocate)(void *, size_t, size_t);
void (*original_free)(void *, size_t);
long outstanding_allocations = 0;

void *count_allocate(size_t size)
{
  ++outstanding_allocations;
  return original_allocate(size);
}

void *count_reallocate(void *pointer, size_t old_size, size_t new_size)
{
  return original_reallocate(pointer, old_size, new_size);
}

void count_free(void *pointer, size_t size)
{
  --outstanding_allocations;
  original_free(pointer, size);
}
}  // namespace

int main()
{
  mpfr_free_cache();
  mp_get_memory_functions(&original_allocate, &original_reallocate, &original_free);
  // These wrappers preserve the original allocator's pointer representation.
  mp_set_memory_functions(count_allocate, count_reallocate, count_free);
  {
    fplll::Pruner<fplll::FP_NR<double>> pruner(4);
  }
  mpfr_free_cache();
  mp_set_memory_functions(original_allocate, original_reallocate, original_free);
  if (outstanding_allocations != 0)
  {
    std::cerr << "Pruner initialization bypassed the GMP free callback for "
              << outstanding_allocations << " allocations\n";
    return 1;
  }
  return 0;
}
