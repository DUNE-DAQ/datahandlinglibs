/**
 * @file test_ratelimiter_app.cxx Test application for
 * ratelimiter implementation
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#

#include <thread>
#include <atomic>
#include <chrono>
#include <iostream>
#include <folly/coro/Baton.h>
#include <folly/coro/Task.h>
#include <folly/futures/Future.h>
#include <folly/coro/CurrentExecutor.h>
#include <folly/coro/Timeout.h>
#include <folly/futures/ThreadWheelTimekeeper.h>
#include <folly/coro/BlockingWait.h>


// using namespace dunedaq::datahandlinglibs;
using namespace std::chrono_literals;

folly::coro::Baton baton{0};
uint32_t max_wait = 500;

folly::coro::Task<void>
postprocess_schedule() {
  
  folly::ThreadWheelTimekeeper tk;

  const auto wait_data = [&baton]() -> folly::coro::Task<void> {
    // folly::coro::timeout cancels the task on timeout.
    // Baton is not cancellable, so we attach a callback to resume the coroutine.
    auto token = co_await folly::coro::co_current_cancellation_token;
    folly::CancellationCallback cb(token, [&baton] { baton.post(); });
    co_await baton; // Wait data
  };


  uint64_t n_timeouts = 0;
  uint64_t n_process = 0;

  while(true) {
    try {
      co_await folly::coro::timeout(
        wait_data(),
        std::chrono::milliseconds{ max_wait },
        &tk);
      ++n_process;
    } catch (const folly::FutureTimeout&) {
      // timeout = true;
      std::cout << "Timeout " << ++n_timeouts << std::endl;
    }
    baton.reset();
  }


  co_return;
}

int
main(int /*argc*/, char** /*argv[]*/)
{
  std::atomic<bool> run_marker;




  // A sleepy worker thread
  std::jthread sleepy_worker(
    [&baton](std::stop_token stoken)
    {

      while(!stoken.stop_requested()) {
        baton.post();
      }
        // for (int i = 10; i; --i)
        // {
        //     std::this_thread::sleep_for(300ms);
        //     if (stoken.stop_requested())
        //     {
        //         print("Sleepy worker is requested to stop\n");
        //         return;
        //     }
        //     print("Sleepy worker goes back to sleep\n");
        // }
    });

    // std::cout << "Sleeping for 3s" << std::endl;

    // std::this_thread::sleep_for(3s);

    std::cout << "Starting the coroutine" << std::endl;
    folly::coro::blockingWait(postprocess_schedule());

    std::cout << "Requesting stop" << std::endl;
    // sleepy_worker.request_stop();
    std::cout << "Thread stopped" << std::endl;


  return 0;
} // NOLINT(readability/fn_size)
