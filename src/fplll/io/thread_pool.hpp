/*********************************************************************************\
*                                                                                 *
* https://github.com/cr-marcstevens/snippets/tree/master/cxxheaderonly            *
*                                                                                 *
* thread_pool.hpp - A header only C++ light-weight thread pool                    *
* Copyright (c) 2017 Marc Stevens                                                 *
*                                                                                 *
* MIT License                                                                     *
*                                                                                 *
* Permission is hereby granted, free of charge, to any person obtaining a copy    *
* of this software and associated documentation files (the "Software"), to deal   *
* in the Software without restriction, including without limitation the rights    *
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell       *
* copies of the Software, and to permit persons to whom the Software is           *
* furnished to do so, subject to the following conditions:                        *
*                                                                                 *
* The above copyright notice and this permission notice shall be included in all  *
* copies or substantial portions of the Software.                                 *
*                                                                                 *
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR      *
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,        *
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE     *
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER          *
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,   *
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE   *
* SOFTWARE.                                                                       *
*                                                                                 *
\*********************************************************************************/

#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <vector>

/*************************** example usage ***************************************\
grep "^//test.cpp" thread_pool.hpp -A33 > test.cpp
g++ -std=c++11 -o test test.cpp -pthread -lpthread

//test.cpp:
#include "thread_pool.hpp"
#include <iostream>

int main()
{
        // use main thread also as worker using wait_work(), so init 1 less in thread pool
        // (alternatively use wait_sleep() and make threads for all logical hardware cores)
        thread_pool::thread_pool tp(std::thread::hardware_concurrency() - 1);

        std::atomic_uint aui(0);
        for (unsigned i = 0; i < 100; ++i)
                tp.push( [&aui](){ ++aui; } );
        std::cout << aui << std::endl;
        tp.wait_work();
        std::cout << aui << std::endl;

        // blocking run 'void f()' function on all threads and main thread
        tp.run( [&aui](){
                        std::cout << "Run 1: Thread started." << std::endl;
                });

        // blocking run 'void f(int)' function on all threads and main thread
        tp.run( [&aui](int threadid){
                        std::cout << "Run 2: Thread " << threadid << " started." << std::endl;
                });

        // blocking run 'void f(int,int)' function on all threads and main thread
        tp.run( [&aui](int threadid, int threads){
                        std::cout << "Run 3: Thread " << threadid << " of " << threads << "
started." << std::endl;
                });
        return 0;
}

\************************* end example usage *************************************/

namespace thread_pool
{

#if __cplusplus < 201703L
namespace detail
{
template <class> struct result_of;
template <class F, class... ArgTypes>
struct result_of<F(ArgTypes...)> : std::result_of<F(ArgTypes...)>
{
};
}  // namespace detail
#else
namespace detail
{
template <class> struct result_of;
template <class F, class... ArgTypes>
struct result_of<F(ArgTypes...)> : std::invoke_result<F, ArgTypes...>
{
};
}  // namespace detail
#endif

class thread_pool
{
public:
  thread_pool(std::size_t nrthreads = 0);
  ~thread_pool();

  std::size_t size() const;

  // resize the thread pool
  void resize(std::size_t nrthreads);

  // enqueue a function and obtain a future on its return value
  template <typename F, typename... Args>
  auto enqueue(F &&f, Args &&...args) -> std::future<typename detail::result_of<F(Args...)>::type>;

  // push a trivial function without a future
  void push(const std::function<void()> &f);
  void push(std::function<void()> &&f);

  // stop the thread pool
  void stop();

  // process single task
  bool work();

  // process tasks & then wait until all threads are idle
  void wait_work();

  // sleep until all threads are idle
  void wait_sleep();

  // run a job 'void f()' on #threads <= #threadpoolsize+1
  // (-1 => #threads = #threadpoolsize + 1)
  void run(const std::function<void()> &f, int threads = -1);
  // run a job given function 'void f(int threadid)'
  void run(const std::function<void(int)> &f, int threads = -1);
  // run a job given function 'void f(int threadid, int threads)' (0 <= threadid < threads)
  void run(const std::function<void(int, int)> &f, int threads = -1);

private:
  void _init_thread();

  std::size_t _outstanding = 0;
  std::exception_ptr _exception;
  std::vector<std::unique_ptr<std::thread>> _threads;
  std::vector<std::shared_ptr<std::atomic_bool>> _threads_stop;
  std::queue<std::function<void()>> _tasks;
  std::mutex _mutex;
  std::condition_variable _condition;
};

class barrier
{
public:
  barrier(std::size_t count);
  ~barrier() noexcept;

  void wait();

private:
  std::size_t _count;
  std::size_t _waiting    = 0;
  std::size_t _generation = 0;
  std::mutex _mutex;
  std::condition_variable _condition;
};

inline thread_pool::thread_pool(std::size_t nrthreads) { resize(nrthreads); }

inline thread_pool::~thread_pool() { stop(); }

inline std::size_t thread_pool::size() const { return _threads.size(); }

inline void thread_pool::stop() { resize(0); }

inline bool thread_pool::work()
{
  std::function<void()> task;
  {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_tasks.empty())
      return false;
    task = std::move(this->_tasks.front());
    this->_tasks.pop();
  }
  try
  {
    task();
  }
  catch (...)
  {
    std::lock_guard<std::mutex> lock(_mutex);
    if (!_exception)
      _exception = std::current_exception();
  }
  {
    std::lock_guard<std::mutex> lock(_mutex);
    --_outstanding;
  }
  _condition.notify_all();
  return true;
}

inline void thread_pool::wait_work()
{
  while (work())
    ;
  std::unique_lock<std::mutex> lock(_mutex);
  _condition.wait(lock, [this] { return _outstanding == 0; });
  if (_exception)
  {
    std::exception_ptr exception = _exception;
    _exception                   = nullptr;
    lock.unlock();
    std::rethrow_exception(exception);
  }
}

inline void thread_pool::wait_sleep()
{
  std::unique_lock<std::mutex> lock(_mutex);
  _condition.wait(lock, [this] { return _outstanding == 0; });
  if (_exception)
  {
    std::exception_ptr exception = _exception;
    _exception                   = nullptr;
    lock.unlock();
    std::rethrow_exception(exception);
  }
}

inline void thread_pool::push(const std::function<void()> &f)
{
  {
    std::unique_lock<std::mutex> lock(_mutex);
    _tasks.emplace(f);
    ++_outstanding;
  }
  _condition.notify_one();
}

inline void thread_pool::push(std::function<void()> &&f)
{
  {
    std::unique_lock<std::mutex> lock(_mutex);
    _tasks.emplace(std::move(f));
    ++_outstanding;
  }
  _condition.notify_one();
}

template <typename F, typename... Args>
inline auto thread_pool::enqueue(F &&f, Args &&...args)
    -> std::future<typename detail::result_of<F(Args...)>::type>
{
  typedef typename detail::result_of<F(Args...)>::type return_type;
  auto task = std::make_shared<std::packaged_task<return_type()>>(
      std::bind(std::forward<F>(f), std::forward<Args>(args)...));
  push([task]() { (*task)(); });
  return task->get_future();
}

inline void thread_pool::run(const std::function<void()> &f, int threads)
{
  if (threads < 1 || threads > int(_threads.size()) + 1)
    threads = int(_threads.size()) + 1;
  for (int i = 0; i < threads - 1; ++i)
    push(f);
  try
  {
    f();
  }
  catch (...)
  {
    std::lock_guard<std::mutex> lock(_mutex);
    if (!_exception)
      _exception = std::current_exception();
  }
  this->wait_sleep();
}

inline void thread_pool::run(const std::function<void(int)> &f, int threads)
{
  if (threads < 1 || threads > int(_threads.size()) + 1)
    threads = int(_threads.size()) + 1;
  for (int i = 0; i < threads - 1; ++i)
    push([f, i] { f(i); });
  try
  {
    f(threads - 1);
  }
  catch (...)
  {
    std::lock_guard<std::mutex> lock(_mutex);
    if (!_exception)
      _exception = std::current_exception();
  }
  this->wait_sleep();
}

inline void thread_pool::run(const std::function<void(int, int)> &f, int threads)
{
  if (threads < 1 || threads > int(_threads.size()) + 1)
    threads = int(_threads.size()) + 1;
  for (int i = 0; i < threads - 1; ++i)
    push([f, i, threads] { f(i, threads); });
  try
  {
    f(threads - 1, threads);
  }
  catch (...)
  {
    std::lock_guard<std::mutex> lock(_mutex);
    if (!_exception)
      _exception = std::current_exception();
  }
  this->wait_sleep();
}

inline void thread_pool::resize(std::size_t nrthreads)
{
  if (nrthreads == _threads.size())
    return;

  // The pool must be idle before the worker vectors are changed.
  wait_work();

  if (nrthreads < _threads.size())
  {
    // decreasing number of active threads
    std::unique_lock<std::mutex> lock(_mutex);
    for (std::size_t i = nrthreads; i < _threads.size(); ++i)
      *(_threads_stop[i]) = true;
    _condition.notify_all();
    lock.unlock();
    for (std::size_t i = nrthreads; i < _threads.size(); ++i)
      _threads[i]->join();

    lock.lock();
    _threads_stop.resize(nrthreads);
    _threads.resize(nrthreads);
  }
  else
  {
    std::unique_lock<std::mutex> lock(_mutex);
    _threads_stop.reserve(nrthreads);
    _threads.reserve(nrthreads);
    for (std::size_t i = _threads.size(); i < nrthreads; ++i)
    {
      _threads_stop.emplace_back(new std::atomic_bool(false));
      _init_thread();
    }
  }
}

inline void thread_pool::_init_thread()
{
  std::size_t i = _threads.size();
  if (i >= _threads_stop.size())
    throw std::runtime_error("thread_pool::_init_thread(): index out of range!");
  auto f = [this, i]()
  {
    std::function<void()> task;
    std::unique_lock<std::mutex> lock(this->_mutex);
    while (true)
    {
      if (this->_tasks.empty())
      {
        if (*(this->_threads_stop[i]))
          return;
        this->_condition.wait(lock);
        continue;
      }

      task = std::move(this->_tasks.front());
      this->_tasks.pop();

      lock.unlock();
      try
      {
        task();
      }
      catch (...)
      {
        std::lock_guard<std::mutex> guard(this->_mutex);
        if (!this->_exception)
          this->_exception = std::current_exception();
      }
      lock.lock();
      --this->_outstanding;
      this->_condition.notify_all();
    }
  };
  _threads.emplace_back(new std::thread(f));
}

inline barrier::barrier(std::size_t count) : _count(count) {}

inline barrier::~barrier() noexcept { assert(_waiting == 0); }

inline void barrier::wait()
{
  std::unique_lock<std::mutex> lock(_mutex);
  const std::size_t generation = _generation;
  if (++_waiting >= _count)
  {
    _waiting = 0;
    ++_generation;
    lock.unlock();
    _condition.notify_all();
  }
  else
  {
    _condition.wait(lock, [this, generation] { return _generation != generation; });
  }
}

}  // namespace thread_pool

#endif  // THREAD_POOL_HPP
