// SPDX-FileCopyrightText: 2026 Omvixiii
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef TEST_UTILS_H
#define TEST_UTILS_H

// FPLLL_WITH_QD is only used in the #if below, so include-cleaner flags this
// header as unused; it must stay for QD-enabled builds.
#include <fplll/fplll.h>  // IWYU pragma: keep

#include <fstream>
#include <iostream>

/**
   @brief Read T from `input_filename`.

   @param X T (T is usually a ZZ_mat<ZT> or a std::vector<Z_NR<ZT>>
   @param input_filename
   @return zero if the file is correctly read, 1 otherwise.
*/
template <class T> int read_file(T &X, const char *input_filename)
{
  int status = 0;
  std::ifstream is;
  is.exceptions(std::ifstream::failbit | std::ifstream::badbit);
  try
  {
    is.open(input_filename);
    is >> X;
    is.close();
  }
  catch (const std::ifstream::failure &)
  {
    status = 1;
    std::cerr << "Error by reading " << input_filename << "." << '\n';
  }

  return status;
}

/**
   @brief Floating-point backends supported by this build.

   Pass this to TEMPLATE_TEST_CASE in tests that exercise every backend, so
   that the conditionally compiled backends stay in one place.
*/
#if defined(FPLLL_WITH_QD)
#define FPLLL_TEST_FP_BACKENDS mpfr_t, double, dd_real, qd_real
#else
#define FPLLL_TEST_FP_BACKENDS mpfr_t, double
#endif

#endif /* TEST_UTILS_H */
