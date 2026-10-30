// SPDX-FileCopyrightText: 2019 Marc Stevens
// SPDX-FileCopyrightText: 2026 Omvixiii
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef FPLLL_THREADPOOL_H
#define FPLLL_THREADPOOL_H

namespace fplll
{

/* Number of threads the parallel enumerator may use, including the calling
   thread.  The initial value is 1, so the enumerator runs single-threaded
   until set_threads() asks for more.  Reading the value never blocks. */
int get_threads();

/* Set the number of threads, including the calling thread, and return the new
   value.  -1 (the default) and values above the hardware concurrency select
   the number of hardware threads; values below 1 are raised to 1.  Worker
   threads are created lazily when the value is raised above 1 and released
   when it is lowered back to 1.

   Every built-in enumeration runs inside one exclusive session.  A second
   enumeration waits until the first one has finished, and a call that
   overlaps a running enumeration waits for the whole enumeration, not just
   the current batch.  The session serialises the global thread budget; the
   search state itself always belongs to one enumeration.  Calling this from
   an enumeration callback, or starting a nested enumeration there, throws
   std::logic_error instead of deadlocking.

   The worker pool is an internal implementation detail; this header and the
   two functions above are the whole supported interface. */
int set_threads(int th = -1);

}  // namespace fplll

#endif
