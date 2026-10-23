/* Copyright (C) 2019 Marc Stevens.

   This file is part of fplll. fplll is free software: you
   can redistribute it and/or modify it under the terms of the GNU Lesser
   General Public License as published by the Free Software Foundation,
   either version 2.1 of the License, or (at your option) any later version.

   fplll is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with fplll. If not, see <http://www.gnu.org/licenses/>. */

#include <fplll/threadpool.h>
#include <atomic>
#include <mutex>

FPLLL_BEGIN_NAMESPACE

namespace
{

thread_pool::thread_pool &pool_instance()
{
  static thread_pool::thread_pool pool;
  return pool;
}

/* Number of threads including the main thread; lock-free so that jobs may
   query it while the pool is active. */
std::atomic<int> thread_count{1};

std::mutex &resize_mutex()
{
  static std::mutex mutex;
  return mutex;
}

}  // namespace

thread_pool::thread_pool &get_threadpool() { return pool_instance(); }

/* get and set number of threads in threadpool, both return the (new) number of threads */
int get_threads() { return thread_count.load(std::memory_order_relaxed); }

int set_threads(int th)
{
  if (th > int(std::thread::hardware_concurrency()) || th == -1)
    th = int(std::thread::hardware_concurrency());
  if (th < 1)
    th = 1;

  std::lock_guard<std::mutex> lock(resize_mutex());
  get_threadpool().resize(static_cast<std::size_t>(th - 1));
  thread_count.store(th, std::memory_order_relaxed);
  return th;
}

FPLLL_END_NAMESPACE
