/**
 * @file datahandlinglibs_DataHandlingModel_test.cxx Unit Tests for DataHandlingModel
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#define BOOST_TEST_MODULE datahandlinglibs_DataHandlingModel_test // NOLINT

#include "boost/test/unit_test.hpp"

#include "datahandlinglibs/ReadoutTypes.hpp"
#include "datahandlinglibs/models/SkipListLatencyBufferModel.hpp"
#include "datahandlinglibs/testutils/UnitTestUtilities.hpp"

#include <folly/futures/ManualTimekeeper.h>

#include <memory>
#include <utility>

BOOST_AUTO_TEST_SUITE(datahandlinglibs_DataHandlingModel_test)

using namespace dunedaq::datahandlinglibs;

using ReadoutType = types::DUMMY_FRAME_STRUCT;

BOOST_AUTO_TEST_CASE(datahandlinglibs_DataHandlingModel_run_postprocess_scheduler_timeout)
{
  std::atomic<bool> run_marker = true;

  auto model =
    unittest::MockDataHandlingModel<ReadoutType,
                                    DefaultRequestHandlerModel<ReadoutType, SkipListLatencyBufferModel<ReadoutType>>,
                                    SkipListLatencyBufferModel<ReadoutType>,
                                    TaskRawDataProcessorModel<ReadoutType>>(run_marker);

  auto buffer = std::make_shared<SkipListLatencyBufferModel<ReadoutType>>(); // Empty buffer

  constexpr bool post_processing_enabled = true;
  auto error_registry = std::make_unique<FrameErrorRegistry>();

  auto raw_processor =
    std::make_shared<TaskRawDataProcessorModel<ReadoutType>>(error_registry, post_processing_enabled);

  auto timekeeper = std::make_unique<folly::ManualTimekeeper>();
  auto* timekeeper_ptr = timekeeper.get();

  constexpr uint64_t delay_max_wait = 2; // NOLINT(build/unsigned)

  std::thread coro_thread([&]() {
    model.test_run_postprocess_scheduler(buffer, raw_processor, std::move(timekeeper), delay_max_wait);
  });

  // Wait for coroutine to start then timeout to get registered
  while (timekeeper_ptr->numScheduled() == 0) {
    std::this_thread::sleep_for(1ms);
  }
  // Safe-guard for the delay between timeout registration and coroutine suspension  
  // If the test is failing, consider a longer sleep or a better way to synchronize
  std::this_thread::sleep_for(1ms);
  timekeeper_ptr->advance(std::chrono::milliseconds{ delay_max_wait }); // Trigger a timeout

  model.set_run_marker(false); // Let coroutine end
  coro_thread.join(); // The test will stuck here if timeout is not triggered (because of folly::coro::blockingWait)

  BOOST_REQUIRE_EQUAL(model.get_num_post_processing_delay_max_waits(), 1);
}

BOOST_AUTO_TEST_CASE(datahandlinglibs_DataHandlingModel_PostprocessScheduleAlgorithm_timeout)
{
  std::atomic<bool> run_marker = true;

  auto model =
    unittest::MockDataHandlingModel<ReadoutType,
                                    DefaultRequestHandlerModel<ReadoutType, SkipListLatencyBufferModel<ReadoutType>>,
                                    SkipListLatencyBufferModel<ReadoutType>,
                                    TaskRawDataProcessorModel<ReadoutType>>(run_marker);

  auto buffer = std::make_shared<SkipListLatencyBufferModel<ReadoutType>>();

  for (int i = 1; i < 6; i++) {
    ReadoutType frame{};
    frame.timestamp = i * 62500;
    buffer->write(std::move(frame));
  }

  constexpr bool post_processing_enabled = true;
  auto error_registry = std::make_unique<FrameErrorRegistry>();

  auto raw_processor =
    std::make_shared<TaskRawDataProcessorModel<ReadoutType>>(error_registry, post_processing_enabled);

  constexpr uint64_t delay_ticks = 4 * 62500; // NOLINT(build/unsigned)
  constexpr uint64_t delay_min_wait = 1; // NOLINT(build/unsigned)
  constexpr uint64_t delay_max_wait = 2; // NOLINT(build/unsigned)

  typename decltype(model)::PostprocessScheduleAlgorithm sched_algo{
    *buffer, *raw_processor, delay_ticks, delay_min_wait, delay_max_wait
  };

  // First pass
  bool timeout = false;
  int processed_count = sched_algo.run(timeout);
  // Buffer = {1, 2, 3, 4, 5} delay_ticks = 4
  // 5 - 1 > 4 is false => no postprocessing
  BOOST_REQUIRE_EQUAL(processed_count, 0);

  timeout = true;
  // 1st timeout => timeout_accumulated = 1 * 2 (delay_max_wait = 2)
  // end_win_ts = 5 - 4 + 2 => postprocess until 3 {1, 2}
  processed_count += sched_algo.run(timeout);
  BOOST_REQUIRE_EQUAL(processed_count, 2);

  // 2nd timeout => timeout_accumulated = 2 * 2
  // end_win_ts = 5 - 4 + 4 => postprocess until 5 {3, 4}
  processed_count += sched_algo.run(timeout);
  BOOST_REQUIRE_EQUAL(processed_count, 4);

  // 3rd timeout => timeout_accumulated = 3 * 2
  // end_win_ts = 5 - 4 + 6 => postprocess until 6 (capped to newest_ts + 1) {5}
  processed_count += sched_algo.run(timeout);
  BOOST_REQUIRE_EQUAL(processed_count, 5);

  // 4th timeout
  // m_processed_up_to.timestamp = newest_ts + 1 => nothing to postprocess (at cap)
  processed_count += sched_algo.run(timeout);
  BOOST_REQUIRE_EQUAL(processed_count, 5);
}

BOOST_AUTO_TEST_SUITE_END()
