/**
 * @file datahandlinglibs_DataHandlingModel_test.cxx Unit Tests for DataHandlingModel
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#define BOOST_TEST_MODULE datahandlinglibs_DataHandlingModel_test // NOLINT

#include "boost/test/unit_test.hpp"
#include "datahandlinglibs/models/BinarySearchQueueModel.hpp"
#include "datahandlinglibs/models/DataHandlingModel.hpp"
#include "datahandlinglibs/models/FixedRateQueueModel.hpp"
#include "datahandlinglibs/models/IterableQueueModel.hpp"
#include "datahandlinglibs/models/SkipListLatencyBufferModel.hpp"
#include "datahandlinglibs/opmon/datahandling_info.pb.h"
#include "datahandlinglibs/testutils/UnitTestUtilities.hpp"

#include <folly/coro/BlockingWait.h>
#include <folly/coro/Timeout.h>
#include <folly/futures/ManualTimekeeper.h>

using namespace dunedaq::datahandlinglibs;

BOOST_AUTO_TEST_SUITE(datahandlinglibs_DataHandlingModel_test)

using RDT = unittest::MockReadoutType;
using RHT = unittest::MockRequestHandlerType<unittest::MockReadoutType,
                                             unittest::MockLatencyBufferType<unittest::MockReadoutType>>;
using LBT = unittest::MockLatencyBufferType<unittest::MockReadoutType>;
using RPT = unittest::MockRawDataProcessorType;



BOOST_AUTO_TEST_CASE(DataHandlingModel_process_item_pre_post_process)
{
  std::atomic<bool> run_marker = true;

  auto model = unittest::MockDataHandlingModel<RDT, RHT, LBT, RPT, RDT>(run_marker);

  bool pre_func_one_called = false;
  bool post_func_one_called = false;
  model.initialize(true);
  model.raw_processor(pre_func_one_called, post_func_one_called, 0);

  RDT elem;
  elem.set_timestamp(2);

  model.test_process_item(std::move(elem));
  BOOST_REQUIRE(pre_func_one_called);
  BOOST_REQUIRE_EQUAL(model.get_num_payloads(), 1);
  BOOST_REQUIRE_EQUAL(model.get_sum_payloads(), 1);
  BOOST_REQUIRE_EQUAL(model.get_stats_packet_count(), 1);
}

BOOST_AUTO_TEST_CASE(DataHandlingModel_process_item_cutoff_triggered)
{
  std::atomic<bool> run_marker = true;

  auto model = unittest::MockDataHandlingModel<RDT, RHT, LBT, RPT, RDT>(run_marker);

  bool pre_func_one_called = false;
  bool post_func_one_called = false;
  model.initialize(true);
  model.raw_processor(pre_func_one_called, post_func_one_called, 10);

  RDT elem;
  elem.set_timestamp(2);

  setenv("DUNEDAQ_ERS_WARNING", "throw", 1);
  BOOST_CHECK_THROW(model.test_process_item(std::move(elem)), std::exception);
}

BOOST_AUTO_TEST_CASE(DataHandlingModel_transform_payload)
{
  std::atomic<bool> run_marker = true;

  auto model = unittest::MockDataHandlingModel<RDT, RHT, LBT, RPT, unittest::MockSuperChunkReadoutType>(run_marker);

  unittest::MockSuperChunkReadoutType input;
  input.set_timestamp(4);
  auto output = model.test_transform_payload(input);
  BOOST_REQUIRE((std::is_same_v<unittest::MockReadoutType, decltype(output)::value_type>));
}

BOOST_AUTO_TEST_CASE(DataHandlingModel_transform_and_process_same_type)
{
  std::atomic<bool> run_marker = true;

  auto model = unittest::MockDataHandlingModel<RDT, RHT, LBT, RPT, RDT>(run_marker);
  model.initialize(true);

  bool pre_func_one_called = false;
  bool post_func_one_called = false;
  model.raw_processor(pre_func_one_called, post_func_one_called, 0);

  RDT input;
  input.set_timestamp(4);
  model.test_transform_and_process(std::move(input));
  BOOST_REQUIRE(pre_func_one_called);
  BOOST_REQUIRE_EQUAL(model.get_num_payloads(), 1);
}

BOOST_AUTO_TEST_CASE(DataHandlingModel_transform_and_process_different_type)
{
  std::atomic<bool> run_marker = true;

  auto model = unittest::MockDataHandlingModel<RDT, RHT, LBT, RPT, unittest::MockSuperChunkReadoutType>(run_marker);
  model.initialize(true);

  bool pre_func_one_called = false;
  bool post_func_one_called = false;
  model.raw_processor(pre_func_one_called, post_func_one_called, 0);

  unittest::MockSuperChunkReadoutType input;
  input.set_timestamp(4);
  model.test_transform_and_process(std::move(input));
  BOOST_REQUIRE(pre_func_one_called);
  BOOST_REQUIRE_EQUAL(model.get_num_payloads(), 1);
}

BOOST_AUTO_TEST_CASE(DataHandlingModel_consume_callback)
{
  std::atomic<bool> run_marker = true;

  auto model = unittest::MockDataHandlingModel<RDT, RHT, LBT, RPT, unittest::MockSuperChunkReadoutType>(run_marker);
  model.initialize(true);

  bool pre_func_one_called = false;
  bool post_func_one_called = false;
  model.raw_processor(pre_func_one_called, post_func_one_called, 0);

  unittest::MockSuperChunkReadoutType input;
  input.set_timestamp(4);
  model.test_consume_callback(std::move(input));
  BOOST_REQUIRE(pre_func_one_called);
  BOOST_REQUIRE_EQUAL(model.get_num_payloads(), 1);
}

BOOST_AUTO_TEST_CASE(DataHandlingModel_consume_callback_write_failure)
{
  std::atomic<bool> run_marker = true;

  auto model = unittest::MockDataHandlingModel<RDT,
                                   unittest::MockRequestHandlerType<RDT, BinarySearchQueueModel<RDT>>,
                                   BinarySearchQueueModel<RDT>,
                                   RPT,
                                   RDT>(run_marker);
  model.initialize(true); // default size of LB is 2

  bool pre_func_one_called = false;
  bool post_func_one_called = false;
  model.raw_processor(pre_func_one_called, post_func_one_called, 0);

  for (int i = 100; i < 102; i++) {
    RDT input;
    input.set_timestamp(i);
    model.test_consume_callback(std::move(input));
  }

  BOOST_REQUIRE(pre_func_one_called);
  BOOST_REQUIRE_EQUAL(model.get_num_lb_insert_failures(), 1);
}

BOOST_AUTO_TEST_CASE(DataHandlingModel_postprocess_schedule_SkipListLatencyBufferModel)
{
  std::atomic<bool> run_marker = true;

  auto model = unittest::MockDataHandlingModel<
    RDT,
    unittest::MockRequestHandlerType<unittest::MockReadoutType, SkipListLatencyBufferModel<unittest::MockReadoutType>>,
    SkipListLatencyBufferModel<unittest::MockReadoutType>,
    RPT,
    RDT>(run_marker);
  model.initialize(true);

  bool pre_func_one_called = false;
  bool post_func_one_called = false;
  model.post_schedule_init(pre_func_one_called, post_func_one_called, 0, 0);

  for (int i = 100; i < 105; i++) {
    RDT input;
    input.set_timestamp(i);
    model.test_transform_and_process(std::move(input));
  }
  // std::this_thread::sleep_for(std::chrono::milliseconds(50));
  std::thread postprocess([&]() { model.test_run_postprocess_scheduler(); });
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  for (int i = 105; i < 112; i++) {
    RDT input;
    input.set_timestamp(i);
    model.test_transform_and_process(std::move(input));
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  model.set_run_marker(false);
  {
    RDT input;
    input.set_timestamp(112);
    model.test_transform_and_process(std::move(input));
  }
  postprocess.join();

  BOOST_REQUIRE_EQUAL(model.get_num_post_processing_delay_max_waits(), 0);
  BOOST_REQUIRE_EQUAL(model.get_num_payloads(), 8); // newest ts - delay ticks
}

BOOST_AUTO_TEST_CASE(DataHandlingModel_postprocess_schedule_BinarySearchQueueModel)
{
  std::atomic<bool> run_marker = true;

  auto model = unittest::MockDataHandlingModel<RDT,
                                   unittest::MockRequestHandlerType<RDT, BinarySearchQueueModel<RDT>>,
                                   BinarySearchQueueModel<RDT>,
                                   RPT,
                                   RDT>(run_marker);
  model.initialize_iterable_queue(true, 32);

  bool pre_func_one_called = false;
  bool post_func_one_called = false;
  model.post_schedule_init(pre_func_one_called, post_func_one_called, 0, 0);

  for (int i = 100; i < 105; i++) {
    RDT input;
    input.set_timestamp(i);
    model.test_transform_and_process(std::move(input));
  }
  // std::this_thread::sleep_for(std::chrono::milliseconds(50));
  std::thread postprocess([&]() { model.test_run_postprocess_scheduler(); });
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  for (int i = 105; i < 112; i++) {
    RDT input;
    input.set_timestamp(i);
    model.test_transform_and_process(std::move(input));
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  model.set_run_marker(false);
  {
    RDT input;
    input.set_timestamp(112);
    model.test_transform_and_process(std::move(input));
  }
  postprocess.join();

  BOOST_REQUIRE_EQUAL(model.get_num_post_processing_delay_max_waits(), 0);
  BOOST_REQUIRE_EQUAL(model.get_num_payloads(), 8); // newest ts - delay ticks
}

BOOST_AUTO_TEST_CASE(DataHandlingModel_postprocess_schedule_FixedRateQueueModel)
{
  std::atomic<bool> run_marker = true;

  auto model = unittest::MockDataHandlingModel<RDT,
                                   unittest::MockRequestHandlerType<RDT, FixedRateQueueModel<RDT>>,
                                   FixedRateQueueModel<RDT>,
                                   RPT,
                                   RDT>(run_marker);
  model.initialize_iterable_queue(true, 32);

  bool pre_func_one_called = false;
  bool post_func_one_called = false;
  model.post_schedule_init(pre_func_one_called, post_func_one_called, 0, 0);

  for (int i = 100; i < 105; i++) {
    RDT input;
    input.set_timestamp(i);
    model.test_transform_and_process(std::move(input));
  }
  // std::this_thread::sleep_for(std::chrono::milliseconds(50));
  std::thread postprocess([&]() { model.test_run_postprocess_scheduler(); });
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  for (int i = 105; i < 112; i++) {
    RDT input;
    input.set_timestamp(i);
    model.test_transform_and_process(std::move(input));
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  model.set_run_marker(false);
  {
    RDT input;
    input.set_timestamp(112);
    model.test_transform_and_process(std::move(input));
  }
  postprocess.join();

  BOOST_REQUIRE_EQUAL(model.get_num_post_processing_delay_max_waits(), 0);
  BOOST_REQUIRE_EQUAL(model.get_num_payloads(), 8); // newest ts - delay ticks
}

BOOST_AUTO_TEST_CASE(datahandlinglibs_DataHandlingModel_run_postprocess_scheduler_timeout)
{
  std::atomic<bool> run_marker = true;

  auto model =
    unittest::MockDataHandlingModel<RDT,
                                    DefaultRequestHandlerModel<RDT, SkipListLatencyBufferModel<RDT>>,
                                    SkipListLatencyBufferModel<RDT>,
                                    TaskRawDataProcessorModel<RDT>>(run_marker);

  auto buffer = std::make_shared<SkipListLatencyBufferModel<RDT>>(); // Empty buffer

  constexpr bool post_processing_enabled = true;
  auto error_registry = std::make_unique<FrameErrorRegistry>();

  auto raw_processor =
    std::make_shared<TaskRawDataProcessorModel<RDT>>(error_registry, post_processing_enabled);

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
    unittest::MockDataHandlingModel<RDT,
                                    DefaultRequestHandlerModel<RDT, SkipListLatencyBufferModel<RDT>>,
                                    SkipListLatencyBufferModel<RDT>,
                                    TaskRawDataProcessorModel<RDT>>(run_marker);

  auto buffer = std::make_shared<SkipListLatencyBufferModel<RDT>>();

  for (int i = 1; i < 6; i++) {
    RDT frame{};
    frame.timestamp = i * 62500;
    buffer->write(std::move(frame));
  }

  constexpr bool post_processing_enabled = true;
  auto error_registry = std::make_unique<FrameErrorRegistry>();

  auto raw_processor =
    std::make_shared<TaskRawDataProcessorModel<RDT>>(error_registry, post_processing_enabled);

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

BOOST_AUTO_TEST_CASE(datahandlinglibs_DataHandlingModel_PostprocessScheduleAlgorithm_data_arrives_after_fully_processed_with_timeout)
{
  std::atomic<bool> run_marker = true;

  auto model =
    unittest::MockDataHandlingModel<RDT,
                                    DefaultRequestHandlerModel<RDT, SkipListLatencyBufferModel<RDT>>,
                                    SkipListLatencyBufferModel<RDT>,
                                    TaskRawDataProcessorModel<RDT>>(run_marker);

  auto buffer = std::make_shared<SkipListLatencyBufferModel<RDT>>();

  for (int i = 1; i < 3; i++) {
    RDT frame{};
    frame.timestamp = i * 62500;
    buffer->write(std::move(frame));
  }

  {
    RDT frame{};
    frame.timestamp = 4 * 62500;
    buffer->write(std::move(frame));  
  }

  constexpr bool post_processing_enabled = true;
  auto error_registry = std::make_unique<FrameErrorRegistry>();

  auto raw_processor =
    std::make_shared<TaskRawDataProcessorModel<RDT>>(error_registry, post_processing_enabled);

  constexpr uint64_t delay_ticks = 1 * 62500; // NOLINT(build/unsigned)
  constexpr uint64_t delay_min_wait = 1; // NOLINT(build/unsigned)
  constexpr uint64_t delay_max_wait = 2; // NOLINT(build/unsigned)

  typename decltype(model)::PostprocessScheduleAlgorithm sched_algo{
    *buffer, *raw_processor, delay_ticks, delay_min_wait, delay_max_wait
  };

  bool timeout = true;
  int processed_count = sched_algo.run(timeout);
  // Buffer = {1, 2, 4} delay_ticks = 1
  // 1st timeout => timeout_accumulated = 1 * 2 (delay_max_wait = 2)
  // end_win_ts = 4 - 1 + 2 => postprocess until 5 {1, 2, 4}
  BOOST_REQUIRE_EQUAL(processed_count, 3);

  {
    RDT frame{};
    frame.timestamp = 3 * 62500;
    buffer->write(std::move(frame));  
  }
  // Buffer = {1, 2, 3, 4}

  // To not trigger the "too fast" case
  std::this_thread::sleep_for(std::chrono::milliseconds(delay_min_wait + 1));

  timeout = false;
  // m_processed_up_to.timestamp = newest_ts + 1 => nothing to postprocess (data arrived too late)  
  processed_count += sched_algo.run(timeout);
  BOOST_REQUIRE_EQUAL(processed_count, 3);
}

BOOST_AUTO_TEST_SUITE_END()
