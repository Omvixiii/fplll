#include <fplll/fplll.h>

using namespace fplll;

template <class FT> int test_enum(size_t d)
{
  RandGen::init_with_seed(0x1337);
  ZZ_mat<mpz_t> A = ZZ_mat<mpz_t>(100, 100);
  Z_NR<mpz_t> q;
  q = 7681;
  A.gen_qary(50, q);
  if (lll_reduction(A) != RED_SUCCESS)
    return -1;
  ZZ_mat<mpz_t> U;
  MatGSO<Z_NR<mpz_t>, FP_NR<FT>> M(A, U, U, 0);
  M.update_gso();

  FastEvaluator<FP_NR<FT>> evaluator;
  Enumeration<Z_NR<mpz_t>, FP_NR<FT>> enum_obj(M, evaluator);
  FP_NR<FT> max_dist;
  M.get_r(max_dist, 0, 0);
  max_dist *= 0.99;
  enum_obj.enumerate(0, d, max_dist, 0);
  int status = 0;

  // Check that we haven't screwed up the sum
  const auto a   = enum_obj.get_nodes_array();
  uint64_t total = 0;
  for (unsigned int i = 0; i < a.size(); i++)
  {
    total += a[i];
  }

  if (total != enum_obj.get_nodes())
  {
    std::cerr << "test_counter failed: backend=" << num_type_str<FT>()
              << ", case=node-total, dimension=" << d << ", seed=0x1337, expected=" << total
              << ", actual=" << enum_obj.get_nodes() << '\n';
    status |= 1;
  }

  // Check that we haven't overwritten beyond our bounds
  for (unsigned int i = d + 1; i < a.size(); i++)
  {
    if (enum_obj.get_nodes(i) != 0)
    {
      std::cerr << "test_counter failed: backend=" << num_type_str<FT>()
                << ", case=unused-level, dimension=" << d << ", seed=0x1337, level=" << i
                << ", expected=0, actual=" << enum_obj.get_nodes(i) << '\n';
      status |= 1;
    }
  }

  return status;
}

int main()
{
  int status = 0;
  // Different, so that we may delegate to either the local enumerator or the
  // external one.
  status |= test_enum<double>(10);
  status |= test_enum<double>(30);
  return status;
}
