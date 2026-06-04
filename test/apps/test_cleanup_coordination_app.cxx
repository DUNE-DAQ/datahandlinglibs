/**
 * @file test_cleanup_coordination_app.cxx Test application for
 * coordination between cleanup, issue request, and recording
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
 
#include "datahandlinglibs/models/DefaultRequestHandlerModel.hpp"
#include "datahandlinglibs/models/DefaultSkipListRequestHandler.hpp"

#include "datahandlinglibs/models/SkipListLatencyBufferModel.hpp"
#include "datahandlinglibs/models/FixedRateQueueModel.hpp"
#include "datahandlinglibs/models/BinarySearchQueueModel.hpp"

#include "datahandlinglibs/ReadoutTypes.hpp"
#include "datahandlinglibs/utils/RateLimiter.hpp"
#include "datahandlinglibs/testutils/UnitTestUtilities.hpp"

#include <chrono>
#include <memory>
#include <thread>
#include <string>
#include <utility>

using namespace dunedaq::datahandlinglibs;

using ReadoutType = types::DUMMY_FRAME_STRUCT;

// This test exercises the synchronization pattern used by request handling
// and recording while cleanup is running, but does not perform real recording
// or fragment sending.
template<class LatencyBufferType, class RequestHandlerType>
void
run_cleanup_coordination_test(const std::string& buffer_name)
{
  constexpr int run_duration = 15;
  constexpr auto producer_rate_khz = 100.0;
  constexpr uint64_t ts_step = 100'000'000; // NOLINT(build/unsigned)
  constexpr size_t queue_size = 1000;
  
  TLOG() << "Starting cleanup coordination test for " << buffer_name;
  
  auto error_registry = std::make_unique<FrameErrorRegistry>();
  
  auto buffer = std::make_shared<LatencyBufferType>();
  buffer->allocate_memory(queue_size); // Queue-based buffers need explicit allocation; this is a no-op for skip list
  
  unittest::MockRequestHandlerType<ReadoutType, RequestHandlerType> request_handler(buffer, error_registry);
  request_handler.set_pop_limit_size(100);
  request_handler.set_pop_size_pct(0.5);
  request_handler.set_run_marker(true);
  
  std::jthread producer([&](std::stop_token stop_token) {
    RateLimiter rate_limiter(producer_rate_khz);

    uint64_t ts = 0; // NOLINT(build/unsigned)

    while (!stop_token.stop_requested()) {
      ReadoutType frame{};
      frame.timestamp = ts;
      buffer->write(std::move(frame));

      ts += ts_step;
      rate_limiter.limit();
    }
  });

  std::jthread cleaner([&]() { request_handler.test_periodic_cleanups(); });

  std::jthread recorder([&](std::stop_token stop_token) {
    while (!stop_token.stop_requested()) {
      request_handler.test_simulate_recording();
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  });

  std::jthread issuer([&](std::stop_token stop_token) {
    while (!stop_token.stop_requested()) {
      request_handler.test_simulate_issuing();
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  });

  std::jthread monitor([&](std::stop_token stop_token) {
    while (!stop_token.stop_requested()) {
      TLOG() << "[" << buffer_name << "]"
             << " occupancy=" << buffer->occupancy()
             << ", cleanups=" << request_handler.get_num_buffer_cleanups()
             << ", pops=" << request_handler.get_pops_count()
             << ", issuing_calls=" << request_handler.get_issuing_calls()
             << ", recording_calls=" << request_handler.get_recording_calls();
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  });

  std::this_thread::sleep_for(std::chrono::seconds(run_duration));

  producer.request_stop();
  recorder.request_stop();
  issuer.request_stop();
  monitor.request_stop();
  request_handler.set_run_marker(false);

  TLOG() << "[" << buffer_name << "] Final statistics:"
         << " occupancy=" << buffer->occupancy()
         << ", cleanups=" << request_handler.get_num_buffer_cleanups()
         << ", pops=" << request_handler.get_pops_count()
         << ", issuing_calls=" << request_handler.get_issuing_calls()
         << ", recording_calls=" << request_handler.get_recording_calls();
}

int
main(int /*argc*/, char** /*argv[]*/)
{
  run_cleanup_coordination_test<
    SkipListLatencyBufferModel<ReadoutType>,
    DefaultSkipListRequestHandler<ReadoutType>>("SkipListLatencyBufferModel");

  run_cleanup_coordination_test<
    FixedRateQueueModel<ReadoutType>,
    DefaultRequestHandlerModel<ReadoutType, FixedRateQueueModel<ReadoutType>>>("FixedRateQueueModel");

  run_cleanup_coordination_test<
    BinarySearchQueueModel<ReadoutType>,
    DefaultRequestHandlerModel<ReadoutType, BinarySearchQueueModel<ReadoutType>>>("BinarySearchQueueModel");

  return 0;
}
