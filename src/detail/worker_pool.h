// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef FPLLL_DETAIL_WORKER_POOL_H
#define FPLLL_DETAIL_WORKER_POOL_H

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace fplll::detail
{

/* Minimal fixed-size worker pool and the enumeration session built on top of
   it, both used by the parallel enumeration backend.
   The backend needs the constructor, submit_task() and the destructor only;
   everything else that a general-purpose pool provides (priority, paused
   state, task monitoring, ...) is deliberately absent.

   The workers are std::jthreads, so destroying the pool requests a stop and
   joins them.  A partially constructed pool (worker creation throws) also
   leaves no joinable threads behind: the constructor's failure path sets
   `closing_` and wakes the workers before the vector destroys and joins the
   already started jthreads.

   submit_task() returns a future that carries the task's exception, so the
   caller sees a failing task exactly as if it had invoked it directly.

   The runtime owns one pool behind a std::unique_ptr and replaces that pool
   only while the run lock guarantees that no task is executing, so tasks never
   outlive the pool by design.  Copying and moving are deleted because the
   workers refer to the pool through `this`. */
class worker_pool final
{
public:
  /* Starts `worker_count` workers (at least one).  The enumeration backend
     passes concurrency() - 1 because the thread that calls run_workers()
     executes the last slot itself; concurrency() 1 never builds a pool. */
  explicit worker_pool(std::size_t worker_count);
  ~worker_pool();

  worker_pool(const worker_pool &)            = delete;
  worker_pool &operator=(const worker_pool &) = delete;
  worker_pool(worker_pool &&)                 = delete;
  worker_pool &operator=(worker_pool &&)      = delete;

  /* Queues `task` and returns the future its outcome is reported through.
     Throws std::bad_alloc when the queue cannot grow and std::runtime_error
     when the pool is already shutting down. */
  [[nodiscard]] std::future<void> submit_task(std::function<void()> task);

private:
  void worker_loop();

  /* wakeup_ and queue_ are guarded by mutex_.  closing_ is only read and
     written while holding mutex_; the destructor sets it before stopping the
     workers, so submit_task() never enqueues work that shutdown skips. */
  std::mutex mutex_;
  std::condition_variable_any wakeup_;
  std::deque<std::function<void()>> queue_;
  std::vector<std::jthread> workers_;
  bool closing_ = false;
};

inline worker_pool::worker_pool(std::size_t worker_count)
{
  workers_.reserve(worker_count);
  try
  {
    for (std::size_t worker = 0; worker < worker_count; ++worker)
    {
      workers_.emplace_back([this] { worker_loop(); });
    }
  }
  catch (...)
  {
    /* ~worker_pool() does not run for a partially constructed object, so the
       started workers are told to return before the vector destroys (and the
       jthreads join) them.  Changing the predicate under the mutex makes the
       wake-up reliable: a worker either sees `closing_` before it sleeps or is
       already waiting on the condition variable. */
    {
      std::unique_lock<std::mutex> lock(mutex_);
      closing_ = true;
    }
    wakeup_.notify_all();
    throw;
  }
}

inline worker_pool::~worker_pool()
{
  {
    std::unique_lock<std::mutex> lock(mutex_);
    closing_ = true;
  }
  wakeup_.notify_all();
  /* Destroying a jthread requests a stop and joins it.  Tasks that were
     queued before shutdown still run, matching the "wait, then release"
     contract the backend relied on: the pool is only destroyed when its owner
     knows that no new work can arrive and every accepted task has already
     been joined. */
  workers_.clear();
}

inline std::future<void> worker_pool::submit_task(std::function<void()> task)
{
  /* A packaged_task stores the task's exception in the shared state behind the
     returned future.  packaged_task is move-only while std::function requires
     a copyable target, so the queue shares one packaged_task; a single small
     allocation per submitted batch is negligible next to the enumeration that
     follows. */
  auto packaged            = std::make_shared<std::packaged_task<void()>>(std::move(task));
  std::future<void> result = packaged->get_future();
  {
    std::unique_lock<std::mutex> lock(mutex_);
    if (closing_)
    {
      throw std::runtime_error("worker_pool::submit_task(): the pool is shutting down");
    }
    queue_.emplace_back([packaged] { (*packaged)(); });
  }
  wakeup_.notify_one();
  return result;
}

inline void worker_pool::worker_loop()
{
  for (;;)
  {
    std::function<void()> task;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      wakeup_.wait(lock, [this] { return closing_ || !queue_.empty(); });
      if (queue_.empty())
      {
        return;
      }
      task = std::move(queue_.front());
      queue_.pop_front();
    }
    try
    {
      task();
    }
    // The exception is deliberately swallowed and reported through the future:
    // see the comment below.  A worker thread must never terminate the process.
    catch (...)  // NOLINT(bugprone-empty-catch)  // reported through the future instead
    {
      /* packaged_task::operator() reports the task's exception through the
         future instead of throwing; only a failure to store it can escape
         here, and a worker thread must never terminate the process. */
    }
  }
}

/* One built-in enumeration runs inside one session.  A session owns the
   process-wide run lock and a fixed number of worker slots for the whole
   enumeration, so two enumerations never take turns inside the shared pool.
   Sessions are created and destroyed on the same thread; creating one while
   another session is alive waits for that session to finish.

   run_workers() must be called from the creating thread.  It runs work on
   slots 0..concurrency()-1, the calling thread always taking the last slot,
   and returns only after every started copy has stopped touching work.  A
   copy that throws does not cancel the others: they finish normally and the
   first exception is rethrown once every accepted copy has been joined. */
class EnumerationSession final
{
public:
  EnumerationSession();
  ~EnumerationSession();

  EnumerationSession(const EnumerationSession &)            = delete;
  EnumerationSession &operator=(const EnumerationSession &) = delete;
  EnumerationSession(EnumerationSession &&)                 = delete;
  EnumerationSession &operator=(EnumerationSession &&)      = delete;

  /* Number of threads this session may use, including the calling thread. */
  int concurrency() const noexcept;

  void run_workers(const std::function<void(int)> &work);

private:
  std::unique_lock<std::mutex> run_lock_;
  int concurrency_ = 1;
};

/* In-tree tests only; none of this is installed or part of the public API.
   The declarations (and their definitions) only exist in test builds, which
   set FPLLL_TESTING for the library and the test programs.

   set_submit_failure_for_testing(k): each following run_workers() call throws
   std::bad_alloc once it has submitted k tasks; a negative value disables the
   injection.
   fail_next_pool_construction_for_testing(): the next set_threads() call that
   would build a worker pool throws std::bad_alloc instead, leaving the old
   configuration and the old pool untouched.
   threads_lock_waiters_for_testing(): number of threads currently blocked
   waiting for the run lock.
   pool_constructions_for_testing(): number of worker pools constructed so far. */
#if defined(FPLLL_TESTING)
void set_submit_failure_for_testing(int accepted_tasks);
void fail_next_pool_construction_for_testing();
int threads_lock_waiters_for_testing();
int pool_constructions_for_testing();
#endif

}  // namespace fplll::detail

#endif  // FPLLL_DETAIL_WORKER_POOL_H
