// SPDX-FileCopyrightText: 2019 Marc Stevens
// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <fplll/threadpool.h>

#include "detail/worker_pool.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace fplll
{

namespace
{

/* Only construction (worker count), submit_task() -> std::future and the
   destructor that joins the workers are needed here; src/detail/worker_pool.h
   implements exactly that on std::jthread. */
using Pool = detail::worker_pool;

/* The role of the current thread with respect to the executor.  Calls that
   would deadlock or nest a session are rejected before any lock is taken. */
enum class Role : std::uint8_t
{
  external,
  session_owner,
  worker,
};

thread_local Role current_role = Role::external;

class RoleGuard
{
public:
  explicit RoleGuard(Role role) : previous_(current_role) { current_role = role; }
  RoleGuard(const RoleGuard &)            = delete;
  RoleGuard &operator=(const RoleGuard &) = delete;
  ~RoleGuard() { current_role = previous_; }

private:
  Role previous_;
};

/* Process-wide state.  The pool is only created lazily when more than one
   thread is requested, and it is only ever touched while holding run_mutex.
   Nothing here is destroyed from the pool itself, so the lock and the pool
   stay valid for the whole library lifetime. */
struct Runtime
{
  std::mutex run_mutex;
  std::atomic<int> threads{1};
  std::unique_ptr<Pool> pool;
};

Runtime &runtime()
{
  static Runtime instance;
  return instance;
}

int hardware_threads()
{
  return std::max(1, static_cast<int>(std::thread::hardware_concurrency()));
}

/* --- In-tree test injection points ------------------------------------- */
#if defined(FPLLL_TESTING)
std::atomic<int> submit_failure_after{-1};
std::atomic<bool> fail_next_pool_construction{false};
std::atomic<int> threads_lock_waiters{0};
std::atomic<int> pool_constructions{0};
#endif

}  // namespace

namespace detail
{

EnumerationSession::EnumerationSession() : run_lock_(runtime().run_mutex, std::defer_lock)
{
  if (current_role != Role::external)
  {
    throw std::logic_error("an enumeration session must be created outside of the executor");
  }

#if defined(FPLLL_TESTING)
  threads_lock_waiters.fetch_add(1, std::memory_order_relaxed);
#endif
  run_lock_.lock();
#if defined(FPLLL_TESTING)
  threads_lock_waiters.fetch_sub(1, std::memory_order_relaxed);
#endif

  concurrency_ = runtime().threads.load(std::memory_order_acquire);
  current_role = Role::session_owner;
}

EnumerationSession::~EnumerationSession()
{
  current_role = Role::external;
  /* The run_lock_ member releases the run lock during destruction, which lets
     a waiting enumeration or set_threads() call proceed. */
}

int EnumerationSession::concurrency() const noexcept { return concurrency_; }

void EnumerationSession::run_workers(const std::function<void(int)> &work)
{
  if (current_role != Role::session_owner)
  {
    throw std::logic_error("run_workers() must be called from the thread that created the session");
  }

  const int total = concurrency_;

  auto invoke = [&work](int slot)
  {
    RoleGuard guard(Role::worker);
    work(slot);
  };

  if (total == 1)
  {
    /* The single-slot path keeps the same role checks and exception handling
       as the worker path, so callbacks cannot tell the two apart. */
    invoke(0);
    return;
  }

  Pool *pool = runtime().pool.get();
  if (pool == nullptr)
  {
    throw std::logic_error("the parallel enumeration pool is missing");
  }

  std::vector<std::future<void>> futures;
  futures.reserve(static_cast<std::size_t>(total - 1));

  std::exception_ptr first_error;
  auto record = [&first_error](std::exception_ptr error)
  {
    if (!first_error)
    {
      first_error = std::move(error);
    }
  };

#if defined(FPLLL_TESTING)
  const int failure_after = submit_failure_after.load(std::memory_order_relaxed);
#endif
  try
  {
    for (int slot = 0; slot < total - 1; ++slot)
    {
#if defined(FPLLL_TESTING)
      if (failure_after >= 0 && slot == failure_after)
      {
        throw std::bad_alloc();
      }
#endif
      futures.push_back(pool->submit_task([invoke, slot] { invoke(slot); }));
    }

    /* The calling thread always takes the last slot.  A submission failure
       skips this call but keeps the exception recorded below. */
    invoke(total - 1);
  }
  catch (...)
  {
    record(std::current_exception());
  }

  /* Get every accepted task, even when an earlier one failed: the caller may
     only return once no task touches `work` any more. */
  for (auto &future : futures)
  {
    try
    {
      future.get();
    }
    catch (...)
    {
      record(std::current_exception());
    }
  }

  if (first_error)
  {
    std::rethrow_exception(first_error);
  }
}

#if defined(FPLLL_TESTING)
void set_submit_failure_for_testing(int accepted_tasks)
{
  submit_failure_after.store(accepted_tasks, std::memory_order_relaxed);
}

void fail_next_pool_construction_for_testing()
{
  fail_next_pool_construction.store(true, std::memory_order_relaxed);
}

int threads_lock_waiters_for_testing()
{
  return threads_lock_waiters.load(std::memory_order_relaxed);
}

int pool_constructions_for_testing() { return pool_constructions.load(std::memory_order_relaxed); }
#endif

}  // namespace detail

int get_threads() { return runtime().threads.load(std::memory_order_relaxed); }

int set_threads(int th)
{
  if (current_role != Role::external)
  {
    throw std::logic_error("set_threads() must not be called from an enumeration callback");
  }

  int hw = hardware_threads();
  if (th == -1 || th > hw)
  {
    th = hw;
  }
  if (th < 1)
  {
    th = 1;
  }

#if defined(FPLLL_TESTING)
  threads_lock_waiters.fetch_add(1, std::memory_order_relaxed);
#endif
  std::unique_lock<std::mutex> lock(runtime().run_mutex);
#if defined(FPLLL_TESTING)
  threads_lock_waiters.fetch_sub(1, std::memory_order_relaxed);
#endif

  Runtime &rt = runtime();
  if (th == rt.threads.load(std::memory_order_relaxed))
  {
    return th;
  }

  /* Build the replacement while the old pool is still alive and published.  A
     failure here leaves both the old configuration and the old pool usable;
     only a successfully constructed candidate replaces them. */
  std::unique_ptr<Pool> candidate;
  if (th > 1)
  {
#if defined(FPLLL_TESTING)
    if (fail_next_pool_construction.exchange(false, std::memory_order_relaxed))
    {
      throw std::bad_alloc();
    }
    pool_constructions.fetch_add(1, std::memory_order_relaxed);
#endif
    candidate = std::make_unique<Pool>(static_cast<std::size_t>(th - 1));
  }

  rt.threads.store(th, std::memory_order_release);
  std::unique_ptr<Pool> retired = std::move(rt.pool);
  rt.pool                       = std::move(candidate);

  /* The retired pool is idle by construction: the run lock guarantees that no
     session or batch is using it.  Destroy it before releasing the lock, so
     its worker threads are gone before another thread can start a session.
     Configuration changes are rare, so the extra serialisation is cheap. */
  retired.reset();
  return th;
}

}  // namespace fplll
