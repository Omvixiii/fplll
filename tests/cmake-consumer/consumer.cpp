#include <fplll/fplll.h>

int main()
{
  fplll::ZZ_mat<mpz_t> basis(2, 2);
  basis.gen_identity(2);
  return fplll::lll_reduction(basis);
}
