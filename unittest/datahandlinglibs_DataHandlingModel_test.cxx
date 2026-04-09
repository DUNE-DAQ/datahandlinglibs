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

#include <chrono>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

using namespace dunedaq::datahandlinglibs;

using ReadoutType = types::DUMMY_FRAME_STRUCT;
using SkipListLatencyBuffer = SkipListLatencyBufferModel<ReadoutType>;
using DefaultRequestHandler = DefaultRequestHandlerModel<ReadoutType, SkipListLatencyBuffer>;

/*
 * Test Fixtures:
 * - DataHandlingFixture: Full model testing
 * - PostprocessScheduleAlgorithmFixture: Algorithm-only testing (call setup() before run())
 * 
 * Both fixtures start with empty buffers. Use setup()/add_frames()/add_frame() to populate.
 */

template<class RequestHandlerType, class LatencyBufferType>
struct DataHandlingBaseFixture
{
  using RawDataProcessorType = TaskRawDataProcessorModel<ReadoutType>;
  using ModelType =
    unittest::MockDataHandlingModel<ReadoutType, RequestHandlerType, LatencyBufferType, RawDataProcessorType>;
  
  using timestamp_t = typename ModelType::timestamp_t;

  std::unique_ptr<ModelType> model;
  std::shared_ptr<LatencyBufferType> buffer;
  std::unique_ptr<FrameErrorRegistry> error_registry;
  std::shared_ptr<RawDataProcessorType> processor;
  std::atomic<bool> run_marker = true;

protected:
  DataHandlingBaseFixture()
  {
    model = std::make_unique<ModelType>(run_marker);
    buffer = std::make_shared<LatencyBufferType>();
  }
  
  void set_processor(bool post_processing_enabled) {
    error_registry = std::make_unique<FrameErrorRegistry>();
    processor = std::make_shared<RawDataProcessorType>(error_registry, post_processing_enabled);
  }
};

template<class RequestHandlerType, class LatencyBufferType>
struct DataHandlingFixture : DataHandlingBaseFixture<RequestHandlerType, LatencyBufferType>
{
  using Base = DataHandlingBaseFixture<RequestHandlerType, LatencyBufferType>;
  using ModelType = typename Base::ModelType;

  void test_run_postprocess_scheduler(std::unique_ptr<folly::Timekeeper> timekeeper,
                                      uint64_t post_processing_delay_max_wait) // NOLINT(build/unsigned)
  {
    constexpr bool post_processing_enabled = true;
    this->set_processor(post_processing_enabled);

    this->model->test_run_postprocess_scheduler(
      this->buffer, this->processor, std::move(timekeeper), post_processing_delay_max_wait);
  }

  void set_run_marker(bool run_marker) {
    this->model->set_run_marker(run_marker);
  }

  ModelType::num_postprocess_schedule_timeouts_t get_num_postprocess_schedule_timeouts()
  {
    return this->model->get_num_postprocess_schedule_timeouts();
  }
};

struct PostprocessScheduleAlgorithmFixture : DataHandlingBaseFixture<DefaultRequestHandler, SkipListLatencyBuffer>
{
  using LatencyBufferType = SkipListLatencyBuffer;
  using RequestHandlerType = DefaultRequestHandlerModel<ReadoutType, LatencyBufferType>;
  using Base = DataHandlingBaseFixture<RequestHandlerType, LatencyBufferType>;
  using AlgorithmType = typename Base::ModelType::PostprocessScheduleAlgorithm;

  static constexpr bool post_processing_enabled = true;
  static constexpr uint64_t ms_to_ticks = 62500; // NOLINT(build/unsigned)

  std::unique_ptr<AlgorithmType> algorithm;
  bool convert_to_ticks; // for timestamps and delay_ticks
  uint64_t delay_ticks; // NOLINT(build/unsigned)

  void setup(uint64_t delay_ticks,                         // NOLINT(build/unsigned)
             uint64_t delay_min_wait,                      // NOLINT(build/unsigned)
             uint64_t delay_max_wait,                      // NOLINT(build/unsigned)
             const std::vector<timestamp_t>& timestamps = {},
             bool convert_to_ticks = false) 
  {
    this->convert_to_ticks = convert_to_ticks;
    if (this->convert_to_ticks) {
      this->delay_ticks = delay_ticks * ms_to_ticks;
    }    

    this->buffer->flush();
    add_frames(timestamps);

    this->set_processor(post_processing_enabled);

    algorithm = std::make_unique<AlgorithmType>(
      *(this->buffer), *(this->processor), this->delay_ticks, delay_min_wait, delay_max_wait, this->model->get_postprocess_state());
  }

  void add_frame(timestamp_t timestamp)
  {
    if (convert_to_ticks) {
      timestamp = timestamp * ms_to_ticks;
    }
    
    this->model->test_update_postprocess_monitoring(this->buffer, delay_ticks, timestamp);
    
    ReadoutType frame{};
    frame.timestamp = timestamp;
    this->buffer->write(std::move(frame));
  }

  void add_frames(const std::vector<timestamp_t>& timestamps) 
  {
    for (timestamp_t ts : timestamps) {
      add_frame(ts);
    }
  }

  int run(bool timeout)
  {
    if (!algorithm) {
      return 0;
    }
    return algorithm->run(timeout);
  }

  ModelType::num_postprocess_late_arrivals_t get_num_postprocess_late_arrivals()
  {
    return this->model->get_num_postprocess_late_arrivals();
  }  

  ModelType::max_postprocess_tick_diff_to_next_window_start_t get_max_postprocess_tick_diff_to_next_window_start()
  {
    return this->model->get_max_postprocess_tick_diff_to_next_window_start();
  }  

  ModelType::max_postprocess_tick_diff_to_newest_t get_max_postprocess_tick_diff_to_newest()
  {
    return this->model->get_max_postprocess_tick_diff_to_newest();
  }  
  
  ModelType::max_postprocess_tick_diff_to_last_processed_t get_max_postprocess_tick_diff_to_last_processed()
  {
    return this->model->get_max_postprocess_tick_diff_to_last_processed();
  }  
};

using DefaultDataHandlingFixture = DataHandlingFixture<DefaultRequestHandler, SkipListLatencyBuffer>;

BOOST_FIXTURE_TEST_SUITE(datahandlinglibs_DataHandlingModel_test, DefaultDataHandlingFixture)

BOOST_AUTO_TEST_CASE(datahandlinglibs_DataHandlingModel_run_postprocess_scheduler_timeout)
{
  auto timekeeper = std::make_unique<folly::ManualTimekeeper>();
  auto* timekeeper_ptr = timekeeper.get();

  constexpr uint64_t delay_max_wait = 2; // NOLINT(build/unsigned)

  std::thread coro_thread(
    [&]() { test_run_postprocess_scheduler(std::move(timekeeper), delay_max_wait); });

  // Wait for coroutine to start then timeout to get registered
  while (timekeeper_ptr->numScheduled() == 0) {
    std::this_thread::sleep_for(1ms);
  }
  // Safe-guard for the delay between timeout registration and coroutine suspension
  // If the test is failing, consider a longer sleep or a better way to synchronize
  std::this_thread::sleep_for(1ms);
  timekeeper_ptr->advance(std::chrono::milliseconds(delay_max_wait)); // Trigger a timeout

  set_run_marker(false); // Let coroutine end
  coro_thread.join();    // The test will stuck here if timeout is not triggered (because of folly::coro::blockingWait)

  BOOST_REQUIRE_EQUAL(get_num_postprocess_schedule_timeouts(), 1);
}

BOOST_AUTO_TEST_SUITE_END() // DataHandlingFixture

BOOST_FIXTURE_TEST_SUITE(datahandlinglibs_DataHandlingModelPostprocessScheduleAlgorithm_test,
                         PostprocessScheduleAlgorithmFixture)

BOOST_AUTO_TEST_CASE(datahandlinglibs_DataHandlingModel_PostprocessScheduleAlgorithm_data_arrives_after_window_processed_with_timeout_monitoring)
{
  std::vector<timestamp_t> timestamps = { 1, 3, 4 };
  constexpr uint64_t delay_ticks = 4;    // NOLINT(build/unsigned)
  constexpr uint64_t delay_min_wait = 1; // NOLINT(build/unsigned)
  constexpr uint64_t delay_max_wait = 2; // NOLINT(build/unsigned)
  bool convert_to_ticks = true;

  setup(delay_ticks, delay_min_wait, delay_max_wait, timestamps, convert_to_ticks);

  // First pass
  bool timeout = false;
  int processed_count = run(timeout);
  BOOST_REQUIRE_EQUAL(processed_count, 0);

  timeout = true;
  // Buffer = {1, 3, 4} delay_ticks = 4
  // 1st timeout => timeout_accumulated = 1 * 2 (delay_max_wait = 2)
  // 4 - 3 + 2 <= 4 => postprocess until 3 {1}
  processed_count += run(timeout);
  BOOST_REQUIRE_EQUAL(processed_count, 1);

  // Buffer = {1, 3, 4} delay_ticks = 4
  // 2nd timeout => timeout_accumulated = 2 * 2
  // 4 - 4 + 4 <= 4 => postprocess until 3 {3} next_window_start_ts = 4
  processed_count += run(timeout);
  BOOST_REQUIRE_EQUAL(processed_count, 2);

  timestamp_t last_processed_ts = 3;
  timestamp_t newest_ts = 4;
  timestamp_t next_window_start_ts = 4;

  // Data arrived for an already closed postprocessing window
  timestamp_t current_ts = 2;
  add_frame(current_ts);

  BOOST_REQUIRE_EQUAL(get_num_postprocess_late_arrivals(), 1);
  BOOST_REQUIRE_EQUAL(get_max_postprocess_tick_diff_to_next_window_start(), (next_window_start_ts - current_ts) * ms_to_ticks);  
  BOOST_REQUIRE_EQUAL(get_max_postprocess_tick_diff_to_newest(), (newest_ts - current_ts) * ms_to_ticks);  
  BOOST_REQUIRE_EQUAL(get_max_postprocess_tick_diff_to_last_processed(), (last_processed_ts - current_ts) * ms_to_ticks);  
}

BOOST_AUTO_TEST_CASE(datahandlinglibs_DataHandlingModelPostprocessScheduleAlgorithm_timeout)
{
  std::vector<timestamp_t> timestamps = { 1, 2, 3, 4, 5 };
  constexpr uint64_t delay_ticks = 4;    // NOLINT(build/unsigned)
  constexpr uint64_t delay_min_wait = 1; // NOLINT(build/unsigned)
  constexpr uint64_t delay_max_wait = 2; // NOLINT(build/unsigned)
  bool convert_to_ticks = true;

  setup(delay_ticks, delay_min_wait, delay_max_wait, timestamps, convert_to_ticks);

  // First pass
  bool timeout = false;
  int processed_count = run(timeout);
  BOOST_REQUIRE_EQUAL(processed_count, 0);

  timeout = true;
  // Buffer = {1, 2, 3, 4, 5} delay_ticks = 4
  // 1st timeout => timeout_accumulated = 1 * 2 (delay_max_wait = 2)
  // 5 - 3 + 2 <= 4 => postprocess until 3 {1, 2}
  processed_count += run(timeout);
  BOOST_REQUIRE_EQUAL(processed_count, 2);

  // 2nd timeout => timeout_accumulated = 2 * 2
  // 5 - 5 + 4 <= 4 => postprocess until 5 {3, 4}
  processed_count += run(timeout);
  BOOST_REQUIRE_EQUAL(processed_count, 4);

  // 3rd timeout => timeout_accumulated = 3 * 2
  // 5 - 5 + 6 > 4 => postprocess until end {5} next_window_start_ts = 5 + 1 = 6
  processed_count += run(timeout);
  BOOST_REQUIRE_EQUAL(processed_count, 5);

  // 4th timeout
  // next_window_start_ts > newest_ts (6 > 5 postprocessing window is already closed)
  processed_count += run(timeout);
  BOOST_REQUIRE_EQUAL(processed_count, 5);
}

BOOST_AUTO_TEST_CASE(
  datahandlinglibs_DataHandlingModelPostprocessScheduleAlgorithm_data_arrives_after_fully_processed_with_timeout)
{
  std::vector<timestamp_t> timestamps = { 1, 2, 4 };
  constexpr uint64_t delay_ticks = 1;    // NOLINT(build/unsigned)
  constexpr uint64_t delay_min_wait = 1; // NOLINT(build/unsigned)
  constexpr uint64_t delay_max_wait = 2; // NOLINT(build/unsigned)
  bool convert_to_ticks = true;

  setup(delay_ticks, delay_min_wait, delay_max_wait, timestamps, convert_to_ticks);

  // First pass  
  bool timeout = false;
  int processed_count = run(timeout);
  BOOST_REQUIRE_EQUAL(processed_count, 0);

  timeout = true;
  processed_count = run(timeout);
  // Buffer = {1, 2, 4} delay_ticks = 1
  // 1st timeout => timeout_accumulated = 1 * 2 (delay_max_wait = 2)
  // 4 - 4 + 2 > 1 => postprocess until end {1, 2, 4} next_window_start_ts = 4 + 1 = 5
  BOOST_REQUIRE_EQUAL(processed_count, 3);

  add_frame(3);
  // Buffer = {1, 2, 3, 4}

  // To not trigger the "too fast" case
  std::this_thread::sleep_for(std::chrono::milliseconds(delay_min_wait + 1));

  timeout = false;
  // next_window_start_ts > newest_ts (5 > 4 postprocessing window is already closed)
  processed_count += run(timeout);
  BOOST_REQUIRE_EQUAL(processed_count, 3);
}

BOOST_AUTO_TEST_SUITE_END() // PostprocessScheduleAlgorithmFixture
