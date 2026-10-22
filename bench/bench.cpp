#include <fplll/fplll.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using fplll::lll_reduction;
using fplll::RED_SUCCESS;
using fplll::ZZ_mat;

namespace
{

ZZ_mat<mpz_t> make_intrel_basis(int dim, int bits)
{
  ZZ_mat<mpz_t> basis(dim, dim + 1);
  basis.gen_intrel(bits);
  return basis;
}

double median(std::vector<double> values)
{
  std::sort(values.begin(), values.end());
  const std::size_t middle = values.size() / 2;
  if (values.size() % 2 == 0)
    return 0.5 * (values[middle - 1] + values[middle]);
  return values[middle];
}

double measure_lll(int dim, int bits, int repeats)
{
  std::vector<double> samples;
  samples.reserve(static_cast<std::size_t>(repeats));
  for (int i = 0; i < repeats; ++i)
  {
    ZZ_mat<mpz_t> basis                               = make_intrel_basis(dim, bits);
    const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    const int status                                  = lll_reduction(basis);
    const std::chrono::steady_clock::time_point stop  = std::chrono::steady_clock::now();
    if (status != RED_SUCCESS)
    {
      std::fprintf(stderr, "lll_reduction failed with status %d (dim=%d)\n", status, dim);
      std::exit(2);
    }
    samples.push_back(std::chrono::duration<double>(stop - start).count());
  }
  return median(samples);
}

std::vector<int> parse_dims(const char *text)
{
  std::vector<int> dims;
  const char *cursor = text;
  while (*cursor != '\0')
  {
    char *end        = nullptr;
    const long value = std::strtol(cursor, &end, 10);
    if (end == cursor || value <= 0)
    {
      std::fprintf(stderr, "invalid --dims value: %s\n", text);
      std::exit(1);
    }
    dims.push_back(static_cast<int>(value));
    cursor = *end == ',' ? end + 1 : end;
  }
  return dims;
}

}  // namespace

int main(int argc, char **argv)
{
  std::vector<int> dims;
  dims.push_back(40);
  dims.push_back(60);
  dims.push_back(80);
  int bits    = 30;
  int repeats = 3;

  for (int i = 1; i < argc; ++i)
  {
    if (std::strcmp(argv[i], "--dims") == 0 && i + 1 < argc)
      dims = parse_dims(argv[++i]);
    else if (std::strcmp(argv[i], "--bits") == 0 && i + 1 < argc)
      bits = static_cast<int>(std::strtol(argv[++i], nullptr, 10));
    else if (std::strcmp(argv[i], "--repeats") == 0 && i + 1 < argc)
      repeats = static_cast<int>(std::strtol(argv[++i], nullptr, 10));
    else
    {
      std::fprintf(stderr, "usage: %s [--dims 40,60,80] [--bits 30] [--repeats 3]\n", argv[0]);
      return 1;
    }
  }
  if (bits <= 0 || repeats <= 0)
  {
    std::fprintf(stderr, "--bits and --repeats must be positive\n");
    return 1;
  }

  // Warm up shared state (GMP allocators, strategy data) before measuring.
  {
    ZZ_mat<mpz_t> warmup = make_intrel_basis(20, bits);
    if (lll_reduction(warmup) != RED_SUCCESS)
      return 2;
  }

  for (std::size_t i = 0; i < dims.size(); ++i)
  {
    const double seconds = measure_lll(dims[i], bits, repeats);
    std::printf("{\"case\":\"lll\",\"dim\":%d,\"bits\":%d,\"repeats\":%d,\"seconds\":%.9f}\n",
                dims[i], bits, repeats, seconds);
  }
  return 0;
}
