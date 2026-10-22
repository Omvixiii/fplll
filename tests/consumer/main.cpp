#include <fplll/fplll.h>

#include <iostream>
#include <sstream>

using fplll::lll_reduction;
using fplll::RED_SUCCESS;
using fplll::ZZ_mat;

int main()
{
  std::istringstream input("[[1 1][1 0]]");
  ZZ_mat<mpz_t> basis;
  input >> basis;
  if (!input)
  {
    std::cerr << "failed to parse the test basis\n";
    return 1;
  }

  const int status = lll_reduction(basis);
  if (status != RED_SUCCESS)
  {
    std::cerr << "lll_reduction failed with status " << status << "\n";
    return 2;
  }

  std::cout << basis;
  return 0;
}
