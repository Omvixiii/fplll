// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <fplll/enum/enumerate.h>
#include <fplll/enum/evaluator.h>
#include <fplll/fplll.h>
#include <fplll/gso/gso.h>
#include <fplll/nr/matrix.h>
#include <fplll/nr/nr.h>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <gmp.h>
#include <span>

namespace
{

// Enumerates the first `d` coordinates of a qary basis and checks the node
// counters.  The seed is fixed, so the enumeration is reproducible.
template <class FT> void check_enumeration_counters(std::size_t d)
{
  INFO("backend: " << fplll::num_type_str<FT>() << ", dimension: " << d << ", seed=0x1337");
  fplll::RandGen::init_with_seed(0x1337);
  fplll::ZZ_mat<mpz_t> A = fplll::ZZ_mat<mpz_t>(100, 100);
  fplll::Z_NR<mpz_t> q;
  q = 7681;
  A.gen_qary(50, q);
  REQUIRE(fplll::succeeded(fplll::lll_reduction(A)));
  fplll::ZZ_mat<mpz_t> U;
  fplll::MatGSO<fplll::Z_NR<mpz_t>, fplll::FP_NR<FT>> M(A, U, U);
  M.update_gso();

  fplll::FastEvaluator<fplll::FP_NR<FT>> evaluator;
  fplll::Enumeration<fplll::Z_NR<mpz_t>, fplll::FP_NR<FT>> enum_obj(M, evaluator);
  fplll::FP_NR<FT> max_dist;
  M.get_r(max_dist, 0, 0);
  max_dist *= 0.99;
  enum_obj.enumerate(0, static_cast<int>(d), max_dist, 0);

  // The per-level counters are sized from the dimension and sum to the total.
  const std::span<const std::uint64_t> nodes = enum_obj.nodes();
  REQUIRE(nodes.size() == d);
  std::uint64_t total = 0;
  for (std::uint64_t count : nodes)
  {
    total += count;
  }
  CHECK(total == enum_obj.get_nodes());

  // Every level of the dimension is reachable through the per-level accessor.
  for (std::size_t i = 0; i < nodes.size(); ++i)
  {
    INFO("level: " << i);
    CHECK(enum_obj.get_nodes(static_cast<int>(i)) == nodes[i]);
  }

  // There is no level past the dimension, so the accessor reports no nodes.
  CHECK(enum_obj.get_nodes(static_cast<int>(d)) == 0);
}

}  // namespace

TEST_CASE("enumeration node counters are consistent in dimension 10", "[enum][counter]")
{
  check_enumeration_counters<double>(10);
}

TEST_CASE("enumeration node counters are consistent in dimension 30", "[enum][counter]")
{
  // A different dimension, so that the sequential as well as the parallel
  // enumerator is exercised.
  check_enumeration_counters<double>(30);
}
