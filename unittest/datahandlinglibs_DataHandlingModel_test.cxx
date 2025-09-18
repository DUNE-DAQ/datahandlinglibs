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

using namespace dunedaq::datahandlinglibs;

BOOST_AUTO_TEST_SUITE(datahandlinglibs_DataHandlingModel_test)

using RDT = unittest::FakeReadoutType;
using RHT = unittest::FakeRequestHandlerType<unittest::FakeReadoutType,
                                             unittest::FakeLatencyBufferType<unittest::FakeReadoutType>>;
using LBT = unittest::FakeLatencyBufferType<unittest::FakeReadoutType>;
using RPT = unittest::FakeRawDataProcessorType;



BOOST_AUTO_TEST_CASE(DataHandlingModel_process_item_pre_post_process)
{
  std::atomic<bool> run_marker = true;

  auto model = unittest::FakeDataHandlingModel<RDT, RHT, LBT, RPT, RDT>(run_marker);

  bool pre_func_one_called = false;
  bool post_func_one_called = false;
  model.initialize(true);
  model.raw_processor(pre_func_one_called, post_func_one_called, 0);

  RDT elem;
  elem.set_timestamp(2);

  model.public_process_item(std::move(elem));
  BOOST_REQUIRE(pre_func_one_called);
  BOOST_REQUIRE_EQUAL(model.get_m_num_payloads(), 1);
  BOOST_REQUIRE_EQUAL(model.get_m_sum_payloads(), 1);
  BOOST_REQUIRE_EQUAL(model.get_m_stats_packet_count(), 1);
}

BOOST_AUTO_TEST_CASE(DataHandlingModel_process_item_cutoff_triggered)
{
  std::atomic<bool> run_marker = true;

  auto model = unittest::FakeDataHandlingModel<RDT, RHT, LBT, RPT, RDT>(run_marker);

  bool pre_func_one_called = false;
  bool post_func_one_called = false;
  model.initialize(true);
  model.raw_processor(pre_func_one_called, post_func_one_called, 10);

  RDT elem;
  elem.set_timestamp(2);

  setenv("DUNEDAQ_ERS_WARNING", "throw", 1);
  BOOST_CHECK_THROW(model.public_process_item(std::move(elem)), std::exception);
}

BOOST_AUTO_TEST_CASE(DataHandlingModel_transform_payload)
{
  std::atomic<bool> run_marker = true;

  auto model = unittest::FakeDataHandlingModel<RDT, RHT, LBT, RPT, unittest::FakeSuperChunkReadoutType>(run_marker);

  unittest::FakeSuperChunkReadoutType input;
  input.set_timestamp(4);
  auto output = model.public_transform_payload(input);
  BOOST_REQUIRE((std::is_same_v<unittest::FakeReadoutType, decltype(output)::value_type>));
}

BOOST_AUTO_TEST_CASE(DataHandlingModel_transform_and_process_same_type)
{
  std::atomic<bool> run_marker = true;

  auto model = unittest::FakeDataHandlingModel<RDT, RHT, LBT, RPT, RDT>(run_marker);
  model.initialize(true);

  bool pre_func_one_called = false;
  bool post_func_one_called = false;
  model.raw_processor(pre_func_one_called, post_func_one_called, 0);

  RDT input;
  input.set_timestamp(4);
  model.public_transform_and_process(std::move(input));
  BOOST_REQUIRE(pre_func_one_called);
  BOOST_REQUIRE_EQUAL(model.get_m_num_payloads(), 1);
}

BOOST_AUTO_TEST_CASE(DataHandlingModel_transform_and_process_different_type)
{
  std::atomic<bool> run_marker = true;

  auto model = unittest::FakeDataHandlingModel<RDT, RHT, LBT, RPT, unittest::FakeSuperChunkReadoutType>(run_marker);
  model.initialize(true);

  bool pre_func_one_called = false;
  bool post_func_one_called = false;
  model.raw_processor(pre_func_one_called, post_func_one_called, 0);

  unittest::FakeSuperChunkReadoutType input;
  input.set_timestamp(4);
  model.public_transform_and_process(std::move(input));
  BOOST_REQUIRE(pre_func_one_called);
  BOOST_REQUIRE_EQUAL(model.get_m_num_payloads(), 1);
}

BOOST_AUTO_TEST_CASE(DataHandlingModel_consume_callback)
{
  std::atomic<bool> run_marker = true;

  auto model = unittest::FakeDataHandlingModel<RDT, RHT, LBT, RPT, unittest::FakeSuperChunkReadoutType>(run_marker);
  model.initialize(true);

  bool pre_func_one_called = false;
  bool post_func_one_called = false;
  model.raw_processor(pre_func_one_called, post_func_one_called, 0);

  unittest::FakeSuperChunkReadoutType input;
  input.set_timestamp(4);
  model.public_consume_callback(std::move(input));
  BOOST_REQUIRE(pre_func_one_called);
  BOOST_REQUIRE_EQUAL(model.get_m_num_payloads(), 1);
}

BOOST_AUTO_TEST_CASE(DataHandlingModel_consume_callback_write_failure)
{
  std::atomic<bool> run_marker = true;

  auto model = unittest::FakeDataHandlingModel<RDT,
                                   unittest::FakeRequestHandlerType<RDT, BinarySearchQueueModel<RDT>>,
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
    model.public_consume_callback(std::move(input));
  }

  BOOST_REQUIRE(pre_func_one_called);
  BOOST_REQUIRE_EQUAL(model.get_m_num_lb_insert_failures(), 1);
}

BOOST_AUTO_TEST_CASE(DataHandlingModel_postprocess_schedule_SkipListLatencyBufferModel)
{
  std::atomic<bool> run_marker = true;

  auto model = unittest::FakeDataHandlingModel<
    RDT,
    unittest::FakeRequestHandlerType<unittest::FakeReadoutType, SkipListLatencyBufferModel<unittest::FakeReadoutType>>,
    SkipListLatencyBufferModel<unittest::FakeReadoutType>,
    RPT,
    RDT>(run_marker);
  model.initialize(true);

  bool pre_func_one_called = false;
  bool post_func_one_called = false;
  model.post_schedule_init(pre_func_one_called, post_func_one_called, 0, 0);

  for (int i = 100; i < 105; i++) {
    RDT input;
    input.set_timestamp(i);
    model.public_transform_and_process(std::move(input));
  }
  // std::this_thread::sleep_for(std::chrono::milliseconds(50));
  std::thread postprocess([&]() { model.public_run_postprocess_scheduler(); });
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  for (int i = 105; i < 112; i++) {
    RDT input;
    input.set_timestamp(i);
    model.public_transform_and_process(std::move(input));
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  model.change_m_run_marker(false);
  {
    RDT input;
    input.set_timestamp(112);
    model.public_transform_and_process(std::move(input));
  }
  postprocess.join();

  BOOST_REQUIRE_EQUAL(model.get_m_num_post_processing_delay_max_waits(), 0);
  BOOST_REQUIRE_EQUAL(model.get_m_num_payloads(), 8); // newest ts - delay ticks
}

BOOST_AUTO_TEST_CASE(DataHandlingModel_postprocess_schedule_BinarySearchQueueModel)
{
  std::atomic<bool> run_marker = true;

  auto model = unittest::FakeDataHandlingModel<RDT,
                                   unittest::FakeRequestHandlerType<RDT, BinarySearchQueueModel<RDT>>,
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
    model.public_transform_and_process(std::move(input));
  }
  // std::this_thread::sleep_for(std::chrono::milliseconds(50));
  std::thread postprocess([&]() { model.public_run_postprocess_scheduler(); });
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  for (int i = 105; i < 112; i++) {
    RDT input;
    input.set_timestamp(i);
    model.public_transform_and_process(std::move(input));
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  model.change_m_run_marker(false);
  {
    RDT input;
    input.set_timestamp(112);
    model.public_transform_and_process(std::move(input));
  }
  postprocess.join();

  BOOST_REQUIRE_EQUAL(model.get_m_num_post_processing_delay_max_waits(), 0);
  BOOST_REQUIRE_EQUAL(model.get_m_num_payloads(), 8); // newest ts - delay ticks
}

BOOST_AUTO_TEST_CASE(DataHandlingModel_postprocess_schedule_FixedRateQueueModel)
{
  std::atomic<bool> run_marker = true;

  auto model = unittest::FakeDataHandlingModel<RDT,
                                   unittest::FakeRequestHandlerType<RDT, FixedRateQueueModel<RDT>>,
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
    model.public_transform_and_process(std::move(input));
  }
  // std::this_thread::sleep_for(std::chrono::milliseconds(50));
  std::thread postprocess([&]() { model.public_run_postprocess_scheduler(); });
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  for (int i = 105; i < 112; i++) {
    RDT input;
    input.set_timestamp(i);
    model.public_transform_and_process(std::move(input));
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  model.change_m_run_marker(false);
  {
    RDT input;
    input.set_timestamp(112);
    model.public_transform_and_process(std::move(input));
  }
  postprocess.join();

  BOOST_REQUIRE_EQUAL(model.get_m_num_post_processing_delay_max_waits(), 0);
  BOOST_REQUIRE_EQUAL(model.get_m_num_payloads(), 8); // newest ts - delay ticks
}

BOOST_AUTO_TEST_SUITE_END()