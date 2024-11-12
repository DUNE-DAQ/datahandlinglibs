/**
 * @file test_coroutines_app.cxx Test application for
 * experimenting with coroutines
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#include "logging/Logging.hpp"

#include <chrono>
#include <coroutine>
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>

struct TimerAwaiter {
  std::chrono::steady_clock::time_point wake_time;
  std::coroutine_handle<> handle;

  bool await_ready() const noexcept { return false; }
  void await_suspend(std::coroutine_handle<> h) { handle = h; }
  void await_resume() const noexcept {}

  bool operator>(const TimerAwaiter& other) const {
    return wake_time > other.wake_time;
  }
};

class TimerQueue {
public:
  void add_timer(TimerAwaiter awaiter) {
    std::unique_lock lock(mutex_);
    timers_.push(awaiter);
    std::cout << "Added timer to queue, waking up timer thread.\n";
    cv_.notify_one(); // Notify the timer thread of a new timer
  }

  void run() {
    while (running_) {
      std::unique_lock lock(mutex_);

      if (timers_.empty()) {
        std::cout << "Timer thread waiting for timers...\n";
        cv_.wait(lock); // Wait indefinitely if no timers are set
      } else {
        auto now = std::chrono::steady_clock::now();
        auto top = timers_.top();

        if (top.wake_time <= now) {
          // Resume coroutine and remove from queue
          std::cout << "Resuming coroutine, timer expired.\n";
          timers_.pop();
          lock.unlock(); // Unlock before resuming to avoid deadlocks
          top.handle.resume();
        } else {
          // Wait until the next timer is ready or a new timer is added
          std::cout << "Waiting until next timer, current time: " << now.time_since_epoch().count() << std::endl;
          cv_.wait_until(lock, top.wake_time);
        }
      }
    }
  }

  void stop() {
    running_ = false;
    cv_.notify_all(); // Stop waiting if the queue is empty
  }

private:
  std::priority_queue<TimerAwaiter, std::vector<TimerAwaiter>, std::greater<>> timers_;
  std::mutex mutex_;
  std::condition_variable cv_;
  std::atomic<bool> running_{true};
};

TimerQueue g_timer_queue;

struct DelayUntil {
  explicit DelayUntil(std::chrono::steady_clock::time_point t) : deadline(t) {}
  bool await_ready() const noexcept { return std::chrono::steady_clock::now() >= deadline; }
  void await_suspend(std::coroutine_handle<> h) const {
    std::cout << "Coroutine suspended, waiting until " << deadline.time_since_epoch().count() << "\n";
    g_timer_queue.add_timer({deadline, h});
  }
  void await_resume() const noexcept {}

private:
  std::chrono::steady_clock::time_point deadline;
};

// Coroutine task definition
struct task {
  struct promise_type {
    task get_return_object() { return {std::coroutine_handle<promise_type>::from_promise(*this)}; }
    std::suspend_never initial_suspend() { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }
    void return_void() {}
    void unhandled_exception() {}
  };

  std::coroutine_handle<promise_type> m_handle;
};

// Coroutine function to sum elements in a vector until the deadline
task async_sum_with_deadline(const std::vector<int>& data, std::atomic<long long>& result) {
  const std::chrono::milliseconds period(500); // Set the processing period to 50 microseconds
  size_t i = 0;

  while (i < data.size()) {
    // Calculate the end time for this period
    auto end_time = std::chrono::steady_clock::now() + period;

    // Process as many elements as possible until the deadline
    while (i < data.size() && std::chrono::steady_clock::now() < end_time) {
      result += data[i];
      ++i;

      // Introduce a small time delay to simulate real work (50us)
      std::this_thread::sleep_for(std::chrono::microseconds(10)); // Simulate some work inside each loop
    }

    std::cout << "Processed a chunk of the vector. Current index: " << i << "\n";

    if (i < data.size()) {
      co_await DelayUntil(end_time); // Suspend until the next period
    }
  }
}

int main() {
  // Timer thread to manage all delays
  std::thread timer_thread([] { g_timer_queue.run(); });

  // Initialize a much larger vector
  std::vector<int> data(100'000'000, 1); // 100 million elements
  std::atomic<long long> result{0}; // To store the final sum

  int runsecs = 10;
  std::atomic<bool> marker{true};

  // Killswitch to stop the application after a specified time
  auto killswitch = std::thread([&]() {
    std::cout << "Application will terminate in " << runsecs << "s...\n";
    std::this_thread::sleep_for(std::chrono::seconds(runsecs));
    marker.store(false);
  });

  // Start the coroutine task with a reduced deadline
  auto my_task = async_sum_with_deadline(data, result);
  std::cout << "Main continues while coroutine is suspended.\n";

  // Wait for the coroutine to complete
  while (marker.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Poll periodically
    std::cout << "Current sum: " << result.load() << "\n";
  }

  // Stop the timer thread and join the killswitch
  g_timer_queue.stop();
  if (timer_thread.joinable()) timer_thread.join();
  if (killswitch.joinable()) killswitch.join();

  std::cout << "Final sum: " << result.load() << "\n";
  std::cout << "Exiting.\n";

  return 0;
}

