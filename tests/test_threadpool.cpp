/* Copyright (C) 2026 The fplll development team

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
#include <cstdio>
#include <stdexcept>

using namespace fplll;

int main()
{
  std::atomic<int> counter{0};

  for (int i = 0; i < 16; ++i)
    get_threadpool().push([&counter] { ++counter; });
  get_threadpool().wait_work();
  if (counter.load() != 16)
  {
    std::fprintf(stderr, "task counter mismatch: %d\n", counter.load());
    return 1;
  }

  // Exceptions thrown by jobs must be rethrown from wait_work, not terminate.
  bool caught = false;
  get_threadpool().push([] { throw std::runtime_error("boom"); });
  try
  {
    get_threadpool().wait_work();
  }
  catch (const std::runtime_error &)
  {
    caught = true;
  }
  if (!caught)
  {
    std::fprintf(stderr, "worker exception was not propagated\n");
    return 2;
  }

  // Resizing is only allowed while the pool is idle.
  set_threads(2);
  get_threadpool().push([&counter] { ++counter; });
  get_threadpool().wait_work();
  if (counter.load() != 17)
  {
    std::fprintf(stderr, "task counter mismatch after resize: %d\n", counter.load());
    return 3;
  }

  std::atomic<int> run_counter{0};
  get_threadpool().run([&run_counter](int) { ++run_counter; });
  if (run_counter.load() != 2)
  {
    std::fprintf(stderr, "run() did not execute on all threads: %d\n", run_counter.load());
    return 4;
  }

  set_threads(1);
  return 0;
}
