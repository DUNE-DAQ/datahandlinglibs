/**
 * @file datahandlinglibs_DefaultRequestHandlerModel_test.cxx Unit Tests for DefaultRequestHandlerModel
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#define BOOST_TEST_MODULE datahandlinglibs_DefaultRequestHandlerModel_test // NOLINT

#include "boost/test/unit_test.hpp"
#include "datahandlinglibs/models/DefaultRequestHandlerModel.hpp"
#include "datahandlinglibs/models/SkipListLatencyBufferModel.hpp"
#include "datahandlinglibs/opmon/datahandling_info.pb.h"
#include "datahandlinglibs/testutils/UnitTestUtilities.hpp"

#include "dfmessages/Types.hpp"

#include "dfmessages/DataRequest.hpp"
#include <daqdataformats/FragmentHeader.hpp>
#include <string>
#include <thread>

using namespace dunedaq::datahandlinglibs;

BOOST_AUTO_TEST_SUITE(datahandlinglibs_DefaultRequestHandlerModel_test)

dunedaq::dfmessages::DataRequest
create_request(int req_number, int difference)
{
  dunedaq::dfmessages::DataRequest req;

  req.request_number = req_number;                               // Unique number for this request
  req.trigger_number = req_number;                               // Trigger ID that caused this
  req.run_number = req_number;                                   // The current run being taken
  req.trigger_timestamp = req_number;                            // When the event occurred
  req.readout_type = dunedaq::dfmessages::ReadoutType::kInvalid; // Readout mode
  req.sequence_number = req_number;                              // Sequence within this run
  req.data_destination = "somewhere";                            // Where to send the result

  req.request_information.window_begin = req_number;
  req.request_information.window_end = req_number + difference;
  return req;
}

BOOST_AUTO_TEST_CASE(DefaultRequestHandlerModel_timestamp_virtuals)
{
  auto latency_buffer = std::make_shared<unittest::MockLatencyBufferType<unittest::MockReadoutType>>();
  auto error_registry = std::make_unique<dunedaq::datahandlinglibs::FrameErrorRegistry>();

  DefaultRequestHandlerModel<unittest::MockReadoutType, unittest::MockLatencyBufferType<unittest::MockReadoutType>>
    req_handler(latency_buffer, error_registry);

  BOOST_REQUIRE_EQUAL(req_handler.get_cutoff_timestamp(), 0);
  BOOST_REQUIRE(!req_handler.supports_cutoff_timestamp());
}

// create_fragment_header,issue_request, data_request
BOOST_AUTO_TEST_CASE(DefaultRequestHandlerModel_issue_request_empty_buffer)
{
  dunedaq::dfmessages::DataRequest req_1 = create_request(1, 1);

  auto latency_buffer = std::make_shared<unittest::MockLatencyBufferType<unittest::MockReadoutType>>();
  auto error_registry = std::make_unique<dunedaq::datahandlinglibs::FrameErrorRegistry>();

  unittest::MockRequestHandlerType<unittest::MockReadoutType,
                                   unittest::MockLatencyBufferType<unittest::MockReadoutType>>
    testhandler(latency_buffer, error_registry);
  testhandler.test_start();

  testhandler.issue_request(req_1, false);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  BOOST_REQUIRE_EQUAL(testhandler.get_handled_requests(), 0);
  BOOST_REQUIRE(testhandler.get_response_time_acc() == 0);
  BOOST_REQUIRE_EQUAL(testhandler.get_response_time_min(), std::numeric_limits<int>::max());
  BOOST_REQUIRE_EQUAL(testhandler.get_response_time_max(), 0);
}

BOOST_AUTO_TEST_CASE(DefaultRequestHandlerModel_issue_request_kPartiallyOld)
{
  auto latency_buffer = std::make_shared<unittest::MockLatencyBufferType<unittest::MockReadoutType>>();
  auto error_registry = std::make_unique<dunedaq::datahandlinglibs::FrameErrorRegistry>();
  unittest::MockRequestHandlerType<unittest::MockReadoutType,
                                   unittest::MockLatencyBufferType<unittest::MockReadoutType>>
    testhandler(latency_buffer, error_registry);

  testhandler.test_start();
  unittest::MockReadoutType elem;
  elem.set_timestamp(2);
  latency_buffer->write(std::move(elem));
  testhandler.issue_request(create_request(1, 1));

  auto start = std::chrono::steady_clock::now();
  while (testhandler.get_handled_requests() < 1) {
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    if (std::chrono::steady_clock::now() - start > std::chrono::seconds(6)) {
      BOOST_FAIL("Timeout: handler never processed the request");
    }
  }
  BOOST_REQUIRE_EQUAL(testhandler.get_handled_requests(), 1);
  BOOST_REQUIRE(testhandler.get_response_time_acc() > 0);
  BOOST_REQUIRE_EQUAL(testhandler.get_response_time_acc(), testhandler.get_response_time_min());
  BOOST_REQUIRE_EQUAL(testhandler.get_response_time_acc(), testhandler.get_response_time_max());
}

BOOST_AUTO_TEST_CASE(DefaultRequestHandlerModel_issue_request_kFound)
{
  auto latency_buffer = std::make_shared<SkipListLatencyBufferModel<unittest::MockReadoutType>>();
  auto error_registry = std::make_unique<dunedaq::datahandlinglibs::FrameErrorRegistry>();
  unittest::MockRequestHandlerType<unittest::MockReadoutType, SkipListLatencyBufferModel<unittest::MockReadoutType>>
    testhandler(latency_buffer, error_registry);

  testhandler.test_start();
  for (int i = 1; i < 7; i++) {
    unittest::MockReadoutType elem;
    elem.set_timestamp(i);
    latency_buffer->write(std::move(elem));
  }

  BOOST_REQUIRE_EQUAL(testhandler.get_num_requests_found(), 0);

  testhandler.issue_request(create_request(3, 2));

  auto start = std::chrono::steady_clock::now();
  while (testhandler.get_handled_requests() < 1) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if (std::chrono::steady_clock::now() - start > std::chrono::seconds(6)) {
      BOOST_FAIL("Timeout: handler never processed the request");
    }
  }
  BOOST_REQUIRE_EQUAL(testhandler.get_handled_requests(), 1);
  BOOST_REQUIRE(testhandler.get_response_time_acc() > 0);
  BOOST_REQUIRE_EQUAL(testhandler.get_response_time_acc(), testhandler.get_response_time_min());
  BOOST_REQUIRE_EQUAL(testhandler.get_response_time_acc(), testhandler.get_response_time_max());
  BOOST_REQUIRE_EQUAL(testhandler.get_num_requests_found(), 1);
}

BOOST_AUTO_TEST_CASE(DefaultRequestHandlerModel_issue_request_kNotFound)
{
  auto latency_buffer = std::make_shared<SkipListLatencyBufferModel<unittest::MockReadoutType>>();
  auto error_registry = std::make_unique<dunedaq::datahandlinglibs::FrameErrorRegistry>();
  unittest::MockRequestHandlerType<unittest::MockReadoutType, SkipListLatencyBufferModel<unittest::MockReadoutType>>
    testhandler(latency_buffer, error_registry);

  testhandler.test_start();
  BOOST_REQUIRE_EQUAL(testhandler.get_num_requests_bad(), 0);

  testhandler.issue_request(create_request(3, 2));

  auto start = std::chrono::steady_clock::now();
  while (testhandler.get_handled_requests() < 1) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if (std::chrono::steady_clock::now() - start > std::chrono::seconds(6)) {
      BOOST_FAIL("Timeout: handler never processed the request");
    }
  }
  BOOST_REQUIRE_EQUAL(testhandler.get_handled_requests(), 1);
  BOOST_REQUIRE(testhandler.get_response_time_acc() > 0);
  BOOST_REQUIRE_EQUAL(testhandler.get_response_time_acc(), testhandler.get_response_time_min());
  BOOST_REQUIRE_EQUAL(testhandler.get_response_time_acc(), testhandler.get_response_time_max());
  BOOST_REQUIRE_EQUAL(testhandler.get_num_requests_bad(), 1);
}

BOOST_AUTO_TEST_CASE(DefaultRequestHandlerModel_issue_request_kNotYet)
{
  auto latency_buffer = std::make_shared<SkipListLatencyBufferModel<unittest::MockReadoutType>>();
  auto error_registry = std::make_unique<dunedaq::datahandlinglibs::FrameErrorRegistry>();
  unittest::MockRequestHandlerType<unittest::MockReadoutType, SkipListLatencyBufferModel<unittest::MockReadoutType>>
    testhandler(latency_buffer, error_registry);

  testhandler.test_start();
  BOOST_REQUIRE_EQUAL(testhandler.get_num_requests_delayed(), 0);
  for (int i = 1; i < 7; i++) {
    unittest::MockReadoutType elem;
    elem.set_timestamp(i);
    latency_buffer->write(std::move(elem));
  }

  testhandler.issue_request(create_request(8, 1));

  auto start = std::chrono::steady_clock::now();
  while (testhandler.get_handled_requests() < 1) {
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    if (std::chrono::steady_clock::now() - start > std::chrono::seconds(6)) {
      BOOST_FAIL("Timeout: handler never processed the request");
    }
  }
  BOOST_REQUIRE_EQUAL(testhandler.get_handled_requests(), 1);
  BOOST_REQUIRE_EQUAL(testhandler.get_num_requests_delayed(), 1);
  BOOST_REQUIRE_EQUAL(testhandler.get_waiting_requests(), 1);
}

BOOST_AUTO_TEST_CASE(DefaultRequestHandlerModel_issue_request_kTooOld)
{
  auto latency_buffer = std::make_shared<SkipListLatencyBufferModel<unittest::MockReadoutType>>();
  auto error_registry = std::make_unique<dunedaq::datahandlinglibs::FrameErrorRegistry>();
  unittest::MockRequestHandlerType<unittest::MockReadoutType, SkipListLatencyBufferModel<unittest::MockReadoutType>>
    testhandler(latency_buffer, error_registry);

  testhandler.test_start();
  for (int i = 3; i < 7; i++) {
    unittest::MockReadoutType elem;
    elem.set_timestamp(i);
    latency_buffer->write(std::move(elem));
  }

  BOOST_REQUIRE_EQUAL(testhandler.get_num_requests_old_window(), 0);
  BOOST_REQUIRE_EQUAL(testhandler.get_num_requests_bad(), 0);
  testhandler.issue_request(create_request(1, 1));

  auto start = std::chrono::steady_clock::now();
  while (testhandler.get_handled_requests() < 1) {
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    if (std::chrono::steady_clock::now() - start > std::chrono::seconds(6)) {
      BOOST_FAIL("Timeout: handler never processed the request");
    }
  }
  BOOST_REQUIRE_EQUAL(testhandler.get_handled_requests(), 1);
  BOOST_REQUIRE_EQUAL(testhandler.get_num_requests_old_window(), 1);
  BOOST_REQUIRE_EQUAL(testhandler.get_num_requests_bad(), 1);
}

BOOST_AUTO_TEST_CASE(DefaultRequestHandlerModel_issue_request_kPartial)
{
  auto latency_buffer = std::make_shared<SkipListLatencyBufferModel<unittest::MockReadoutType>>();
  auto error_registry = std::make_unique<dunedaq::datahandlinglibs::FrameErrorRegistry>();
  unittest::MockRequestHandlerType<unittest::MockReadoutType, SkipListLatencyBufferModel<unittest::MockReadoutType>>
    testhandler(latency_buffer, error_registry);

  testhandler.test_start();
  for (int i = 1; i < 7; i++) {
    unittest::MockReadoutType elem;
    elem.set_timestamp(i);
    latency_buffer->write(std::move(elem));
  }

  BOOST_REQUIRE_EQUAL(testhandler.get_num_requests_delayed(), 0);
  testhandler.issue_request(create_request(6, 1));

  auto start = std::chrono::steady_clock::now();
  while (testhandler.get_handled_requests() < 1) {
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    if (std::chrono::steady_clock::now() - start > std::chrono::seconds(6)) {
      BOOST_FAIL("Timeout: handler never processed the request");
    }
  }
  BOOST_REQUIRE_EQUAL(testhandler.get_handled_requests(), 1);
  BOOST_REQUIRE_EQUAL(testhandler.get_num_requests_delayed(), 1);
}

BOOST_AUTO_TEST_CASE(DefaultRequestHandlerModel_cleanups)
{
  auto latency_buffer = std::make_shared<SkipListLatencyBufferModel<unittest::MockReadoutType>>();
  auto error_registry = std::make_unique<dunedaq::datahandlinglibs::FrameErrorRegistry>();
  unittest::MockRequestHandlerType<unittest::MockReadoutType, SkipListLatencyBufferModel<unittest::MockReadoutType>>
    testhandler(latency_buffer, error_registry);

  testhandler.set_run_marker(true);
  testhandler.test_start();

  for (int i = 1; i < 7; i++) {
    unittest::MockReadoutType elem;
    elem.set_timestamp(i);
    latency_buffer->write(std::move(elem));
  }
  std::thread cleanup_thread([&]() { testhandler.test_periodic_cleanups(); });

  std::this_thread::sleep_for(std::chrono::seconds(2));
  testhandler.set_run_marker(false);

  cleanup_thread.join();

  BOOST_REQUIRE_EQUAL(testhandler.get_num_buffer_cleanups(), 1);
  BOOST_REQUIRE_EQUAL(testhandler.get_occupancy(), 3);
  BOOST_REQUIRE_EQUAL(testhandler.get_pops_count(), 3);
}

BOOST_AUTO_TEST_CASE(DefaultRequestHandlerModel_check_waiting_requests_window_end)
{
  auto latency_buffer = std::make_shared<SkipListLatencyBufferModel<unittest::MockReadoutType>>();
  auto error_registry = std::make_unique<dunedaq::datahandlinglibs::FrameErrorRegistry>();
  unittest::MockRequestHandlerType<unittest::MockReadoutType, SkipListLatencyBufferModel<unittest::MockReadoutType>>
    testhandler(latency_buffer, error_registry);

  testhandler.set_run_marker(true);
  testhandler.test_start();

  for (int i = 1; i < 7; i++) {
    unittest::MockReadoutType elem;
    elem.set_timestamp(i);
    latency_buffer->write(std::move(elem));
  }

  BOOST_REQUIRE_EQUAL(testhandler.get_waiting_requests(), 0);

  testhandler.issue_request(create_request(6, 1));

  auto start = std::chrono::steady_clock::now();
  while (testhandler.get_handled_requests() < 1) {
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    if (std::chrono::steady_clock::now() - start > std::chrono::seconds(6)) {
      BOOST_FAIL("Timeout: handler never processed the request");
    }
  }
  BOOST_REQUIRE_EQUAL(testhandler.get_waiting_requests(), 1);

  for (int i = 7; i < 9; i++) {
    unittest::MockReadoutType elem;
    elem.set_timestamp(i);
    latency_buffer->write(std::move(elem));
  }

  std::thread t([&]() { testhandler.test_check_waiting_requests(); });
  std::this_thread::sleep_for(std::chrono::seconds(1));
  testhandler.set_run_marker(false);
  t.join();

  BOOST_REQUIRE_EQUAL(testhandler.get_waiting_requests(), 0);
  BOOST_REQUIRE_EQUAL(testhandler.get_handled_requests(),
                      2); // first one is issue_request, second one is check_waiting_requests
}

BOOST_AUTO_TEST_CASE(DefaultRequestHandlerModel_dump_to_buffer)
{
  auto latency_buffer = std::make_shared<SkipListLatencyBufferModel<unittest::MockReadoutType>>();
  auto error_registry = std::make_unique<dunedaq::datahandlinglibs::FrameErrorRegistry>();
  unittest::MockRequestHandlerType<unittest::MockReadoutType, SkipListLatencyBufferModel<unittest::MockReadoutType>>
    testhandler(latency_buffer, error_registry);

  std::string data = "data to copy";
  std::string buffer = "    ";

  testhandler.test_dump_to_buffer(data.data(), 4, buffer.data(), 0, buffer.size());
  BOOST_REQUIRE_EQUAL(buffer, "data");
}

BOOST_AUTO_TEST_CASE(DefaultRequestHandlerModel_create_empty_fragment)
{
  auto latency_buffer = std::make_shared<SkipListLatencyBufferModel<unittest::MockReadoutType>>();
  auto error_registry = std::make_unique<dunedaq::datahandlinglibs::FrameErrorRegistry>();
  unittest::MockRequestHandlerType<unittest::MockReadoutType, SkipListLatencyBufferModel<unittest::MockReadoutType>>
    testhandler(latency_buffer, error_registry);

  auto fragment = testhandler.test_create_empty_fragment(create_request(1, 1));
  BOOST_REQUIRE_EQUAL(fragment->get_header().status_bits, 1u << static_cast<size_t>(dunedaq::daqdataformats::FragmentStatusBits::kEmptyFragment));
}

BOOST_AUTO_TEST_SUITE_END()
