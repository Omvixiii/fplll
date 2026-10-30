// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <fplll/nr/matrix.h>
#include <fplll/nr/nr.h>
#include <fplll/threadpool.h>

#include "../src/detail/worker_pool.h"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <gmp.h>
#include <new>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

#include <fplll/fplll.h>

namespace
{

// The process-wide pool is created on first use, so reading the initial value
// must happen before any test case runs rather than inside one.
const int initial_thread_count = fplll::get_threads();

int hardware_threads()
{
  return std::max(1, static_cast<int>(std::thread::hardware_concurrency()));
}

int parallel_threads() { return std::min(4, hardware_threads()); }

// Tests that open a session must restore the thread count only after the
// session is gone.  Declare this guard before the session so that it is
// destroyed after it.
struct ResetThreadCount
{
  ~ResetThreadCount() { fplll::set_threads(1); }
};

// Wait until `predicate` holds or the deadline passes.  Tests that wait for a
// blocked thread use this instead of sleeping for a fixed time.
template <typename Predicate> bool wait_until(Predicate predicate)
{
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!predicate())
  {
    if (std::chrono::steady_clock::now() >= deadline)
    {
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return true;
}

std::optional<double> shortest_squared_norm(const fplll::ZZ_mat<mpz_t> &basis, int threads)
{
  fplll::set_threads(threads);
  fplll::ZZ_mat<mpz_t> reduced = basis;
  if (!fplll::succeeded(fplll::lll_reduction(reduced)))
  {
    return std::nullopt;
  }
  std::vector<fplll::Z_NR<mpz_t>> coordinates;
  const fplll::RedStatus status = fplll::shortest_vector(
      reduced, coordinates, fplll::SVPOptions{.method = fplll::SVPMethod::fast});
  if (!fplll::succeeded(status))
  {
    return std::nullopt;
  }
  std::vector<fplll::Z_NR<mpz_t>> solution;
  fplll::vector_matrix_product(solution, coordinates, reduced);
  fplll::Z_NR<mpz_t> norm;
  fplll::Z_NR<mpz_t> square;
  for (const auto &coordinate : solution)
  {
    square.mul(coordinate, coordinate);
    norm.add(norm, square);
  }
  return norm.get_d();
}

}  // namespace

TEST_CASE("the thread count starts at one and set_threads clamps its argument", "[threadpool]")
{
  CHECK(initial_thread_count == 1);

  CHECK(fplll::set_threads(1) == 1);
  CHECK(fplll::set_threads(0) == 1);
  CHECK(fplll::set_threads(-2) == 1);

  const int hw = hardware_threads();
  CHECK(fplll::set_threads(hw + 100) == hw);
  CHECK(fplll::get_threads() == hw);
  CHECK(fplll::set_threads(-1) == hw);
  CHECK(fplll::set_threads(1) == 1);
}

TEST_CASE("a session snapshots the committed thread count", "[threadpool]")
{
  const int total = fplll::set_threads(parallel_threads());
  {
    fplll::detail::EnumerationSession session;
    CHECK(session.concurrency() == total);
    CHECK(fplll::get_threads() == total);
    // The owner is inside the session, so config changes are rejected.
    CHECK_THROWS_AS(fplll::set_threads(1), std::logic_error);
    CHECK(fplll::get_threads() == total);
  }
  // Once the session is gone the config can change again.
  CHECK(fplll::set_threads(1) == 1);
}

TEST_CASE("run_workers runs each slot once and the caller takes the last slot", "[threadpool]")
{
  const int total = fplll::set_threads(parallel_threads());
  if (total < 2)
  {
    return;
  }

  ResetThreadCount reset_threads;
  fplll::detail::EnumerationSession session;
  const std::thread::id caller = std::this_thread::get_id();

  for (int batch = 0; batch < 2; ++batch)
  {
    std::vector<std::atomic<int>> hits(static_cast<std::size_t>(total));
    for (auto &hit : hits)
    {
      hit.store(0);
    }
    std::vector<std::thread::id> slot_thread(static_cast<std::size_t>(total));

    session.run_workers(
        [&hits, &slot_thread](int slot)
        {
          hits[static_cast<std::size_t>(slot)].fetch_add(1);
          slot_thread[static_cast<std::size_t>(slot)] = std::this_thread::get_id();
        });

    for (int slot = 0; slot < total; ++slot)
    {
      CHECK(hits[static_cast<std::size_t>(slot)].load() == 1);
    }
    CHECK(slot_thread[static_cast<std::size_t>(total - 1)] == caller);
    for (int slot = 0; slot < total - 1; ++slot)
    {
      CHECK(slot_thread[static_cast<std::size_t>(slot)] != caller);
    }
  }
}

TEST_CASE("a single-slot session runs the work on the caller", "[threadpool]")
{
  CHECK(fplll::set_threads(1) == 1);
  fplll::detail::EnumerationSession session;
  REQUIRE(session.concurrency() == 1);

  const std::thread::id caller = std::this_thread::get_id();
  int runs                     = 0;
  int observed_slot            = -1;
  std::thread::id run_thread;
  session.run_workers(
      [&runs, &run_thread, &observed_slot](int slot)
      {
        ++runs;
        observed_slot = slot;
        run_thread    = std::this_thread::get_id();
      });
  CHECK(runs == 1);
  CHECK(observed_slot == 0);
  CHECK(run_thread == caller);

  // The single-slot path keeps the same reentry contract as the pooled one.
  CHECK_THROWS_AS(session.run_workers([](int) { fplll::set_threads(1); }), std::logic_error);
}

TEST_CASE("reentrant executor calls are rejected", "[threadpool]")
{
  fplll::set_threads(parallel_threads());
  ResetThreadCount reset_threads;
  fplll::detail::EnumerationSession session;

  // From the calling thread.
  CHECK_THROWS_AS(fplll::set_threads(1), std::logic_error);
  CHECK_THROWS_AS((void)fplll::detail::EnumerationSession(), std::logic_error);

  // From a worker (slot 0) and from the caller's slot (the last one).
  CHECK_THROWS_AS(session.run_workers([](int) { fplll::set_threads(1); }), std::logic_error);
  CHECK_THROWS_AS(session.run_workers([](int) { fplll::detail::EnumerationSession nested; }),
                  std::logic_error);
  CHECK_THROWS_AS(session.run_workers([](int) { fplll::set_threads(1); }), std::logic_error);
}

TEST_CASE("run_workers joins every copy when one throws", "[threadpool]")
{
  const int total = fplll::set_threads(parallel_threads());
  ResetThreadCount reset_threads;
  fplll::detail::EnumerationSession session;

  std::atomic<int> copies{0};
  CHECK_THROWS_AS(session.run_workers(
                      [&copies](int)
                      {
                        ++copies;
                        throw std::runtime_error("boom");
                      }),
                  std::runtime_error);
  CHECK(copies.load() == total);

  // A throwing batch does not cancel the session; the next one runs normally.
  std::atomic<int> runs{0};
  session.run_workers([&runs](int) { ++runs; });
  CHECK(runs.load() == total);
}

TEST_CASE("run_workers joins the workers when the caller throws", "[threadpool]")
{
  const int total = fplll::set_threads(parallel_threads());
  ResetThreadCount reset_threads;
  fplll::detail::EnumerationSession session;

  std::atomic<int> workers{0};
  CHECK_THROWS_AS(session.run_workers(
                      [&workers, total](int slot)
                      {
                        if (slot == total - 1)
                        {
                          throw std::runtime_error("caller boom");
                        }
                        ++workers;
                      }),
                  std::runtime_error);
  CHECK(workers.load() == total - 1);
}

TEST_CASE("a partial submission failure joins the accepted copies", "[threadpool]")
{
  const int total = fplll::set_threads(parallel_threads());
  if (total < 3)
  {
    return;
  }

  ResetThreadCount reset_threads;
  fplll::detail::EnumerationSession session;
  std::atomic<int> started{0};
  std::atomic<bool> caller_ran{false};

  // Accept task 0, then fail while submitting task 1.
  fplll::detail::set_submit_failure_for_testing(1);
  CHECK_THROWS_AS(session.run_workers(
                      [&started, &caller_ran, total](int slot)
                      {
                        if (slot == total - 1)
                        {
                          caller_ran = true;
                        }
                        ++started;
                      }),
                  std::bad_alloc);
  fplll::detail::set_submit_failure_for_testing(-1);

  CHECK(started.load() == 1);
  CHECK_FALSE(caller_ran.load());

  // The session is still usable afterwards.
  std::atomic<int> runs{0};
  session.run_workers([&runs](int) { ++runs; });
  CHECK(runs.load() == total);
}

TEST_CASE("a failed pool construction keeps the old configuration", "[threadpool]")
{
  if (hardware_threads() < 3)
  {
    return;
  }

  CHECK(fplll::set_threads(2) == 2);
  const int before = fplll::detail::pool_constructions_for_testing();

  fplll::detail::fail_next_pool_construction_for_testing();
  CHECK_THROWS_AS(fplll::set_threads(3), std::bad_alloc);
  CHECK(fplll::get_threads() == 2);

  // The old pool still runs, and no worker pool was created for the failed
  // candidate.
  {
    ResetThreadCount reset_threads;
    fplll::detail::EnumerationSession session;
    REQUIRE(session.concurrency() == 2);
    std::atomic<int> runs{0};
    session.run_workers([&runs](int) { ++runs; });
    CHECK(runs.load() == 2);
    CHECK(fplll::detail::pool_constructions_for_testing() == before);
  }

  // A later retry works.
  CHECK(fplll::set_threads(3) == 3);
  CHECK(fplll::get_threads() == 3);
  fplll::set_threads(1);
}

TEST_CASE("set_threads waits for a whole session, including between batches", "[threadpool]")
{
  const int total = fplll::set_threads(2);
  REQUIRE(total == 2);

  std::atomic<bool> done{false};
  int new_threads = 0;
  std::thread waiter;

  {
    fplll::detail::EnumerationSession session;
    std::atomic<int> runs{0};
    session.run_workers([&runs](int) { ++runs; });
    CHECK(runs.load() == 2);

    waiter = std::thread(
        [&done, &new_threads]
        {
          new_threads = fplll::set_threads(3);
          done        = true;
        });

    // The batch above has finished, but the session is still open: the config
    // change must wait for the session, not just for the batch.
    REQUIRE(wait_until([] { return fplll::detail::threads_lock_waiters_for_testing() > 0; }));
    CHECK_FALSE(done.load());
    CHECK(fplll::get_threads() == 2);

    session.run_workers([&runs](int) { ++runs; });
    CHECK(runs.load() == 4);
    CHECK_FALSE(done.load());
  }

  waiter.join();
  CHECK(done.load());
  CHECK(new_threads == 3);
  CHECK(fplll::get_threads() == 3);
  CHECK(fplll::set_threads(1) == 1);
}

TEST_CASE("a second session waits for the first one", "[threadpool]")
{
  CHECK(fplll::set_threads(2) == 2);

  std::atomic<bool> done{false};
  std::thread second_session_thread;

  {
    fplll::detail::EnumerationSession first;

    second_session_thread = std::thread(
        [&done]
        {
          fplll::detail::EnumerationSession second;
          second.run_workers([](int) {});
          done = true;
        });

    REQUIRE(wait_until([] { return fplll::detail::threads_lock_waiters_for_testing() > 0; }));
    CHECK_FALSE(done.load());
  }

  second_session_thread.join();
  CHECK(done.load());
  CHECK(fplll::set_threads(1) == 1);
}

TEST_CASE("the pool is released and rebuilt for one and many threads", "[threadpool]")
{
  CHECK(fplll::set_threads(1) == 1);

  for (const int requested : {parallel_threads(), 1, parallel_threads(), 1})
  {
    const int total = fplll::set_threads(requested);

    fplll::detail::EnumerationSession session;
    REQUIRE(session.concurrency() == total);
    std::atomic<int> runs{0};
    session.run_workers([&runs](int) { ++runs; });
    CHECK(runs.load() == total);
  }

  fplll::set_threads(1);
}

TEST_CASE("parallel enumeration agrees with single-threaded enumeration", "[threadpool]")
{
  const int total = fplll::set_threads(parallel_threads());
  if (total < 2)
  {
    return;
  }

  fplll::RandGen::init_with_seed(0x1337);
  fplll::ZZ_mat<mpz_t> basis(20, 20);
  fplll::Z_NR<mpz_t> q;
  q = 65537;
  basis.gen_qary(10, q);

  for (const int dim : {11, 12, 20})
  {
    fplll::ZZ_mat<mpz_t> sub_basis(static_cast<int>(dim), static_cast<int>(dim));
    for (int row = 0; row < dim; ++row)
    {
      for (int col = 0; col < dim; ++col)
      {
        sub_basis[row][col] = basis[row][col];
      }
    }

    const std::optional<double> single = shortest_squared_norm(sub_basis, 1);
    REQUIRE(single.has_value());
    for (const int threads : {2, parallel_threads()})
    {
      CAPTURE(dim, threads);
      const std::optional<double> multi = shortest_squared_norm(sub_basis, threads);
      REQUIRE(multi.has_value());
      CHECK(single == multi);
    }
    // Single-threaded enumeration must also keep working after the sessions
    // above released their run locks.
    CHECK(fplll::set_threads(1) == 1);
  }

  fplll::set_threads(1);
}
