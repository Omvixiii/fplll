// SPDX-FileCopyrightText: 2022 Joe Rowell
// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <fplll/fplll.h>  // IWYU pragma: keep
#include <fplll/nr/nr.h>

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include "test_utils.h"

TEMPLATE_TEST_CASE("FP_NR::ceil returns the least integer not below its argument", "[nr]",
                   FPLLL_TEST_FP_BACKENDS)
{
  // A fixed value is rounded up to the next integer.
  fplll::FP_NR<TestType> value(3.5);
  fplll::FP_NR<TestType> ceiled{};
  ceiled.ceil(value);
  CHECK(ceiled == 4.0);

  // ceil never returns a value that is smaller than its argument.
  value = fplll::RandGenInt::get();
  ceiled.ceil(value);
  CHECK(ceiled >= value);
}
