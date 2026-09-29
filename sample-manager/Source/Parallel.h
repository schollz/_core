#pragma once
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <exception>
#include <functional>
#include <future>
#include <mutex>
#include <thread>
#include <vector>
namespace core {
// Bounded read/compute work. Each job owns a distinct output element; progress
// and exception propagation stay on the calling project worker, never on the
// UI.
inline void parallelFor(size_t count, const std::function<void(size_t)> &work,
                        const std::function<void(size_t)> &progress = {}) {
  if (count == 0)
    return;
  const size_t workers = std::min(
      count,
      size_t(std::min(4u, std::max(1u, std::thread::hardware_concurrency()))));
  std::atomic<size_t> next{0};
  std::atomic<bool> failed{false};
  std::mutex mutex;
  std::condition_variable changed;
  size_t completed = 0, running = workers;
  std::exception_ptr error;
  std::vector<std::future<void>> jobs;
  for (size_t worker = 0; worker < workers; ++worker)
    jobs.push_back(std::async(std::launch::async, [&] {
      try {
        while (!failed) {
          const auto index = next.fetch_add(1);
          if (index >= count)
            break;
          work(index);
          {
            std::lock_guard<std::mutex> lock(mutex);
            ++completed;
          }
          changed.notify_one();
        }
      } catch (...) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!error)
          error = std::current_exception();
        failed = true;
      }
      {
        std::lock_guard<std::mutex> lock(mutex);
        --running;
      }
      changed.notify_one();
    }));
  size_t reported = 0;
  std::unique_lock<std::mutex> lock(mutex);
  while (running != 0) {
    changed.wait(lock, [&] { return completed != reported || running == 0; });
    reported = completed;
    lock.unlock();
    if (progress)
      progress(reported);
    lock.lock();
  }
  lock.unlock();
  for (auto &job : jobs)
    job.get();
  if (error)
    std::rethrow_exception(error);
  if (progress && reported != count)
    progress(count);
}
} // namespace core
