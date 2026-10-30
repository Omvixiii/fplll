// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

/* The public check macros must work in downstream code that does not import
   namespace fplll.  This translation unit deliberately stays in the global
   namespace. */

#include <fplll/fplll.h>  // IWYU pragma: keep

#include <catch2/catch_test_macros.hpp>

#include <string_view>

TEST_CASE("public check macros work outside namespace fplll", "[public-macros]")
{
  FPLLL_CHECK(1 + 1 == 2, "arithmetic");
  FPLLL_DEBUG_CHECK(1 + 1 == 2);
}

TEST_CASE("precondition failures throw a descriptive error", "[public-macros]")
{
  bool threw = false;
  try
  {
    FPLLL_CHECK(1 + 1 == 3, "broken arithmetic: {} + {}", 1, 1);
  }
  catch (const fplll::Error &error)
  {
    threw                          = true;
    const std::string_view message = error.what();
    CHECK(message.find("broken arithmetic: 1 + 1") != std::string_view::npos);
    // The location is the call site of the macro, not the header that implements it.
    CHECK(message.find("test_public_macros.cpp") != std::string_view::npos);
  }
  CHECK(threw);
}

TEST_CASE("debug checks evaluate their argument exactly once when enabled", "[public-macros]")
{
  int calls                  = 0;
  [[maybe_unused]] auto once = [&calls] { return ++calls > 0; };

  FPLLL_DEBUG_CHECK(once());

  // A checked build evaluates the expression once; an unchecked build does not
  // evaluate it at all.  Either way the argument is evaluated at most once.
#ifdef FPLLL_DEBUG_CHECKS
  CHECK(calls == 1);
#else
  CHECK(calls == 0);
#endif
}
