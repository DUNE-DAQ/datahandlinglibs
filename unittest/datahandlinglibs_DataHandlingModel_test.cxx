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

  static constexpr bool post_processing_enabled = true;

  std::shared_ptr<LatencyBufferType> buffer;
  std::unique_ptr<FrameErrorRegistry> error_registry;
  std::shared_ptr<RawDataProcessorType> processor;

protected:
  DataHandlingBaseFixture()
  {
    buffer = std::make_shared<LatencyBufferType>();
    error_registry = std::make_unique<FrameErrorRegistry>();
    processor = std::make_shared<RawDataProcessorType>(error_registry, post_processing_enabled);
  }

public:
  void setup(const std::vector<uint64_t>& timestamps = {}) // NOLINT(build/unsigned)
  {
    buffer->flush();
    add_frames(timestamps);
  }

  void add_frame(uint64_t timestamp) // NOLINT(build/unsigned)
  {
    ReadoutType frame{};
    frame.timestamp = timestamp;
    buffer->write(std::move(frame));
  }

  void add_frames(const std::vector<uint64_t>& timestamps) // NOLINT(build/unsigned)
  {
    for (uint64_t ts : timestamps) { // NOLINT(build/unsigned)
      add_frame(ts);
    }
  }
};

template<class RequestHandlerType, class LatencyBufferType>
struct DataHandlingFixture : DataHandlingBaseFixture<RequestHandlerType, LatencyBufferType>
{
  using Base = DataHandlingBaseFixture<RequestHandlerType, LatencyBufferType>;
  using RawDataProcessorType = typename Base::RawDataProcessorType;
  using ModelType = typename Base::ModelType;

  std::atomic<bool> run_marker = true;
  std::unique_ptr<ModelType> model;

  DataHandlingFixture() { model = std::make_unique<ModelType>(run_marker); }

  void set_run_marker(bool run_marker) { model->set_run_marker(run_marker); }

  void test_run_postprocess_scheduler(std::shared_ptr<LatencyBufferType> latency_buffer_impl,
                                      std::shared_ptr<RawDataProcessorType> raw_processor_impl,
                                      std::unique_ptr<folly::Timekeeper> timekeeper,
                                      uint64_t post_processing_delay_max_wait) // NOLINT(build/unsigned)
  {
    model->test_run_postprocess_scheduler(
      latency_buffer_impl, raw_processor_impl, std::move(timekeeper), post_processing_delay_max_wait);
  }

  ModelType::num_post_processing_delay_max_waits_t get_num_post_processing_delay_max_waits()
  {
    return model->get_num_post_processing_delay_max_waits();
  }
};

struct PostprocessScheduleAlgorithmFixture : DataHandlingBaseFixture<DefaultRequestHandler, SkipListLatencyBuffer>
{
  using LatencyBufferType = SkipListLatencyBuffer;
  using RequestHandlerType = DefaultRequestHandlerModel<ReadoutType, LatencyBufferType>;
  using Base = DataHandlingBaseFixture<RequestHandlerType, LatencyBufferType>;
  using AlgorithmType = typename Base::ModelType::PostprocessScheduleAlgorithm;

  static constexpr uint64_t ms_to_ticks = 62500; // NOLINT(build/unsigned)

  std::unique_ptr<AlgorithmType> algorithm;

  void setup(uint64_t delay_ticks,                         // NOLINT(build/unsigned)
             uint64_t delay_min_wait,                      // NOLINT(build/unsigned)
             uint64_t delay_max_wait,                      // NOLINT(build/unsigned)
             const std::vector<uint64_t>& timestamps = {}) // NOLINT(build/unsigned)
  {
    Base::setup(timestamps);
    algorithm = std::make_unique<AlgorithmType>(*buffer, *processor, delay_ticks, delay_min_wait, delay_max_wait);
  }

  int run(bool timeout)
  {
    if (!algorithm) {
      return 0;
    }
    return algorithm->run(timeout);
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
    [&]() { test_run_postprocess_scheduler(buffer, processor, std::move(timekeeper), delay_max_wait); });

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

  BOOST_REQUIRE_EQUAL(get_num_post_processing_delay_max_waits(), 1);
}

BOOST_AUTO_TEST_SUITE_END() // DataHandlingFixture

BOOST_FIXTURE_TEST_SUITE(datahandlinglibs_DataHandlingModelPostprocessScheduleAlgorithm_test,
                         PostprocessScheduleAlgorithmFixture)

BOOST_AUTO_TEST_CASE(datahandlinglibs_DataHandlingModelPostprocessScheduleAlgorithm_timeout)
{
  std::vector<uint64_t> timestamps = {
    1 * ms_to_ticks, 2 * ms_to_ticks, 3 * ms_to_ticks, 4 * ms_to_ticks, 5 * ms_to_ticks
  }; // NOLINT(build/unsigned)
  constexpr uint64_t delay_ticks = 4 * ms_to_ticks; // NOLINT(build/unsigned)
  constexpr uint64_t delay_min_wait = 1;            // NOLINT(build/unsigned)
  constexpr uint64_t delay_max_wait = 2;            // NOLINT(build/unsigned)

  setup(delay_ticks, delay_min_wait, delay_max_wait, timestamps);

  // First pass
  bool timeout = false;
  int processed_count = run(timeout);
  // Buffer = {1, 2, 3, 4, 5} delay_ticks = 4
  // 5 - 1 > 4 is false => no postprocessing
  BOOST_REQUIRE_EQUAL(processed_count, 0);

  timeout = true;
  // 1st timeout => timeout_accumulated = 1 * 2 (delay_max_wait = 2)
  // end_win_ts = 5 - 4 + 2 => postprocess until 3 {1, 2}
  processed_count += run(timeout);
  BOOST_REQUIRE_EQUAL(processed_count, 2);

  // 2nd timeout => timeout_accumulated = 2 * 2
  // end_win_ts = 5 - 4 + 4 => postprocess until 5 {3, 4}
  processed_count += run(timeout);
  BOOST_REQUIRE_EQUAL(processed_count, 4);

  // 3rd timeout => timeout_accumulated = 3 * 2
  // end_win_ts = 5 - 4 + 6 => postprocess until 6 (capped to newest_ts + 1) {5}
  processed_count += run(timeout);
  BOOST_REQUIRE_EQUAL(processed_count, 5);

  // 4th timeout
  // m_processed_up_to.timestamp = newest_ts + 1 => nothing to postprocess (at cap)
  processed_count += run(timeout);
  BOOST_REQUIRE_EQUAL(processed_count, 5);
}

BOOST_AUTO_TEST_CASE(
  datahandlinglibs_DataHandlingModelPostprocessScheduleAlgorithm_data_arrives_after_fully_processed_with_timeout)
{
  std::vector<uint64_t> timestamps = { 1 * ms_to_ticks, 2 * ms_to_ticks, 4 * ms_to_ticks }; // NOLINT(build/unsigned)
  constexpr uint64_t delay_ticks = 1 * ms_to_ticks;                                         // NOLINT(build/unsigned)
  constexpr uint64_t delay_min_wait = 1;                                                    // NOLINT(build/unsigned)
  constexpr uint64_t delay_max_wait = 2;                                                    // NOLINT(build/unsigned)

  setup(delay_ticks, delay_min_wait, delay_max_wait, timestamps);

  bool timeout = true;
  int processed_count = run(timeout);
  // Buffer = {1, 2, 4} delay_ticks = 1
  // 1st timeout => timeout_accumulated = 1 * 2 (delay_max_wait = 2)
  // end_win_ts = 4 - 1 + 2 => postprocess until 5 {1, 2, 4}
  BOOST_REQUIRE_EQUAL(processed_count, 3);

  add_frame(3 * ms_to_ticks);
  // Buffer = {1, 2, 3, 4}

  // To not trigger the "too fast" case
  std::this_thread::sleep_for(std::chrono::milliseconds(delay_min_wait + 1));

  timeout = false;
  // m_processed_up_to.timestamp = newest_ts + 1 => nothing to postprocess (data arrived too late)
  processed_count += run(timeout);
  BOOST_REQUIRE_EQUAL(processed_count, 3);
}

BOOST_AUTO_TEST_SUITE_END() // PostprocessScheduleAlgorithmFixture
